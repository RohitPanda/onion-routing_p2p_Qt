#include "onionapi.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QDataStream>

OnionApi::OnionApi(QObject *parent) : QObject(parent), server_(this)
{
    connect(&server_, &QTcpServer::newConnection, this, &OnionApi::onConnection);
}

int OnionApi::port() const
{
    return port_;
}

void OnionApi::setPort(int port)
{
    port_ = port;
}

QHostAddress OnionApi::interface() const
{
    return interface_;
}

void OnionApi::setInterface(const QHostAddress &interface)
{
    interface_ = interface;
}

void OnionApi::setBinding(Binding binding)
{
    setInterface(binding.address);
    setPort(binding.port);
}

bool OnionApi::start()
{
    return server_.listen(interface_, port_);
}

QString OnionApi::socketError() const
{
    return server_.errorString();
}

void OnionApi::sendTunnelReady(QTcpSocket *requester, quint32 tunnelId, QByteArray hostkey)
{
    if(!buffers_.contains(requester) || !requester->isOpen()) {
        qDebug() << "sendTunnelReady, but source requester is no longer connected, emitting destroyTunnel";
        emit requestDestroyTunnel(tunnelId);
        return;
    }

    //                    HDR  tunnelId
    quint16 messageLength = 4 + 4 + hostkey.length();
    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << messageLength;
    stream << (quint16)MessageType::ONION_TUNNEL_READY;
    stream << tunnelId;
    message.append(hostkey);

    // sanity
    Q_ASSERT(messageLength == message.length());

    if(requester->write(message) == -1) {
        qDebug() << "failed to send ONION_TUNNEL_READY (" << requester->errorString()
                 << "), tearing down tunnel..";
        emit requestDestroyTunnel(tunnelId);
    } else {
        // establish mapping tunnelId->socket
        tunnelMapping_[tunnelId] = requester;
    }
}

void OnionApi::sendTunnelIncoming(quint32 tunnelId, QByteArray hostkey)
{
    // tunnel incoming will be sent to all api clients
    if(nClients_ > 1) {
        qDebug() << "ONION_TUNNEL_INCOMING, but multiple clients connected. Notifying each one, but they might destroy each others tunnels..";
    }

    //                    HDR  tunnelId
    quint16 messageLength = 4 + 4 + hostkey.length();
    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << messageLength;
    stream << (quint16)MessageType::ONION_TUNNEL_INCOMING;
    stream << tunnelId;
    message.append(hostkey);

    // sanity
    Q_ASSERT(messageLength == message.length());

    for(QTcpSocket *socket : buffers_.keys()) {
        if(socket->isOpen()) {
            if(socket->write(message) == -1) {
                qDebug() << "Failed to send ONION_TUNNEL_INCOMING to a client (total" << nClients_
                         << ") ->" << socket->errorString();
            }
        }
    }
}

void OnionApi::sendTunnelData(quint32 tunnelId, QByteArray data)
{
    if(!tunnelMapping_.contains(tunnelId)) {
        qDebug() << "No mapping entry for tunnelId" << tunnelId << "not destroying dangling tunnel..";
        return;
    }

    QTcpSocket *peer = tunnelMapping_[tunnelId];

    //                    HDR  tunnelId
    quint16 messageLength = 4 + 4 + data.length();
    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << messageLength;
    stream << (quint16)MessageType::ONION_TUNNEL_DATA;
    stream << tunnelId;
    message.append(data);

    // sanity
    Q_ASSERT(messageLength == message.length());

    if(peer->write(message) == -1) {
        qDebug() << "failed to write" << data.size() << "bytes to tunnel" << tunnelId
                 << "->" << peer->errorString();
    }
}

void OnionApi::sendTunnelError(quint32 tunnelId, MessageType requestType)
{
    if(!tunnelMapping_.contains(tunnelId)) {
        qDebug() << "No mapping entry for tunnelId" << tunnelId;
        return;
    }

    QTcpSocket *peer = tunnelMapping_[tunnelId];

    //                     HDR request reserved tunnelId
    quint16 messageLength = 4 +    2   +    2  +  4;
    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << messageLength;
    stream << (quint16)MessageType::ONION_ERROR;
    stream << (quint16)requestType;
    stream << (quint16)0;
    stream << tunnelId;

    // sanity
    Q_ASSERT(messageLength == message.length());

    if(peer->write(message) == -1) {
        qDebug() << "Cannot send ONION_ERROR for tunnel" << tunnelId
                 << "cleaning up tunnel anyway.. ->" << peer->errorString();
    }

    // tunnel is torn on error -> do local cleanup
    onTunnelDestroyed(tunnelId);
}

