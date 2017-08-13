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

    enum payloadType
    {
        PLAINTEXT = 0,
        ENCRYPTED = 1
    };

signals:
    void payloadFromAuthApi(QByteArray payload, payloadType type);

public slots:
    void requestAuthSessionStart(Binding peer, QByteArray key);
    void requestAuthSessionIncomingHS1(QByteArray hsp1);
    void requestAuthSessionIncomingHS2(Binding peer, QByteArray hsp2);
    void requestAuthLayerEncrypt(QVector<Binding> peers, QByteArray cleartextPayload);
    void requestAuthLayerDecrypt(QVector<Binding> peers, QByteArray encryptedPayload);
    void requestAuthCipherEncrypt(Binding peer, QByteArray payload, payloadType type);
    void requestAuthCipherDecrypt(Binding peer, QByteArray payload, payloadType type);
    void requestAuthSessionClose(Binding peer);

private slots:
    void readAuthSessionHS1(QByteArray message);
    void readAuthSessionHS2(QByteArray message);
    void readAuthLayerEncryptResp(QByteArray message);
    void readAuthLayerDecryptResp(QByteArray message);
    void readAuthCipherEncryptResp(QByteArray message);
    void readAuthCipherDecryptResp(QByteArray message);
    void readAuthError(QByteArray message);

private:
    void maybeReconnect();
    void onData();
    quint32 getRequestID();

    quint16 getSessionId(Binding Peer);

    bool checkRequestId(quint16 sessionId, quint32 requestId);

    bool checkRequestId(quint32 requestId);

    struct Hop
    {
        Binding peer;
        QByteArray key;
        quint16 sessionId;
        quint32 last_requestId;
    };

    quint32 requestID =0;

    bool running_ = false;

    QHostAddress host_;

    quint16 port_ = -1;

    QTcpSocket socket_;

    QByteArray buffer_;

    QVector<Hop> Hops;

};



#endif // OAUTHAPI_H
