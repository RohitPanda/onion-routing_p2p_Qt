#ifndef MOCKOAUTHAPI_H
#define MOCKOAUTHAPI_H

#include "oauthapi.h"
#include <QObject>

class MockOAuthApi : public OAuthApi
{
    Q_OBJECT
public:
    explicit MockOAuthApi(QObject *parent = 0);

signals:

public slots:
    virtual void requestAuthSessionStart(quint32 requestId, QByteArray hostkey) override;
    virtual void requestAuthSessionIncomingHS1(quint32 requestId, QByteArray handshake) override;
    virtual void requestAuthSessionIncomingHS2(quint32 requestId, quint16 sessionId, QByteArray handshake) override;
    virtual void requestAuthLayerEncrypt(quint32 requestId, QVector<quint16> sessionIds, QByteArray payload) override;
    virtual void requestAuthLayerDecrypt(quint32 requestId, QVector<quint16> sessionIds, QByteArray payload) override;
    virtual void requestAuthCipherEncrypt(quint32 requestId, quint16 sessionId, QByteArray payload) override;
    virtual void requestAuthCipherDecrypt(quint32 requestId, quint16 sessionId, QByteArray payload) override;
    virtual void requestAuthSessionClose(quint16 sessionId) override;

private:
    // we even check if session ids make sense
    QHash<quint16, QByteArray> startedIds_; // half-open, missing HS2. sessionId => hostkey
    QSet<quint16> establishedIds_; // established

    int nextSessionId_ = 5;
};

#endif // MOCKOAUTHAPI_H
