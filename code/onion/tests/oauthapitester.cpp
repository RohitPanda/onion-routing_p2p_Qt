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
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("001A026300000000000000170075656e63727970742074686973");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5128, onData, nullptr);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5128);
    api.start();

    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);

    api.requestAuthCipherEncrypt(23, 117, QByteArray("encrypt this"));


    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;

}

void OAuthApiTester::testDecrypt()
{
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("001A026500000000000000170075646563727970742074686973");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5129, onData, nullptr);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5129);
    api.start();

    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);

    api.requestAuthCipherDecrypt(23, 117, QByteArray("decrypt this"));


    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;
}

void OAuthApiTester::testLayeredEncrypt()
{
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("001A025D00000100000000170075656e63727970742074686973");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5130, onData, nullptr);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5130);
    api.start();

    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);
    QVector<quint16> sessionIds = {117};

    api.requestAuthLayerEncrypt(23, sessionIds, QByteArray("encrypt this"));


    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;

}

void OAuthApiTester::testLayeredDecrypt()
{
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("001A025E00000100000000170075646563727970742074686973");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5131, onData, nullptr);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5131);
    api.start();

    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);

    QVector<quint16> sessionIds = {117};

    api.requestAuthLayerDecrypt(23, sessionIds, QByteArray("decrypt this"));


    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;
}

void OAuthApiTester::testSessionClose()
{
    bool hadData = false;
    std::function<void(QByteArray)> onData = [&](QByteArray data) {
        QByteArray expected = QByteArray::fromHex("000802610000007B");
        QCOMPARE(data, expected);
        hadData = true;
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5136, onData, nullptr);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5136);
    api.start();

    QSignalSpy spy(server, &QTcpServer::newConnection);
    spy.wait(1000);

    api.requestAuthSessionClose(123);


    QTest::qWait(500);
    QVERIFY(hadData);

    delete server;
}

void OAuthApiTester::testEncryptResp()
{
    std::function<void(QTcpSocket *)> onConnection = [=](QTcpSocket *socket) {
        QByteArray peer = QByteArray::fromHex("0016025F000000000000001700000000000000000001");
        socket->write(peer);
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5132, nullptr, onConnection);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5132);

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
    std::function<void(QTcpSocket *)> onConnection = [=](QTcpSocket *socket) {
        QByteArray peer = QByteArray::fromHex("00160260000000000000001700000000000000000001");
        socket->write(peer);
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5133, nullptr, onConnection);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5133);

    // wait for connection
    QSignalSpy spy(&api, &OAuthApi::recvDecrypted);

    api.start();

    spy.wait(2000);
    QCOMPARE(spy.count(), 1);

    auto params = spy.takeFirst();

    QCOMPARE(params[0].toInt(), 23);
    QCOMPARE(params[1].value<QByteArray>(), QByteArray::fromHex("00000000000000000001"));

    delete server;

}

void OAuthApiTester::testLayeredEncryptResp()
{
    std::function<void(QTcpSocket *)> onConnection = [=](QTcpSocket *socket) {
        QByteArray peer = QByteArray::fromHex("0016025F000000000000001700000000000000000001");
        socket->write(peer);
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5134, nullptr, onConnection);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5134);

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

void OAuthApiTester::testLayeredDecryptResp()
{
    std::function<void(QTcpSocket *)> onConnection = [=](QTcpSocket *socket) {
        QByteArray peer = QByteArray::fromHex("00160260000000000000001700000000000000000001");
        socket->write(peer);
    };

    auto server = tcpServer(QHostAddress::LocalHost, 5135, nullptr, onConnection);

    OAuthApi api;
    api.setHost(QHostAddress::LocalHost, 5135);

    // wait for connection
    QSignalSpy spy(&api, &OAuthApi::recvDecrypted);

    api.start();

    spy.wait(2000);
    QCOMPARE(spy.count(), 1);

    auto params = spy.takeFirst();

    QCOMPARE(params[0].toInt(), 23);
    QCOMPARE(params[1].value<QByteArray>(), QByteArray::fromHex("00000000000000000001"));

    delete server;

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
