#include "onionapi.h"

#include <QTcpSocket>

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

bool OnionApi::start()
{
    return server_.listen(interface_, port_);
}

void OnionApi::onConnection()
{
    while (server_.hasPendingConnections()) {
        QTcpSocket *socket = server_.nextPendingConnection();
        qDebug() << "client connected from" << socket->peerAddress() << ":" << socket->peerPort();
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
        });
    }
}

void OnionApi::onData(QTcpSocket *socket)
{
    qDebug() << "got data from" << socket->peerAddress();
    QByteArray &buffer = buffers_[socket]; // does auto-insert
    buffer.append(socket->readAll());

    // TODO process message from buffer
}
