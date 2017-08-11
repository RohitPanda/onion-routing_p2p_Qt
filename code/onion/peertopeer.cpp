#include "peertopeer.h"

PeerToPeer::PeerToPeer(QObject *parent) : QObject(parent), socket_(this)
{
    connect(&socket_, &QUdpSocket::readyRead, this, &PeerToPeer::onDatagram);
}

QHostAddress PeerToPeer::interface() const
{
    return interface_;
}

void PeerToPeer::setInterface(const QHostAddress &interface)
{
    interface_ = interface;
}

int PeerToPeer::port() const
{
    return port_;
}

void PeerToPeer::setPort(int port)
{
    port_ = port;
}

bool PeerToPeer::start()
{
    return socket_.bind(port_);
}

void PeerToPeer::onDatagram()
{
    while (socket_.hasPendingDatagrams()) {
        handleDatagram(socket_.receiveDatagram());
    }
}

void PeerToPeer::handleDatagram(QNetworkDatagram datagram)
{
    if(!datagram.isValid()) {
        qDebug() << "error receiving p2p datagram.";
        return;
    }

    QByteArray data = datagram.data();
    if(data.size() != MESSAGE_LENGTH) {
        qDebug() << "P2P data with invalid length" << data.size() << "should be" << MESSAGE_LENGTH;
        // TODO close connection to peer
    }

    Binding peer(datagram.senderAddress(), datagram.senderPort());

    // peek a P2PMessage
    QDataStream stream(data);
    PeerToPeerMessage message;
    peekMessage(stream, message);
    qDebug() << "P2P data from" << peer.toString() << message.typeString();

    if(message.malformed) {
        qDebug() << "P2P malformed message from" << peer.toString() << message.typeString()
                 << "; closing connection.";
        // TODO close connection to peer
    }

    if(message.celltype == PeerToPeerMessage::BUILD) {
        handleBuild(datagram, message);
        return;
    }

    if(message.celltype == PeerToPeerMessage::CREATED) {
        handleCreated(datagram, message);
        return;
    }


    QByteArray preface = data.left(5);
    QByteArray encryptedPayload = data.mid(5); // TODO adapt these with design spec

    // we'll need auth at one point
    OnionAuthRequest storage;
    storage.peer = peer;
    storage.preface = preface;
    storage.tunnelId = message.tunnelId;


    // incoming encrypted message -> flowchart:
    //
    //                              YES                   1.
    // tunnelId in circuits_  --------------> incoming onioned answer;
    //         |                              layered decrypt
    //         | NO
    //         |
    //         v
    // tunneled message
    // => we should forward to
    //    next or previous peer                          2.
    // => find tunnel;                  NO    packet is an answer;
    //    src IP/port == previousHop ? -----> encrypt with our layer, forward to source
    //         |
    //         | YES
    //         |
    //         v
    // 3. packet is in transit to dest;
    //    decrypt our layer;             YES
    //    digest valid ?  ------------------> packet is for us
    //         |
    //         | NO
    //         |
    //         v
    //    forward to nextHop

    if(circuits_.contains(message.tunnelId)) {
        // layered decrypt
        CircuitState circuit = circuits_[message.tunnelId];

        storage.type = OnionAuthRequest::LayeredDecrypt;
        storage.remainingHops = circuit.hopStates;

        continueLayeredDecrypt(storage, encryptedPayload);
        return;
    }


    // find tunnel of this message
    for(TunnelState &candidate : tunnels_) {
        if(candidate.tunnelIdPreviousHop == message.tunnelId) {
            // from prevHop -> towards dest
            // 3. decrypt once, then forward to nexthop
            storage.type = OnionAuthRequest::DecryptOnce;
            storage.nextHop = candidate.nextHop;
            storage.nextHopTunnelId = candidate.tunnelIdNextHop;

            // decrypt with K_src,us,
            // which is the key associated with tunnelIdPreviousHop
            if(!sessions_.has(candidate.tunnelIdPreviousHop) || !candidate.hasNextHop()) {
                qDebug() << "broken tunnel: no session with source or no nexthop for tunnel"
                         << candidate.previousHop.toString() << "<-> us <->"
                         << candidate.nextHop.toString();
                return;
            }

            quint16 session = sessions_.get(candidate.tunnelIdPreviousHop);
            quint32 reqId = nextAuthRequest_++;
            decryptQueue_[reqId] = storage;
            requestDecrypt(reqId, session, encryptedPayload);
            return;
        } else if(candidate.tunnelIdNextHop == message.tunnelId) {
            // from nextHop -> towards src
            // 2. encrypt once, then forward
            storage.type = OnionAuthRequest::EncryptOnce;
            storage.nextHop = candidate.previousHop;
            storage.nextHopTunnelId = candidate.tunnelIdPreviousHop;

            // encrypt with K_src,us,
            // which is associated with tunnelIdPreviousHop
            if(!sessions_.has(candidate.tunnelIdPreviousHop)) {
                qDebug() << "broken tunnel: no session with source for tunnel"
                         << candidate.previousHop.toString() << "<-> us <->"
                         << candidate.nextHop.toString();
                return;
            }

            quint16 session = sessions_.get(candidate.tunnelIdPreviousHop);
            quint32 reqId = nextAuthRequest_++;
            encryptQueue_[reqId] = storage;
            requestEncrypt(reqId, session, encryptedPayload);
            return;
        }
    }

    // tunnel not found
    qDebug() << "Discarding message with unknown tunnelId" << message.tunnelId;
    // TODO close connection to peer?
}

