#include "peertopeer.h"

#include <QTimer>

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
    bool ok = socket_.bind(port_);
    if(!ok) {
        qDebug() << "p2p api failed to bind" << socket_.error() << socket_.errorString();
    }
    return ok;
}

void PeerToPeer::onDatagram()
{
//    qDebug() << "onDatagram";
    while (socket_.hasPendingDatagrams()) {
        handleDatagram(socket_.receiveDatagram());
    }
}

void PeerToPeer::handleDatagram(QNetworkDatagram datagram)
{
    if(!datagram.isValid()) {
        qDebug() << "error receiving p2p datagram." << socket_.error() << socket_.errorString();
        return;
    }

    QByteArray data = datagram.data();
    if(data.size() != MESSAGE_LENGTH) {
        qDebug() << "P2P data with invalid length" << data.size() << "should be" << MESSAGE_LENGTH;
        disconnectPeer(Binding(datagram.senderAddress(), datagram.senderPort()));
        return;
    }

    PeerToPeerMessage message = PeerToPeerMessage::fromDatagram(datagram);
    Binding peer = message.sender;
    if(debugLog_) {
        qDebug() << "P2P data from" << peer.toString() << message.typeString();
    }

    if(message.malformed) {
        qDebug() << "P2P malformed message from" << peer.toString() << message.typeString()
                 << "; closing connection.";
        disconnectPeer(Binding(datagram.senderAddress(), datagram.senderPort()));
        return;
    }

    if(message.celltype == PeerToPeerMessage::BUILD) {
        handleBuild(message);
        return;
    }

    if(message.celltype == PeerToPeerMessage::CREATED) {
        handleCreated(message);
        return;
    }

    QByteArray preface = data.left(UNENCRYPTED_HEADER_LEN);
    QByteArray encryptedPayload = data.mid(UNENCRYPTED_HEADER_LEN);

    // we'll need auth at one point, so init here
    OnionAuthRequest storage;
    storage.peer = peer;
    storage.circuitId = message.circuitId;

    quint32 tunnelId = tunnelIds_.tunnelId(peer, message.circuitId);

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

    if(circuits_.contains(tunnelId)) {
        // 1.
        // layered decrypt
        CircuitState circuit = circuits_[tunnelId];

        storage.type = OnionAuthRequest::LayeredDecrypt;
        storage.remainingHops = circuit.hopStates;

        continueLayeredDecrypt(storage, encryptedPayload);
        return;
    }


    TunnelState *state = findTunnelByPreviousHopId(tunnelId);
    if(state != nullptr) {
        // from prevHop towards dest
        // 3. decrypt once, then forward to nexthop
        storage.type = OnionAuthRequest::DecryptOnce;
        storage.nextHop = state->nextHop;
        storage.nextHopCircuitId = state->circIdNextHop;

        // decrypt with K_src,us,
        // which is the key associated with tunnelIdPreviousHop
        if(!sessions_.has(state->tunnelIdPreviousHop)) {
            qDebug() << "broken tunnel: no session with source or no nexthop for tunnel"
                     << state->previousHop.toString() << "<-> us <->"
                     << state->nextHop.toString();
            return;
        }

        quint16 session = sessions_.get(state->tunnelIdPreviousHop);
        quint32 reqId = nextAuthRequest_++;
        decryptQueue_[reqId] = storage;
        requestDecrypt(reqId, session, encryptedPayload);
        return;
    }

    state = findTunnelByNextHopId(tunnelId);
    if(state != nullptr) {
        // from nextHop -> towards src
        // 2. encrypt once, then forward
        storage.type = OnionAuthRequest::EncryptOnce;
        storage.nextHop = state->previousHop;
        storage.nextHopCircuitId = state->circIdPreviousHop;

        // encrypt with K_src,us,
        // which is associated with tunnelIdPreviousHop
        if(!sessions_.has(state->tunnelIdPreviousHop)) {
            qDebug() << "broken tunnel: no session with source for tunnel"
                     << state->previousHop.toString() << "<-> us <->"
                     << state->nextHop.toString();
            return;
        }

        quint16 session = sessions_.get(state->tunnelIdPreviousHop);
        quint32 reqId = nextAuthRequest_++;
        encryptQueue_[reqId] = storage;
        requestEncrypt(reqId, session, encryptedPayload);
        return;
    }

    // tunnel not found
    qDebug() << "Discarding message with unknown tunnelId" << tunnelId << "<=>"
             << tunnelIds_.describe(tunnelId);
}

