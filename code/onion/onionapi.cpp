#include "onionapi.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QDataStream>
#include "messagetypes.h"

OnionApi::OnionApi(QObject *parent) : QObject(parent), server_(this)
{
    connect(&server_, &QTcpServer::newConnection, this, &OnionApi::onConnection);


    static bool initmetatypes = false;
    if(!initmetatypes) {
        initmetatypes = true;
        qRegisterMetaType<QHostAddress>();
    }
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

void OnionApi::onConnection()
{
    while (server_.hasPendingConnections()) {
        QTcpSocket *socket = server_.nextPendingConnection();
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
    // reserved 2B, port 2B
    // ip addr 16B
    // hostkey VARCHAR
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(6);

    quint16 port;
    stream >> port;

    QByteArray ipArr = message.mid(8, 16);
    // constructor takes 16 bytes of ipv6 addr in network byte order -> ok
    QHostAddress ip(reinterpret_cast<quint8*>(ipArr.data()));
    QByteArray hostkey = message.mid(24);

    if(port == 0) {
        qDebug() << "cannot build tunnel to port 0";
        return;
    }

    if(ip.isNull()) {
        qDebug() << "got invalid ip from ONION TUNNEL BUILD. Discarding message. IP hex:" << ipArr.toHex();
        return;
    }

    emit requestBuildTunnel(ip, port, hostkey);
    Q_UNUSED(client);
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






