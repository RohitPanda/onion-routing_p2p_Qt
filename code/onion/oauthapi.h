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
    void requestAuthSessionStart(Binding peer, QByteArray key);
    void requestAuthSessionIncomingHS1(QByteArray hsp1);
    void requestAuthSessionIncomingHS2(QByteArray hsp2);
    void requestAuthLayerEncrypt(QVector<Binding> peers, QByteArray cleartextPayload);
    void requestAuthLayerDecrypt(QVector<Binding> peers, QByteArray encryptedPayload);
    void requestAuthCipherEncrypt(Binding peer, QByteArray payload, quint16 payload_type);
    void requestAuthCipherDecrypt(Binding peer, QByteArray payload, quint16 payload_type);
    void requestAuthSessionClose(Binding peer);

private slots:
    void readAuthSessionHS1();
    void readAuthSessionHS2();
    void readAuthLayerEncryptResp();
    void readAuthLayerDecryptResp();
    void readAuthCipherEncryptResp();
    void readAuthCipherDecryptResp();
    void readAuthError();

private:
    void maybeReconnect();
    void onData();

    quint16 getSessionId(Binding Peer);

    bool checkRequestId(quint16 sessionId, quint16 requestId);

    struct Hop
    {
        Binding peer;
        QByteArray key;
        quint16 sessionId;
        quint16 last_requestId;
    };

    bool running_ = false;

    QHostAddress host_;

    quint16 port_ = -1;

    QTcpSocket socket_;

    QByteArray buffer_;

    QVector<Hop> Hops;

};



#endif // OAUTHAPI_H
