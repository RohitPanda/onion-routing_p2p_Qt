#include "onionapitester.h"

OnionApiTester::OnionApiTester(QObject *parent) : QObject(parent)
{

}

void OnionApiTester::setupPassiveApi(OnionApi *api, QTcpSocket *socket, QHostAddress addr, quint16 port)
{
    api->setInterface(addr);
    api->setPort(port);

    api->start();

    socket->connectToHost(addr, port);
    QTest::qWait(2000);

    int i = 0;
    while(i < 30 && socket->state() != QAbstractSocket::ConnectedState) {
        QTest::qWait(100);
    }

    QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void OnionApiTester::testBuildTunnel()
{
    QHostAddress addr(QHostAddress::LocalHost);
    int port = 5023;

    OnionApi api;
    api.setInterface(addr);
    api.setPort(port);
    QSignalSpy spy(&api, &OnionApi::requestBuildTunnel);
    api.start();


    QByteArray testMessage = QByteArray::fromHex("002f0230000042C500000000000000000000000000000001746869732069732061207465737420686f73746b657900");
    QTcpSocket s;
    s.connectToHost(addr, port);
    QVERIFY(s.isOpen());
    s.write(testMessage);

    spy.wait();

    QCOMPARE(spy.count(), 1);
    QVariantList params = spy.takeFirst();
    QCOMPARE(params.size(), 4);
    QCOMPARE(params[0].value<QHostAddress>(), QHostAddress(QHostAddress::LocalHostIPv6));
    QCOMPARE(params[1].toInt(), 17093);
    QCOMPARE(params[2].toByteArray(), QByteArray("this is a test hostkey\0", 23));

    s.close();
}

void OnionApiTester::testDestroyTunnel()
{
    QHostAddress addr(QHostAddress::LocalHost);
    int port = 5024;

    OnionApi api;
    api.setInterface(addr);
    api.setPort(port);
    QSignalSpy spy(&api, &OnionApi::requestDestroyTunnel);
    api.start();


    QByteArray testMessage = QByteArray::fromHex("00080233000042c5");
    QTcpSocket s;
    s.connectToHost(addr, port);
    QVERIFY(s.isOpen());
    s.write(testMessage);

    spy.wait();

    QCOMPARE(spy.count(), 1);
    QVariantList params = spy.takeFirst();
    QCOMPARE(params.size(), 1);
    QCOMPARE(params[0].toInt(), 17093);

    s.close();
}

void OnionApiTester::testSendTunnel()
{
    QHostAddress addr(QHostAddress::LocalHost);
    int port = 5024;

    OnionApi api;
    api.setInterface(addr);
    api.setPort(port);
    QSignalSpy spy(&api, &OnionApi::requestSendTunnel);
    api.start();


    QByteArray testMessage = QByteArray::fromHex("001e0234000042c57361792068656c6c6f20746f20746573742064617461");
    QTcpSocket s;
    s.connectToHost(addr, port);
    QVERIFY(s.isOpen());
    s.write(testMessage);

    spy.wait();

    QCOMPARE(spy.count(), 1);
    QVariantList params = spy.takeFirst();
    QCOMPARE(params.size(), 2);
    QCOMPARE(params[0].toInt(), 17093);
    QCOMPARE(params[1].toByteArray(), QByteArray("say hello to test data"));

    s.close();
}

void OnionApiTester::testBuildCoverTunnel()
{
    QHostAddress addr(QHostAddress::LocalHost);
    int port = 5025;

    OnionApi api;
    api.setInterface(addr);
    api.setPort(port);
    QSignalSpy spy(&api, &OnionApi::requestBuildCoverTunnel);
    api.start();


    QByteArray testMessage = QByteArray::fromHex("0008023642c50000");
    QTcpSocket s;
    s.connectToHost(addr, port);
    QVERIFY(s.isOpen());
    s.write(testMessage);

    spy.wait();

    QCOMPARE(spy.count(), 1);
    QVariantList params = spy.takeFirst();
    QCOMPARE(params.size(), 1);
    QCOMPARE(params[0].toInt(), 17093);

    s.close();
}

void OnionApiTester::testSendTunnelReady()
{
    OnionApi api;
    QTcpSocket socket;

    setupPassiveApi(&api, &socket, QHostAddress::LocalHost, 5027);
    QSignalSpy spy(&socket, &QTcpSocket::readyRead);

    // call
    QCOMPARE(api.buffers_.size(), 1);
    QTcpSocket *incomingSocket = api.buffers_.keys().first();
    api.sendTunnelReady(incomingSocket, 17, QByteArray("imahostkey"));

    spy.wait();
    QCOMPARE(spy.count(), 1);

    // read data
    QByteArray result = socket.readAll();
    QByteArray expected = QByteArray::fromHex("0012023100000011696d61686f73746b6579");
    QCOMPARE(result, expected);

    socket.close();
}

void OnionApiTester::testSendTunnelIncoming()
{

}

void OnionApiTester::testSendTunnelData()
{

}

void OnionApiTester::testSendTunnelError()
{

}

