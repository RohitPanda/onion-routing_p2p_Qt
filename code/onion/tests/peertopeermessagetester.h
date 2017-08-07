#ifndef PEERTOPEERMESSAGETESTER_H
#define PEERTOPEERMESSAGETESTER_H

#include <QObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include "peertopeermessage.h"

class PeerToPeerMessageTester : public QObject
{
    Q_OBJECT
public:
    explicit PeerToPeerMessageTester(QObject *parent = 0);

signals:

public slots:

private slots:
    void testCmdBuild();
    void testCmdCreated();
    void testCmdDestroy();
    void testCmdCover();
    void testRelayData();
    void testRelayExtend4();
    void testRelayExtend6();
    void testRelayExtended();
    void testRelayTruncated();

private:
    void verifyWritePayload(PeerToPeerMessage message, QByteArray expectedPayload);
    PeerToPeerMessage verifyReadPayload(QByteArray payload);
};

#endif // PEERTOPEERMESSAGETESTER_H