void PeerToPeer::handleBuild(const QNetworkDatagram &datagram, PeerToPeerMessage message)
{
    // incoming tunnel
    // send handshake to auth to build us a session
    quint32 reqId = nextAuthRequest_++;
    incomingTunnels_[reqId] = Binding(datagram.senderAddress(), datagram.senderPort());
    sessionIncomingHS1(reqId, message.data);
    // triggers onSessionHS2
}

void PeerToPeer::handleCreated(const QNetworkDatagram &datagram, PeerToPeerMessage message)
{
    // nexthop established
    // a) part of a circuit we made
    // b) part of an incomming tunnel, i.e. an earlier relay_extend
}

void PeerToPeer::handleMessage(PeerToPeerMessage message)
{
    // TODO check validity, then process
    if(message.malformed || message.celltype != PeerToPeerMessage::ENCRYPTED ||
            message.command == PeerToPeerMessage::CMD_INVALID || !message.isValidDigest()) {
        qDebug() << "invalid message in handleMessage" << message.typeString() << "digest ok? ->"
                 << message.isValidDigest();
    }

    qDebug() << "handleMessage ->" << message.typeString();

    switch (message.command) {
    case PeerToPeerMessage::RELAY_DATA:
        // get tunnelId
        // emit tunnelData
        break;
    case PeerToPeerMessage::RELAY_EXTEND:
        // compose handshake
        // save binding, setup tunnel/circuit ids
        // emit requestStartSession
        // -> continue in callback: send build, save binding, etc
        //                          await created
        //                          send relay_extended
        break;
    case PeerToPeerMessage::RELAY_EXTENDED:
        // get circuit that we extended?
        // emit sessionIncomingHS2
        // -> continue in callback: continue building tunnel or emit tunnelReady
        break;
    case PeerToPeerMessage::RELAY_TRUNCATED:
        // should only come from later hop to us when we started the circuit
        // tear circuit if not from start node
        // TODO -> need to know source node of this message => see layeredDecrypt
        break;
    case PeerToPeerMessage::CMD_DESTROY:
        // should only come from previous node
        // send CMD_DESTROY to nexthop
        // cleanup tunnel
        break;
    case PeerToPeerMessage::CMD_TRUNCATED:
        // to be removed
        break;
    case PeerToPeerMessage::CMD_COVER:
        // do nothing, content has been dropped anyways
        break;
    default:
        break;
    }
}

void PeerToPeer::continueLayeredDecrypt(PeerToPeer::OnionAuthRequest request, QByteArray payload)
{
    // get next session key
    // remove hop from list
    if(request.remainingHops.isEmpty()) {
        qDebug() << "continueLayeredDecrypt with empty hop list. Discarding request after"
                 << request.operations << "decrypts.";
        return;
    }

    HopState hop = request.remainingHops.takeFirst(); // decrypt is in order, take from front
    if(hop.status != Created) {
        qDebug() << "continueLayeredDecrypt with wrong hop state:" << hop.status
                 << "Discarding request after" << request.operations << "decrypts.";
        return;
    }

    request.operations++;

    // request decrypt
    quint32 reqId = nextAuthRequest_++;
    decryptQueue_[reqId] = request;
    requestDecrypt(reqId, hop.sessionKey, payload);
}

void PeerToPeer::continueLayeredEncrypt(PeerToPeer::OnionAuthRequest request, QByteArray payload)
{
    // get next session key
    // remove hop from list
    if(request.remainingHops.isEmpty()) {
        qDebug() << "continueLayeredEncrypt with empty hop list. Discarding request after"
                 << request.operations << "encrypts.";
        return;
    }

    HopState hop = request.remainingHops.takeLast(); // encrypte is in reverse order, take from back
    if(hop.status != Created) {
        qDebug() << "continueLayeredEncrypt with wrong hop state:" << hop.status
                 << "Discarding request after" << request.operations << "encrypts.";
        return;
    }

    request.operations++;

    // request decrypt
    quint32 reqId = nextAuthRequest_++;
    encryptQueue_[reqId] = request;
    requestEncrypt(reqId, hop.sessionKey, payload);
}

