#ifndef ONIONAPITESTER_H
#define ONIONAPITESTER_H

#include <QObject>

#include "onionapi.h"
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

class OnionApiTester : public QObject
{
    Q_OBJECT
public:
    explicit OnionApiTester(QObject *parent = 0);

private:
    void setupPassiveApi(OnionApi *api, QTcpSocket *socket, QHostAddress addr, quint16 port);

private slots:
    // test methods below

    // incoming side (client->us)
    void testBuildTunnel();
    void testDestroyTunnel();
    void testSendTunnel();
    void testBuildCoverTunnel();

    // outgoing side (us->client)
    void testSendTunnelReady();
    void testSendTunnelIncoming();
    void testSendTunnelData();
    void testSendTunnelError();
};

#endif // ONIONAPITESTER_H
