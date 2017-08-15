#include "mockoauthapi.h"

MockOAuthApi::MockOAuthApi(QObject *parent) : OAuthApi(parent)
{

}

void MockOAuthApi::requestAuthSessionStart(quint32 requestId, QByteArray hostkey)
{
    quint16 sessionId = nextSessionId_++;
    startedIds_[sessionId] = hostkey;

    QByteArray handshake = hostkey;
    handshake.append("_hs1");

    QTimer::singleShot(0, [=]() {
        recvSessionHS1(requestId, sessionId, handshake);
    });
}

void MockOAuthApi::requestAuthSessionIncomingHS1(quint32 requestId, QByteArray handshake)
{
    // directly established
    quint16 sessionId = nextSessionId_++;
    establishedIds_.insert(sessionId);

    handshake.append(" -> HS2");

    QTimer::singleShot(0, [=]() {
        recvSessionHS2(requestId, sessionId, handshake);
    });
}

void MockOAuthApi::requestAuthSessionIncomingHS2(quint32 requestId, quint16 sessionId, QByteArray handshake)
{
    // finish handshake, no answer
    Q_UNUSED(requestId);
    Q_UNUSED(handshake);
    if(!startedIds_.contains(sessionId)) {
        qDebug() << "MockOAuth: IncomingHS2 for sessionId" << sessionId << "which was never started";
    } else {
        startedIds_.remove(sessionId);
    }
    establishedIds_.insert(sessionId);
}

void MockOAuthApi::requestAuthLayerEncrypt(quint32, QVector<quint16>, QByteArray)
{
    qDebug() << "MockOAuthApi::requestAuthLayerEncrypt not implemented";
}

void MockOAuthApi::requestAuthLayerDecrypt(quint32, QVector<quint16>, QByteArray)
{
    qDebug() << "MockOAuthApi::requestAuthLayerDecrypt not implemented";
}

void MockOAuthApi::requestAuthCipherEncrypt(quint32 requestId, quint16 sessionId, QByteArray payload)
{
    // actual work o.0
    // encrypt == digest++
    if(payload.size() != 1024) {
        qDebug() << "requestAuthCipherEncrypt payload size is" << payload.size() << "expected 1024.";
    }

    if(!establishedIds_.contains(sessionId)) {
        qDebug() << "requestAuthCipherEncrypt on un-established session id. It is "
                 << (startedIds_.contains(sessionId) ? QString("initialized") : QString("unknown"));
    }

    // layout is | cmd | digest | streamid
    // -> bytes 1-5 is digest
    // since our max hoplength is <256, we just increment/decrement the first byte, which should be fine
    payload[1] = payload[1] + 1;
    QTimer::singleShot(0, [=]() {
        recvEncrypted(requestId, sessionId, payload);
    });
}

void MockOAuthApi::requestAuthCipherDecrypt(quint32 requestId, quint16 sessionId, QByteArray payload)
{
    // actual work o.0
    // encrypt == digest--
    if(payload.size() != 1024) {
        qDebug() << "requestAuthCipherDecrypt payload size is" << payload.size() << "expected 1024.";
    }

    if(!establishedIds_.contains(sessionId)) {
        qDebug() << "requestAuthCipherDecrypt on un-established session id. It is "
                 << (startedIds_.contains(sessionId) ? QString("initialized") : QString("unknown"));
    }

    // layout is | cmd | digest | streamid
    // -> bytes 1-5 is digest
    // since our max hoplength is <256, we just increment/decrement the first byte, which should be fine
    payload[1] = payload[1] - 1;
    QTimer::singleShot(0, [=]() {
        recvDecrypted(requestId, payload);
    });
}

void MockOAuthApi::requestAuthSessionClose(quint16 sessionId)
{
    if(!startedIds_.contains(sessionId) && !establishedIds_.contains(sessionId)) {
        qDebug() << "requestAuthSessionClose on unknown session.";
    }

    startedIds_.remove(sessionId);
    establishedIds_.remove(sessionId);
}



