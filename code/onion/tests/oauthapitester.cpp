#include "oauthapitester.h"
#include "oauthapi.h"

#include <QTcpSocket>

OAuthApiTester::OAuthApiTester(QObject *parent) : QObject(parent)
{

}


QTcpServer *OAuthApiTester::tcpServer(QHostAddress addr, quint16 port, std::function<void(QByteArray)> callback,
                                    std::function<void(QTcpSocket*)> connection)
{
    QTcpServer *server = new QTcpServer(this);
    server->listen(addr, port);

    connect(server, &QTcpServer::newConnection, [=]() {
        while (server->hasPendingConnections()) {
            QTcpSocket *socket = server->nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, [=]() {
                if(callback) {
                    callback(socket->readAll());
                }
            });
            if(connection) {
                connection(socket);
            }
        }
    });
    return server;
}
