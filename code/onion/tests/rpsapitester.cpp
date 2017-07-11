#include "rpsapitester.h"
#include "rpsapi.h"

#include <QTcpSocket>

RPSApiTester::RPSApiTester(QObject *parent) : QObject(parent)
{

}

void RPSApiTester::testRequestPeer()
{
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("0004021C");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5120, onData, nullptr);

    RPSApi api;
    api.setHost(QHostAddress::LocalHost, 5120);
    api.start();

    // wait for connection
    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);

    api.requestPeer();

    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;
}

void RPSApiTester::testReceivePeer()
{
    std::function<void(QTcpSocket *)> onConnection = [=](QTcpSocket *socket) {
        QByteArray peer = QByteArray::fromHex("0022021d427f00012a001450401680d00000000000002003727073686f73746b6579");
        socket->write(peer);
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5121, nullptr, onConnection);

    RPSApi api;
    api.setHost(QHostAddress::LocalHost, 5121);

    // wait for connection
    QSignalSpy spy(&api, &RPSApi::onPeer);

    api.start();

    spy.wait(2000);
    QCOMPARE(spy.count(), 1);

    auto params = spy.takeFirst();
    QCOMPARE(params[0].value<QHostAddress>(), QHostAddress("2a00:1450:4016:80d0::2003"));
    QCOMPARE(params[1].toInt(), 17023);
    QCOMPARE(params[2].value<QByteArray>(), QByteArray("rpshostkey"));

    delete server;
}

QTcpServer *RPSApiTester::tcpServer(QHostAddress addr, quint16 port, std::function<void(QByteArray)> callback,
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
