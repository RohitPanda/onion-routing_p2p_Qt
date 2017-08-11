#ifndef OAUTHAPI_H
#define OAUTHAPI_H

#include "binding.h"
#include "messagetypes.h"
#include "metatypes.h"

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

//API for Onion Authentication Module
class OAuthApi : public QObject
{
    Q_OBJECT
public:
    explicit OAuthApi(QObject *parent = 0);

    void setHost(QHostAddress address, quint16 port);
    void setHost(Binding binding);

    void start();

signals:

public slots:

private slots:

private:
    void maybeReconnect();
    void onData();

    bool running_ = false;
    QHostAddress host_;
    quint16 port_ = -1;
    QTcpSocket socket_;
    QByteArray buffer_;

};



#endif // OAUTHAPI_H
