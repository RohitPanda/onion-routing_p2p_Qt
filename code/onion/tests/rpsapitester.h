#ifndef RPSAPITESTER_H
#define RPSAPITESTER_H

#include <QObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QTcpServer>
#include <functional>

class RPSApiTester : public QObject
{
    Q_OBJECT
public:
    explicit RPSApiTester(QObject *parent = 0);

signals:

public slots:

private slots:
    void testRequestPeer();
    void testReceivePeer();

private:
    QTcpServer *tcpServer(QHostAddress addr, quint16 port,
                          std::function<void(QByteArray)> callback,
                          std::function<void(QTcpSocket*)> connection);
};

#endif // RPSAPITESTER_H
