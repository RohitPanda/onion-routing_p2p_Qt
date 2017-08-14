#include "oauthapitester.h"
#include "oauthapi.h"

#include <QTcpSocket>

OAuthApiTester::OAuthApiTester(QObject *parent) : QObject(parent)
{

}

void OAuthApiTester::setupPassiveApi(OAuthApi *api, QTcpSocket *socket, QHostAddress addr, quint16 port)
{
    api->setHost(addr, port);

    api->start();

    QTest::qWait(2000);

    int i = 0;
    while(i < 30 && socket->state() != QAbstractSocket::ConnectedState) {
        QTest::qWait(100);
    }

    QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void OAuthApiTester::testSessionStart()
{
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("001602580000000000000017696D61686F73746B6579");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5127, onData, nullptr);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5127);
    api.start();

    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);

    api.requestAuthSessionStart(23, QByteArray("imahostkey"));


    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;


}

void OAuthApiTester::testEncrypt()
{

}

void OAuthApiTester::testDecrypt()
{

}

void OAuthApiTester::testLayeredEncrypt()
{

}

void OAuthApiTester::testLayeredDecrypt()
{

}

void OAuthApiTester::testSessionClose()
{

}

void OAuthApiTester::testEncryptResp()
{
    std::function<void(QTcpSocket *)> onConnection = [=](QTcpSocket *socket) {
        QByteArray peer = QByteArray::fromHex("0016025F000000000000001700000000000000000001");
        socket->write(peer);
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5127, nullptr, onConnection);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5127);

    // wait for connection
    QSignalSpy spy(&api, &OAuthApi::recvEncrypted);

    api.start();

    spy.wait(2000);
    QCOMPARE(spy.count(), 1);

    auto params = spy.takeFirst();

    QCOMPARE(params[0].toInt(), 23);
    //QCOMPARE(params[1].toInt(), );
    QCOMPARE(params[2].value<QByteArray>(), QByteArray::fromHex("00000000000000000001"));

    delete server;
}

void OAuthApiTester::testDecryptResp()
{

}

void OAuthApiTester::testLayeredEncryptResp()
{

}

void OAuthApiTester::testLayeredDecryptResp()
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