void PeerToPeer::forwardEncryptedMessage(Binding to, quint32 tunnelId, QByteArray payload)
{
    // compose blind message:
    QByteArray newMessage;
    QDataStream out(&newMessage, QIODevice::ReadWrite);
    out.setByteOrder(QDataStream::BigEndian);
    out << 0x03; // encrypted
    out << tunnelId; // new tunnelId
    out.writeRawData(payload.data(), payload.size());
    // no need for padding, payload should have size 1024

    if(newMessage.size() != MESSAGE_LENGTH) {
        qDebug() << "invariant violation. trying to send packet of size" << newMessage.size();
    }

    QNetworkDatagram packet(newMessage, to.address, to.port);
    packet.setSender(interface_, port_);
    qint64 ok = socket_.writeDatagram(packet);
    qDebug() << ok == -1 ? QString("FAIL") : QString("OK") << "sent packet to" << storage.nextHop.toString();
}

int PeerToPeer::nHops() const
{
    return nHops_;
}

void PeerToPeer::setNHops(int nHops)
{
    nHops_ = nHops;
}

void PeerToPeer::buildTunnel(QHostAddress destinationAddr, quint16 destinationPort, QTcpSocket *requestId)
{

}

void PeerToPeer::destroyTunnel(quint32 tunnelId)
{

}

bool PeerToPeer::sendData(quint32 tunnelId, QByteArray data)
{
    return true;
}

void PeerToPeer::coverTunnel(quint16 size)
{

}

void PeerToPeer::onEncrypted(quint32 requestId, quint16 sessionId, QByteArray payload)
{
    // we encrypted something => we're
    // a) in iterative encrypting of a request
    // b) encrypted a tunnel message we should forward

    OnionAuthRequest storage = encryptQueue_.take(requestId);

    if(storage.type == OnionAuthRequest::EncryptOnce) {
        // b)
        if(!storage.nextHop.isValid()) {
            qDebug() << "encryptOnce with invalid nextHop";
            return;
        }

        forwardEncryptedMessage(storage.nextHop, storage.nextHopTunnelId, payload);
        return;
    }

    if(storage.type == OnionAuthRequest::LayeredEncrypt) {
        // a)
        if(storage.remainingHops.isEmpty()) {
            // done encrypting, send to first hop in circuit
            forwardEncryptedMessage(storage.nextHop, storage.nextHopTunnelId, payload);
        }

        continueLayeredEncrypt(storage, payload);
        return;
    }

    qDebug() << "invalid request of type" << storage.type << "in onEncrypted";
}

void PeerToPeer::onDecrypted(quint32 requestId, QByteArray payload)
{
    // we decrypted something => check for valid digest, otherwise
    //  a) continue layered decrypt
    //  b) forward to nexthop

    OnionAuthRequest storage = decryptQueue_.take(requestId);
    // fake it into a message with the old preface, so we can parse it
    PeerToPeerMessage message;
    QByteArray fakeMessage;
    fakeMessage.append(storage.preface);
    fakeMessage.append(payload);
    QDataStream stream(fakeMessage);
    stream.setByteOrder(QDataStream::BigEndian);

    peekMessage(stream, message);

    if(message.isValidDigest()) {
        // for us, reparse full message
        QDataStream stream2(fakeMessage);
        stream2.setByteOrder(QDataStream::BigEndian);
        parseMessage(stream2, message);
        handleMessage(message);
        return;
    }

    // not valid yet
    if(storage.type == OnionAuthRequest::DecryptOnce) {
        // b)
        if(!storage.nextHop.isValid()) {
            qDebug() << "decryptOnce with invalid nextHop";
            return;
        }

        forwardEncryptedMessage(storage.nextHop, storage.nextHopTunnelId, payload);
        return;
    }

    if(storage.type == OnionAuthRequest::LayeredDecrypt) {
        // a)
        continueLayeredDecrypt(storage, payload);
        return;
    }

    qDebug() << "invalid request of type" << storage.type << "in onDecrypted";
}

void PeerToPeer::onSessionHS1(quint32 requestId, quint16 sessionId, QByteArray handshake)
{
    // TODO
}

void PeerToPeer::onSessionHS2(quint32 requestId, quint16 sessionId, QByteArray handshake)
{
    if(!incomingTunnels_.contains(requestId)) {
        qDebug() << "generated 2nd-half handshake does not match any connection";
        return;
    }

    Binding previousHop = incomingTunnels_.take(requestId);
    // setup tunnel on our side
    TunnelState newTunnel;
    newTunnel.previousHop = previousHop;
    newTunnel.tunnelIdPreviousHop = nextTunnelId_++;
    // setup session established with other side
    sessions_.set(newTunnel.tunnelIdPreviousHop, sessionId);

    // send back handshake in a CREATED message
    PeerToPeerMessage created;
    created.tunnelId =
        #error tunnelId <-> circid mapping
    created.celltype = PeerToPeerMessage::CREATED;
}