void OnionApi::onConnection()
{
    while (server_.hasPendingConnections()) {
        QTcpSocket *socket = server_.nextPendingConnection();
        buffers_[socket] = QByteArray(); // we use buffers_ for maintaining connected clients -> insert here early
        nClients_++;
        qDebug() << "client connected from" << socket->peerAddress() << ":" << socket->peerPort()
                 << "|" << nClients_ << "clients connected.";

        connect(socket, &QTcpSocket::readyRead, this, [=]() {
            onData(socket);
        });
        connect(socket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error), this,
                [=]() {
            qDebug() << "Api socket error:" << socket->errorString();
            socket->close();
            socket->deleteLater();
        });
        connect(socket, &QTcpSocket::disconnected, this, [=]() {
            qDebug() << "Client disconnected";
            socket->deleteLater();
        });
        connect(socket, &QTcpSocket::destroyed, this, [=]() {
            buffers_.remove(socket); // clean up buffer
            // clean up tunnel mapping
            for(auto it = tunnelMapping_.begin(); it != tunnelMapping_.end();) {
                if(it.value() == socket) {
                    it = tunnelMapping_.erase(it);
                } else {
                    it++;
                }
            }
            nClients_--;
            qDebug() << nClients_ << "api clients remain";
        });
    }
}

void OnionApi::onData(QTcpSocket *socket)
{
    QByteArray &buffer = buffers_[socket]; // does auto-insert
    buffer.append(socket->readAll());

    if(buffer.size() < 4) {
        // wait for more data
        return;
    }

    // read message header
    QDataStream stream(buffer); // does endianness with qt types for us
    stream.setByteOrder(QDataStream::BigEndian); // network byte order

    quint16 size, messageTypeInt;
    stream >> size;
    stream >> messageTypeInt;

    qDebug() << "got data from" << socket->peerAddress() << "->" << size
             << "bytes, type" << messageTypeInt << buffer.size() << "bytes in buffer";

    if(size > buffer.size()) {
        // wait for more data
        return;
    }

    // take message
    QByteArray message = buffer.mid(0, size);
    buffer.remove(0, size);
    MessageType type = (MessageType)messageTypeInt;

    switch (type) {
    case MessageType::ONION_TUNNEL_BUILD:
        readTunnelBuild(message, socket);
        break;
    case MessageType::ONION_TUNNEL_DESTROY:
        readTunnelDestroy(message, socket);
        break;
    case MessageType::ONION_TUNNEL_DATA:
        readTunnelData(message, socket);
        break;
    case MessageType::ONION_COVER:
        readTunnelCover(message, socket);
        break;
    case MessageType::ONION_TUNNEL_READY:
    case MessageType::ONION_TUNNEL_INCOMING:
    case MessageType::ONION_ERROR:
        qDebug() << "onion-api: cannot command a onion signal-type ->" << messageTypeInt << "discarding message";
        break;
    default:
        qDebug() << "onion-api: discarding message of invalid type" << messageTypeInt;
        break;
    }
}

void OnionApi::readTunnelBuild(QByteArray message, QTcpSocket *client)
{
    // header 4B
    // reserved 2B -> 1bit v4 flag, port 2B
    // ip addr 16B/4B
    // hostkey VARCHAR
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(6);

    quint16 port, ipV;
    stream >> port;
    stream >> ipV;

    QByteArray hostkey;
    QHostAddress ip;

    bool ipV4 = (ipV & 0x0001) == 0x0000;
    if(ipV4) {
        quint32 ipaddr;
        stream >> ipaddr;
        ip.setAddress(ipaddr);
        hostkey = message.mid(12);
    } else {
        QByteArray ipArr = message.mid(8, 16);
        // constructor takes 16 bytes of ipv6 addr in network byte order -> ok
        ip = QHostAddress(reinterpret_cast<quint8*>(ipArr.data()));
        hostkey = message.mid(24);
    }

    if(port == 0) {
        qDebug() << "cannot build tunnel to port 0";
        return;
    }

    if(ip.isNull()) {
        qDebug() << "got invalid ip from ONION TUNNEL BUILD. Discarding message";
        return;
    }

    emit requestBuildTunnel(ip, port, hostkey, client);
}

void OnionApi::readTunnelDestroy(QByteArray message, QTcpSocket *client)
{
    // header 4b
    // tunnel id 4b
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(4);

    quint32 tunnelId;
    stream >> tunnelId;
    emit requestDestroyTunnel(tunnelId);
    Q_UNUSED(client);
}

void OnionApi::readTunnelData(QByteArray message, QTcpSocket *client)
{
    // header 4b
    // tunnel id 4b
    // data varchar
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(4);

    quint32 tunnelId;
    stream >> tunnelId;

    QByteArray data = message.mid(8);

    emit requestSendTunnel(tunnelId, data);
    Q_UNUSED(client);
}

void OnionApi::readTunnelCover(QByteArray message, QTcpSocket *client)
{
    // header 4b
    // requested random data length 2b, reserved 2b
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(4);

    quint16 randomDataSize;
    stream >> randomDataSize;

    emit requestBuildCoverTunnel(randomDataSize);
    Q_UNUSED(client);
}

void OnionApi::onTunnelDestroyed(quint32 tunnelId)
{
    tunnelMapping_.remove(tunnelId);
}