void PeerToPeer::handleBuild(PeerToPeerMessage message)
{
    // incoming tunnel
    // send handshake to auth to build us a session
    if(debugLog_) {
        qDebug() << "handleBuild, requesting session";
    }

    quint32 reqId = nextAuthRequest_++;
    incomingTunnels_[reqId] = tunnelIds_.tunnelId(message.sender, message.circuitId);
    sessionIncomingHS1(reqId, message.data);
    // triggers onSessionHS2
}

void PeerToPeer::handleCreated(PeerToPeerMessage message)
{
    // nexthop established
    // a) part of a circuit we initiated
    // b) part of an incomming tunnel, i.e. an earlier relay_extend
    quint32 nextHopTunnelId = tunnelIds_.tunnelId(message.sender, message.circuitId);
    if(pendingTunnelExtensions_.contains(nextHopTunnelId)) {
        // b)
        // setup extended tunnel
        quint32 incomingTunnelId = pendingTunnelExtensions_[nextHopTunnelId];
        TunnelState *state = findTunnelByPreviousHopId(incomingTunnelId);
        if(state == nullptr) {
            qDebug() << "got an orphaned relay_extend->created response from"
                     << tunnelIds_.describe(nextHopTunnelId) << "originator is"
                     << tunnelIds_.describe(incomingTunnelId);
            return;
        }

        state->tunnelIdNextHop = nextHopTunnelId;
        state->nextHop = message.sender;
        state->circIdNextHop = message.circuitId;

        // send relay_extended with handshake response
        PeerToPeerMessage msg = PeerToPeerMessage::makeRelayExtended(state->circIdPreviousHop, 0, message.data);
        sendPeerToPeerMessage(msg, state->previousHop);
        return;
    }


    // if a), can only be the first hop, thus nextHopTunnelId is the circuit's tunnelId
    if(!circuits_.contains(nextHopTunnelId)) {
        qDebug() << "orphaned CREATED message received from" << tunnelIds_.describe(nextHopTunnelId);
        return;
    }

    // a)
    CircuitState &state = circuits_[nextHopTunnelId];
    HopState &hop = state.hopStates.first();
    // finish handshake -> send to auth
    sessionIncomingHS2(nextAuthRequest_++, hop.sessionKey, message.data);
    // set status in circuit
    hop.status = Created;
    // continue circuit build
    continueBuildingTunnel(nextHopTunnelId);
    if(debugLog_) {
        qDebug() << "initial tunnel hop ready, extending...";
    }
}

