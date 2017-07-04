#include "onionapitester.h"

#include "onionapi.h"
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

OnionApiTester::OnionApiTester(QObject *parent) : QObject(parent)
{

}

void OnionApiTester::testBuildTunnel()
{
    QHostAddress addr(QHostAddress::LocalHost);
    int port = 5023;

    OnionApi *api = new OnionApi(this);
    api->setInterface(addr);
    api->setPort(port);
    QSignalSpy spy(api, &OnionApi::requestBuildTunnel);
    api->start();


    QByteArray testMessage = QByteArray::fromHex("002f0230000042C500000000000000000000000000000001746869732069732061207465737420686f73746b657900");
    QTcpSocket s;
    s.connectToHost(addr, port);
    QVERIFY(s.isOpen());
    s.write(testMessage);

    spy.wait();

    QCOMPARE(spy.count(), 1);
    QVariantList params = spy.takeFirst();
    QCOMPARE(params.size(), 3);
    QCOMPARE(params[0].value<QHostAddress>(), QHostAddress(QHostAddress::LocalHostIPv6));
    QCOMPARE(params[1].toInt(), 17093);
    QCOMPARE(params[2].toByteArray(), QByteArray("this is a test hostkey\0", 23));

    s.close();
}

