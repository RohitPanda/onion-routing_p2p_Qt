#ifndef RPSAPI_H
#define RPSAPI_H

#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>
#include "binding.h"
#include "messagetypes.h"
#include "metatypes.h"

class RPSApi : public QObject
{
    Q_OBJECT
public:
    explicit RPSApi(QObject *parent = 0);

    void setHost(QHostAddress address, quint16 port);
    void setHost(Binding binding);

    void start();

signals:
    void onPeer(QHostAddress address, quint16 port, QByteArray hostkey);

public slots:
    void requestPeer();

private:
    void maybeReconnect();
    void onData();

    bool running_ = false;
    QHostAddress host_;
    quint16 port_ = -1;

    QTcpSocket socket_;

    QByteArray buffer_;
};

#endif // RPSAPI_H