void PeerToPeer::handleMessage(PeerToPeerMessage message, quint32 originatorTunnelId)
{
    if(message.malformed || message.celltype != PeerToPeerMessage::ENCRYPTED ||
            message.command == PeerToPeerMessage::CMD_INVALID || !message.isValidDigest()) {
        qDebug() << "invalid message in handleMessage" << message.typeString()
                 << "digest ok? ->" << message.isValidDigest()
                 << "originator seems to be" << tunnelIds_.describe(originatorTunnelId);
        return;
    }

    if(debugLog_) {
        qDebug() << "handleMessage ->" << message.typeString();
    }

    switch (message.command) {
    case PeerToPeerMessage::RELAY_DATA:
        // get tunnelId
        // emit tunnelData with tunnelId_us_src
        emit tunnelData(originatorTunnelId, message.data);
        break;
    case PeerToPeerMessage::RELAY_EXTEND:
    {
        // save binding, setup tunnel/circuit ids
        Binding nexthop(message.address, message.port);
        quint16 nextHopCircuitId = tunnelIds_.nextCircId(nexthop);
        quint32 nextHopId = tunnelIds_.tunnelId(nexthop, nextHopCircuitId);
        pendingTunnelExtensions_[nextHopId] = originatorTunnelId;

        // send build with handshake we got
        PeerToPeerMessage build = PeerToPeerMessage::makeBuild(nextHopCircuitId, message.data);
        QNetworkDatagram dgram = build.toDatagram(nexthop); // cant encrypt this message, directly send
        dgram.setSender(interface_, port_);
        qDebug() << "RELAY_EXTEND -> sending build to" << nexthop.toString();
        socket_.writeDatagram(dgram);
        // await created, then send relay_extended
    }
        break;
    case PeerToPeerMessage::RELAY_EXTENDED:
    {
        // get circuit that we extended -> circuitId is tunnelId of this message
        quint32 circuitTunnelId = tunnelIds_.tunnelId(message.sender, message.circuitId);

        if(!circuits_.contains(circuitTunnelId)) {
            qDebug() << "orphaned RELAY_EXTENDED received from neighbour"
                     << tunnelIds_.describe(circuitTunnelId) << "originator"
                     << tunnelIds_.describe(originatorTunnelId);
            return;
        }

        CircuitState &state = circuits_[circuitTunnelId];
        for(int i = 0; i < state.hopStates.size(); i++) {
            HopState &hopState = state.hopStates[i];
            if(hopState.status == BuildSent) {
                // verify that originator is the hop before this one
                if(i > 0) {
                    if(state.hopStates[i - 1].tunnelId != originatorTunnelId) {
                        qDebug() << "RELAY_EXTENDED does not fit tunnel state: originator should be"
                                 << tunnelIds_.describe(state.hopStates[i - 1].tunnelId)
                                 << "but is" << tunnelIds_.describe(originatorTunnelId);
                    }
                }

                qDebug() << "Tunnel extended successfully until" << hopState.peer.toString();

                // finish session establishment
                sessionIncomingHS2(nextAuthRequest_++, hopState.sessionKey, message.data);
                // apply state change
                hopState.status = Created;
                // continue building tunnel
                continueBuildingTunnel(circuitTunnelId);
                return;
            }
        }
        qDebug() << "orphaned RELAY_EXTENDED received. Tunnel is already built.";
    }
        break;
    case PeerToPeerMessage::RELAY_TRUNCATED:
        // should only come from later hop to us when we started the circuit
        // tear circuit if not from start node
        // clean up circuit
    {
        if(debugLog_) {
            qDebug() << "truncated circuit, reporting & cleaning up";
        }

        // find circuit
        quint32 circuitTunnelId = tunnelIds_.tunnelId(message.sender, message.circuitId);
        if(!circuits_.contains(circuitTunnelId)) {
            return;
        }

        CircuitState &circuit = circuits_[circuitTunnelId];
        int lastPeerIndex = -1;
        for(int i = 0; i < circuit.hopStates.size(); i++) {
            if(circuit.hopStates[i].tunnelId == originatorTunnelId) {
                lastPeerIndex = i;
                break;
            }
        }

        if(lastPeerIndex == -1) {
            // peer is not in this circuit??
            return;
        }

        // fix circuit state, to represent the truncated connection
        circuit.hopStates = circuit.hopStates.mid(0, lastPeerIndex + 1);
        // now tear the circuit
        tearCircuit(circuitTunnelId, true);
        // announce circuit failure to api
        tunnelError(circuit.circuitApiTunnelId, circuit.lastMessage);
    }
        break;
    case PeerToPeerMessage::CMD_DESTROY:
        // should only come from previous node
    {
        if(debugLog_) {
            qDebug() << "tunnel destroyed by source";
        }
        TunnelState *state = findTunnelByPreviousHopId(originatorTunnelId);
        if(state != nullptr) {
            // we cannot talk to nexthop directly, originator must send destroys to all hops
            // cleanup tunnel
            requestEndSession(sessions_.get(state->tunnelIdPreviousHop));
            sessions_.remove(state->tunnelIdPreviousHop);
            tunnels_.removeOne(*state);
        }
    }
        break;
    case PeerToPeerMessage::CMD_COVER:
        // do nothing
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

    HopState hop = request.remainingHops.takeLast(); // encrypt is in reverse order, take from back
    if(hop.status != Created) {
        qDebug() << "continueLayeredEncrypt with wrong hop state:" << hop.status
                 << "Discarding request after" << request.operations << "encrypts.";
        return;
    }

    request.operations++;

    // request encrypt
    quint32 reqId = nextAuthRequest_++;
    encryptQueue_[reqId] = request;
    requestEncrypt(reqId, hop.sessionKey, payload);
}

void PeerToPeer::forwardEncryptedMessage(Binding to, quint16 circuitId, QByteArray payload)
{
    QByteArray newMessage = PeerToPeerMessage::composeEncrypted(circuitId, payload);
    QNetworkDatagram packet(newMessage, to.address, to.port);
    packet.setSender(interface_, port_);
    qint64 ok = socket_.writeDatagram(packet);
    if(debugLog_) {
        qDebug() << (ok == -1 ? QString("FAIL") : QString("OK")) << "sent packet to" << to.toString();
    }
}

void PeerToPeer::sendPeerToPeerMessage(PeerToPeerMessage unencrypted, Binding target)
{
    QByteArray msgPayload = unencrypted.toEncryptedPayload();

    // encrypt once then send
    OnionAuthRequest request;
    request.type = OnionAuthRequest::EncryptOnce;
    request.nextHop = target;
    request.nextHopCircuitId = unencrypted.circuitId;
    request.debugString = QString("direct-encrypt %1 to %2").arg(unencrypted.typeString(), target.toString());

    quint16 sessionId = sessions_.get(tunnelIds_.tunnelId(target, unencrypted.circuitId));
    quint32 reqId = nextAuthRequest_++;
    encryptQueue_[reqId] = request;
    requestEncrypt(reqId, sessionId, msgPayload);
}

void PeerToPeer::sendPeerToPeerMessage(PeerToPeerMessage unencrypted, QVector<PeerToPeer::HopState> tunnel)
{
    if(tunnel.isEmpty()) {
        return;
    }

    QByteArray msgPayload = unencrypted.toEncryptedPayload();

    OnionAuthRequest request;
    request.type = OnionAuthRequest::LayeredEncrypt;
    request.nextHop = tunnel[0].peer;
    request.nextHopCircuitId = tunnel[0].circuitId;
    request.remainingHops = tunnel;
    request.debugString = QString("onion-encrypt %1 towards %2").arg(unencrypted.typeString(), tunnel.last().peer.toString());

    continueLayeredEncrypt(request, msgPayload);
}

void PeerToPeer::tearCircuit(quint32 tunnelId, bool clean)
{
    if(!circuits_.contains(tunnelId)) {
        return;
    }

    CircuitState state = circuits_[tunnelId];
    // send destroys in reverse order, give them some delay, so that later ones still get delivered
    for(int i = state.hopStates.size(); i > 0; i--) {
        int delay = (state.hopStates.size() - i) * 500;
        QVector<HopState> hops = state.hopStates.mid(0, i);
        PeerToPeerMessage message = PeerToPeerMessage::makeCommandDestroy(hops.last().circuitId);
        QTimer::singleShot(delay, [=]() {
            sendPeerToPeerMessage(message, hops);
        });
    }

    if(clean) {
        QTimer::singleShot(500 * (state.hopStates.size() + 1), [=]() {
            cleanCircuit(tunnelId);
        });
    }
}

void PeerToPeer::cleanCircuit(quint32 tunnelId)
{
    if(!circuits_.contains(tunnelId)) {
        return;
    }

    CircuitState state = circuits_.take(tunnelId);
    state.retryEstablishingTimer->deleteLater();
    for(HopState hop : state.hopStates) {
        if(hop.status == Created) {
            // clear auth sessions
            requestEndSession(hop.sessionKey);
            sessions_.remove(hop.tunnelId);
        }
    }
}

void PeerToPeer::peersArrived(int id, QList<PeerSampler::Peer> peers)
{
    if(!pendingPeerSamples_.contains(id)) {
        qDebug() << "peersArrived: unknown sample id";
        return;
    }

    if(debugLog_) {
        qDebug() << "peersArrived ->" << peers.size();
    }

    PeerSample sample = pendingPeerSamples_.take(id);
    if(sample.isBuildTunnel) {
        peers.append(sample.dest);
    }

    // get handshake for each peer
    CircuitHandshakes handshakes;
    handshakes.coverTrafficBytes = sample.coverTrafficBytes;
    handshakes.isBuildTunnel = sample.isBuildTunnel;
    handshakes.requesterId = sample.requesterId;

    for(auto hop : peers) {
        BuildTunnelPeer peer;
        peer.peer = hop.address;
        peer.hostkey = hop.hostkey;
        peer.authRequestId = nextAuthRequest_++;
        handshakes.peers.append(peer);
    }

    pendingCircuitHandshakes_.append(handshakes);
    for(auto peer : handshakes.peers) {
        // request actual handshake from auth only after inserting data
        requestStartSession(peer.authRequestId, peer.hostkey);
    }
}

void PeerToPeer::continueBuildingTunnel(quint32 id, bool isRetry)
{
    if(!circuits_.contains(id)) {
        return;
    }

    CircuitState &state = circuits_[id];
    int nextBuildIndex = -1;
    for(int i = 0; i < state.hopStates.size(); i++) {
        HopState &hop = state.hopStates[i];
        if(hop.status == Created) {
            continue;
        } else if(hop.status == BuildSent) {
            if(isRetry) {
                nextBuildIndex = i;
                break;
            } else {
                // wait more
                return;
            }
        } else {
            // establish with this hop
            nextBuildIndex = i;
            break;
        }
    }

    if(nextBuildIndex == -1) {
        // tunnel is ready
        state.retryEstablishingTimer->stop();
        if(state.remainingCoverData > 0) {
            sendCoverData(id);
        } else {
            tunnelReady(state.requesterId, state.circuitApiTunnelId, state.hopStates.last().peerHostkey);
        }
        return;
    }

    // send build / extend & set status
    HopState &nextHopState = state.hopStates[nextBuildIndex];
    nextHopState.status = BuildSent;
    quint16 circId = state.hopStates.first().circuitId;
    if(nextBuildIndex == 0) {
        // send a build
        PeerToPeerMessage build = PeerToPeerMessage::makeBuild(circId, nextHopState.peerHandshakeHS1);
        QNetworkDatagram dgram = build.toDatagram(nextHopState.peer);
        dgram.setSender(interface_, port_);
        qDebug() << "building circuit -> sent build to" << nextHopState.peer.toString();
        socket_.writeDatagram(dgram);
    } else {
        // send a relay extend
        QVector<HopState> halfOnion = state.hopStates.mid(0, nextBuildIndex); // should go until predecessor of nextHop
        PeerToPeerMessage extend = PeerToPeerMessage::makeRelayExtend(circId, 0, nextHopState.peer, nextHopState.peerHandshakeHS1);
        sendPeerToPeerMessage(extend, halfOnion);
    }

    // start retry timer
    state.retryEstablishingTimer->start();
}

void PeerToPeer::sendCoverData(quint32 tunnelId)
{
    if(!circuits_.contains(tunnelId)) {
        return;
    }

    CircuitState &state = circuits_[tunnelId];
    if(state.remainingCoverData <= 0) {
        return;
    }

    state.remainingCoverData -= MESSAGE_LENGTH;
    PeerToPeerMessage message = PeerToPeerMessage::makeCommandCover(state.hopStates.first().circuitId);
    sendPeerToPeerMessage(message, state.hopStates);

    // random backoff for next cover message
    float rand = (float)qrand() / (float)RAND_MAX;
    int minWait = 300, maxWait = 2000;
    int wait = (int)((maxWait - minWait) * rand) + minWait;

    QTimer::singleShot(wait, [=]() { sendCoverData(tunnelId); });
}

void PeerToPeer::disconnectPeer(Binding who)
{
    // phew this is a lot
    // tear circuits with this guy
    // tear tunnels with this guy
    // don't do anything yet so we can debug properly
    if(debugLog_) {
        qDebug() << "tearing connections with misbehaving peer" << who.toString();
    }
}

PeerToPeer::TunnelState *PeerToPeer::findTunnelByPreviousHopId(quint32 tId)
{
    for(TunnelState &candidate : tunnels_) {
        if(candidate.tunnelIdPreviousHop == tId) {
            return &candidate;
        }
    }

    return nullptr;
}

PeerToPeer::TunnelState *PeerToPeer::findTunnelByNextHopId(quint32 tId)
{
    for(TunnelState &candidate : tunnels_) {
        if(candidate.tunnelIdNextHop == tId) {
            return &candidate;
        }
    }

    return nullptr;
}

void PeerToPeer::setDebugLog(bool debugLog)
{
    debugLog_ = debugLog;
}

int PeerToPeer::nHops() const
{
    return nHops_;
}

void PeerToPeer::setNHops(int nHops)
{
    nHops_ = nHops;
}

void PeerToPeer::setPeerSampler(PeerSampler *sampler)
{
    peerSampler_ = sampler;
    connect(sampler, &PeerSampler::peersArrived, this, &PeerToPeer::peersArrived);
}

void PeerToPeer::buildTunnel(QHostAddress destinationAddr, quint16 destinationPort, QByteArray hostkey, QTcpSocket *requestId)
{
    // get some middle peers
    PeerSample sample;
    sample.isBuildTunnel = true;
    PeerSampler::Peer dest;
    dest.address = Binding(destinationAddr, destinationPort);
    dest.hostkey = hostkey;
    sample.dest = dest;
    sample.requesterId = requestId;

    int sampleId = peerSampler_->requestPeers(nHops_);
    pendingPeerSamples_[sampleId] = sample;
    // wait for peersArrived()
}

void PeerToPeer::destroyTunnel(quint32 tunnelId)
{
    // find circuit with end-to-start tunnelid
    for(QHash<quint32, CircuitState>::iterator it = circuits_.begin(); it != circuits_.end(); it++) {
        CircuitState &state = it.value();
        if(state.circuitApiTunnelId == tunnelId) {
            state.lastMessage = MessageType::ONION_TUNNEL_DESTROY;
            tearCircuit(it.key(), true);
            return;
        }
    }

    // find tunnel with tunnelid
    TunnelState *state = findTunnelByPreviousHopId(tunnelId);
    if(state != nullptr) {
        // cleanup tunnel
        requestEndSession(state->tunnelIdPreviousHop);
        sessions_.remove(state->tunnelIdPreviousHop);

        // send truncated to source
        PeerToPeerMessage message = PeerToPeerMessage::makeRelayTruncated(state->circIdPreviousHop, 0);
        sendPeerToPeerMessage(message, state->previousHop);

        tunnels_.removeOne(*state);
    }
}

bool PeerToPeer::sendData(quint32 tunnelId, QByteArray data)
{
    // find circuit with end-to-start tunnelid
    for(QHash<quint32, CircuitState>::iterator it = circuits_.begin(); it != circuits_.end(); it++) {
        CircuitState &state = it.value();
        if(state.circuitApiTunnelId == tunnelId) {
            state.lastMessage = MessageType::ONION_TUNNEL_DATA;
            PeerToPeerMessage message = PeerToPeerMessage::makeRelayData(0, 0, data);
            sendPeerToPeerMessage(message, state.hopStates);
            return true;
        }
    }

    // find backwards-tunnel with start-us tunnelid
    TunnelState *tunnel = findTunnelByPreviousHopId(tunnelId);
    if(tunnel != nullptr) {
        PeerToPeerMessage message = PeerToPeerMessage::makeRelayData(tunnel->circIdPreviousHop, 0, data);
        sendPeerToPeerMessage(message, tunnel->previousHop);
        return true;
    }

    qDebug() << "failed to send data along tunnel" << tunnelId;
    return false;
}

void PeerToPeer::coverTunnel(quint16 size)
{
    // get some peers
    PeerSample sample;
    sample.isBuildTunnel = false;
    sample.coverTrafficBytes = size;

    int sampleId = peerSampler_->requestPeers(nHops_ + 1); // add 1 for random dest
    pendingPeerSamples_[sampleId] = sample;
    // wait for peersArrived()
}

void PeerToPeer::onEncrypted(quint32 requestId, quint16 sessionId, QByteArray payload)
{
    // we encrypted something => we're
    // a) in iterative encrypting of a request
    // b) encrypted a tunnel message we should forward

    Q_UNUSED(sessionId);
    OnionAuthRequest storage = encryptQueue_.take(requestId);

    if(storage.type == OnionAuthRequest::EncryptOnce) {
        // b)
        if(!storage.nextHop.isValid()) {
            qDebug() << "encryptOnce with invalid nextHop";
            return;
        }

        if(debugLog_) {
            qDebug() << "sending encrypt-once message ->" << storage.debugString;
        }
        forwardEncryptedMessage(storage.nextHop, storage.nextHopCircuitId, payload);
        return;
    }

    if(storage.type == OnionAuthRequest::LayeredEncrypt) {
        // a)
        if(storage.remainingHops.isEmpty()) {
            // done encrypting, send to first hop in circuit
            if(debugLog_) {
                qDebug() << "sending encrypt-onion message ->" << storage.debugString;
            }
            forwardEncryptedMessage(storage.nextHop, storage.nextHopCircuitId, payload);
            return;
        }

        // more layers to come, encrypt it further
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
    PeerToPeerMessage message = PeerToPeerMessage::fromEncryptedPayload(payload, storage.circuitId);
    message.sender = storage.peer;

    if(message.isValidDigest()) {
        // we need to figure out the sender of this message
        // and pass along the correct circuit ids and sender
        quint32 originatorTunnelId;
        if(storage.type == OnionAuthRequest::DecryptOnce) {
            // original sender is correct, along with circuit id
            originatorTunnelId = tunnelIds_.tunnelId(storage.peer, storage.circuitId);
        } else if(storage.type == OnionAuthRequest::LayeredDecrypt) {
            // we decrypted <storag.operations> times from the beginning
            // => sender is at circuitHops[operations-1]
            quint32 circuitTunnelId = tunnelIds_.tunnelId(storage.peer, storage.circuitId);
            if(!circuits_.contains(circuitTunnelId)) {
                qDebug() << "successfully decrypted layered message from unknown circuit"
                         << tunnelIds_.describe(circuitTunnelId) << "it probably went down";
                return;
            }
            const CircuitState &state = circuits_[circuitTunnelId];
            if(state.hopStates.size() < storage.operations) {
                qDebug() << "cannot find orginal sender of decrypted message. circuit might have been torn"
                         << message.typeString();
                return;
            } else {
                HopState senderHop = state.hopStates[storage.operations - 1];
                originatorTunnelId = senderHop.tunnelId;
            }
        }

        handleMessage(message, originatorTunnelId);
        return;
    }

    // not valid yet
    if(storage.type == OnionAuthRequest::DecryptOnce) {
        // b)
        if(!storage.nextHop.isValid()) {
            qDebug() << "decryptOnce with invalid nextHop";
            return;
        }

        if(debugLog_) {
            qDebug() << "forwarding peeled, but still encrypted packet along tunnel" << storage.nextHop.toString();
        }
        forwardEncryptedMessage(storage.nextHop, storage.nextHopCircuitId, payload);
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
    // find our stuff in pendingHandshakes
    for(QList<CircuitHandshakes>::iterator it = pendingCircuitHandshakes_.begin(); it != pendingCircuitHandshakes_.end();) {
        bool done = true;
        for(BuildTunnelPeer &hop : it->peers) {
            if(hop.authRequestId == requestId) {
                hop.handshake = handshake;
                hop.sessionId = sessionId;
            }
            if(hop.handshake.isEmpty()) {
                done = false;
            }
        }

        if(done) {
            // start building the tunnel
            CircuitState circuit;
            for(BuildTunnelPeer hop : it->peers) {
                HopState hopState;
                hopState.peer = hop.peer;
                hopState.sessionKey = hop.sessionId;
                hopState.peerHostkey = hop.hostkey;
                hopState.peerHandshakeHS1 = hop.handshake;
                hopState.status = Unconnected;
                hopState.circuitId = tunnelIds_.nextCircId(hopState.peer);
                hopState.tunnelId = tunnelIds_.tunnelId(hopState.peer, hopState.circuitId);
                circuit.hopStates.append(hopState);
            }
            circuit.requesterId = it->requesterId;
            circuit.circuitApiTunnelId = circuit.hopStates.last().tunnelId;
            circuit.lastMessage = it->isBuildTunnel ? MessageType::ONION_TUNNEL_BUILD : MessageType::ONION_COVER;
            circuit.remainingCoverData = it->isBuildTunnel ? 0 : it->coverTrafficBytes;

            quint32 circuitId = circuit.hopStates.first().tunnelId;

            // setup retry timer
            QTimer *retry = new QTimer(this);
            retry->setSingleShot(true);
            retry->setInterval(20000); // 20 secs
            connect(retry, &QTimer::timeout, this, [=]() { continueBuildingTunnel(circuitId, true); });
            circuit.retryEstablishingTimer = retry;

            circuits_[circuitId] = circuit;
            continueBuildingTunnel(circuitId);
            it = pendingCircuitHandshakes_.erase(it);
        } else {
            it++;
        }
    }
}

void PeerToPeer::onSessionHS2(quint32 requestId, quint16 sessionId, QByteArray handshake)
{
    if(!incomingTunnels_.contains(requestId)) {
        qDebug() << "generated 2nd-half handshake does not match any connection";
        return;
    }

//    qDebug() << "onSessionHS2";
    quint32 peerTunnelId = incomingTunnels_.take(requestId);
    Binding previousHop;
    quint16 previousHopCircuitId;
    tunnelIds_.decompose(peerTunnelId, &previousHop, &previousHopCircuitId);

    // setup tunnel on our side
    TunnelState newTunnel;
    newTunnel.previousHop = previousHop;
    newTunnel.circIdPreviousHop = previousHopCircuitId;
    newTunnel.tunnelIdPreviousHop = peerTunnelId;
    // setup session established with other side
    sessions_.set(peerTunnelId, sessionId);

    tunnels_.append(newTunnel);

    // send back handshake in a CREATED message
    PeerToPeerMessage created = PeerToPeerMessage::makeCreated(previousHopCircuitId, handshake);
    QNetworkDatagram dgram = created.toDatagram(previousHop);
    dgram.setSender(interface_, port_);
    if(debugLog_) {
        qDebug() << "incoming tunnel -> sent CREATED to" << previousHop.toString();
    }
    if(socket_.writeDatagram(dgram) == -1) {
        qDebug() << "send failed ->" << socket_.error() << socket_.errorString();
    }

    // announce tunnel
    tunnelIncoming(peerTunnelId);
}

bool PeerToPeer::TunnelState::operator ==(const PeerToPeer::TunnelState &other) const
{
    return previousHop == other.previousHop &&
            nextHop == other.nextHop &&
            circIdPreviousHop == other.circIdPreviousHop &&
            circIdNextHop == other.circIdNextHop &&
            tunnelIdPreviousHop == other.tunnelIdPreviousHop &&
            tunnelIdNextHop == other.tunnelIdNextHop;
}
