#include "oauthapi.h"

#include <QDataStream>
#include <QTimer>

OAuthApi::OAuthApi(QObject *parent) : QObject(parent), socket_(this)
{
    connect(&socket_, &QTcpSocket::readyRead, this, &OAuthApi::onData);
    connect(&socket_, &QTcpSocket::stateChanged, [=](QAbstractSocket::SocketState state) {
        if(state == QTcpSocket::UnconnectedState) {
            QTimer::singleShot(2000, this, &OAuthApi::maybeReconnect);
        }
        if(state == QTcpSocket::ConnectedState) {
            qDebug() << "OAuthApi connected";
        }
    });
    connect(&socket_, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error), this, [=]() {
        qDebug() << "OAuthApi error:" << socket_.errorString();
    });
}

void OAuthApi::setHost(QHostAddress address, quint16 port)
{
    host_ = address;
    port_ = port;
}

void OAuthApi::setHost(Binding binding)
{
    host_ = binding.address;
    port_ = binding.port;
}

void OAuthApi::start()
{
    running_ = true;
    socket_.connectToHost(host_, port_);
}

void OAuthApi::requestAuthSessionStart(quint32 requestId, QByteArray hostkey)
{
    quint16 messageLength = 4 + 4 + 4 + hostkey.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);
    //quint32 requestId = OAuthApi::getRequestID();
    stream << messageLength;
    stream << (quint16)MessageType::AUTH_SESSION_START;
    stream << (quint32)0;
    stream << requestId;
    message.append(hostkey);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_SESSION_START:" << socket_.errorString();
    }
}

void OAuthApi::requestAuthSessionIncomingHS1(quint32 requestId, QByteArray handshake)
{
    quint16 messageLength = 4 + 4 + 4 + handshake.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);
    //quint32 requestId = OAuthApi::getRequestID();
    stream << messageLength;
    stream << (quint16)MessageType::AUTH_SESSION_INCOMING_HS1;
    stream << (quint32)0;
    stream << requestId;
    message.append(handshake);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_SESSION_INCOMING_HS1:" << socket_.errorString();
    }

}

void OAuthApi::requestAuthSessionIncomingHS2(quint32 requestId, quint16 sessionId, QByteArray handshake)
{
    quint16 messageLength = 4 + 4 + 4 + handshake.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    //quint32 requestId = OAuthApi::getRequestID();
    //quint16 sessionId = OAuthApi::getSessionId(peer);

    stream << messageLength;
    stream << (quint16)MessageType::AUTH_SESSION_INCOMING_HS2;
    stream << (quint16)0;
    stream << sessionId;
    stream << requestId;
    message.append(handshake);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_SESSION_INCOMING_HS2:" << socket_.errorString();
    }

}

void OAuthApi::requestAuthLayerEncrypt(quint32 requestId, QVector<quint16> sessionIds, QByteArray cleartextPayload)
{
    quint8 noLayers = sessionIds.size();
    quint16 messageLength = 4 + 4 + 4 + 2*noLayers + cleartextPayload.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    //quint32 requestId = OAuthApi::getRequestID();


    stream << messageLength;
    stream << (quint16)MessageType::AUTH_LAYER_ENCRYPT;
    stream << (quint16)0;
    stream << noLayers;
    stream << (quint8)0;
    stream << requestId;
    for(QVector<quint16>::iterator it = sessionIds.begin(); it != sessionIds.end(); it++)
    {
        stream << *it;
    }
    message.append(cleartextPayload);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_LAYER_ENCRYPT:" << socket_.errorString();
    }

}

void OAuthApi::requestAuthLayerDecrypt(quint32 requestId, QVector<quint16> sessionIds, QByteArray encryptedPayload)
{
    quint8 noLayers = sessionIds.size();
    quint16 messageLength = 4 + 4 + 4 + 2*noLayers + encryptedPayload.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    //quint32 requestId = OAuthApi::getRequestID();


    stream << messageLength;
    stream << (quint16)MessageType::AUTH_LAYER_DECRYPT;
    stream << (quint16)0;
    stream << noLayers;
    stream << (quint8)0;
    stream << requestId;
    for(QVector<quint16>::iterator it = sessionIds.begin(); it != sessionIds.end(); it++)
    {
        stream << *it;
    }

    message.append(encryptedPayload);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_LAYER_DECRYPT:" << socket_.errorString();
    }
}

void OAuthApi::requestAuthCipherEncrypt(quint32 requestId, quint16 sessionId, QByteArray payload)
{
    quint16 messageLength = 4 + 4 + 4 + 2 + payload.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    //quint32 requestId = OAuthApi::getRequestID();
    //quint16 sessionId = OAuthApi::getSessionId(peer);

    stream << messageLength;
    stream << (quint16)MessageType::AUTH_CIPHER_ENCRYPT;
    stream << (quint32)0;
    stream << requestId;
    stream << sessionId;
    message.append(payload);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_CIPHER_ENCRYPT:" << socket_.errorString();
    }
}

void OAuthApi::requestAuthCipherDecrypt(quint32 requestId, quint16 sessionId, QByteArray payload)
{
    quint16 messageLength = 4 + 4 + 4 + 2 + payload.length();

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    //quint32 requestId = OAuthApi::getRequestID();
    //quint16 sessionId = OAuthApi::getSessionId(peer);

    stream << messageLength;
    stream << (quint16)MessageType::AUTH_CIPHER_DECRYPT;
    stream << (quint32)0;
    stream << requestId;
    stream << sessionId;
    message.append(payload);

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_CIPHER_DECRYPT:" << socket_.errorString();
    }

}



void OAuthApi::requestAuthSessionClose(quint16 sessionId)
{
    quint16 messageLength = 4 + 4;

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    //quint32 sessionId = OAuthApi::getSessionId(peer);

    stream << messageLength;
    stream << (quint16)MessageType::AUTH_SESSION_CLOSE;
    stream << (quint16)0;
    stream << sessionId;

    if(!socket_.write(message)) {
        qDebug() << "Failed to write AUTH_SESSION_CLOSE:" << socket_.errorString();
    }

}

void OAuthApi::readAuthSessionHS1(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(6);

    quint16 sessionId;
    quint32 requestId;

    stream >> sessionId;
    stream >> requestId;



    if(checkRequestId(sessionId, requestId))
    {
        QByteArray payload;
        payload = message.mid(12);
        emit recvSessionHS1(requestId, sessionId, payload);
    }

}

void OAuthApi::readAuthSessionHS2(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(6);

    quint16 sessionId;
    quint32 requestId;

    stream >> sessionId;
    stream >> requestId;



    if(checkRequestId(sessionId, requestId))
    {
        QByteArray payload;
        payload = message.mid(12);
        emit recvSessionHS2(requestId, sessionId, payload);
    }

}

void OAuthApi::readAuthLayerEncryptResp(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(8);

    quint32 requestId;

    stream >> requestId;

    if(checkRequestId(requestId))
    {
        quint16 sessionId;
        sessionId = OAuthApi::getSessionId(requestId);
        QByteArray payload;
        payload = message.mid(12);
        emit recvEncrypted(requestId, sessionId, payload);
    }
}

void OAuthApi::readAuthLayerDecryptResp(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(8);

    quint32 requestId;

    stream >> requestId;

    if(checkRequestId(requestId))
    {
        QByteArray payload;
        payload = message.mid(12);
        emit recvDecrypted(requestId, payload);
    }
}

void OAuthApi::readAuthCipherEncryptResp(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(8);

    quint32 requestId;

    stream >> requestId;

    if(checkRequestId(requestId))
    {
        quint16 sessionId;
        sessionId = OAuthApi::getSessionId(requestId);
        QByteArray payload;
        payload = message.mid(12);
        emit recvEncrypted(requestId, sessionId, payload);
    }
}

void OAuthApi::readAuthCipherDecryptResp(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(8);

    quint32 requestId;

    stream >> requestId;

    if(checkRequestId(requestId))
    {
        QByteArray payload;
        payload = message.mid(12);
        emit recvDecrypted(requestId, payload);
    }
}

void OAuthApi::readAuthError(QByteArray message)
{
    QDataStream stream(message);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.skipRawData(6);

    quint16 sessionId;
    quint32 requestId;

    stream >> sessionId;
    stream >> requestId;



    if(checkRequestId(sessionId, requestId))
    {

    }
}

void OAuthApi::onData()
{
    buffer_.append(socket_.readAll());

    if(buffer_.size() < 4) {
        // wait for more data
        return;
    }

    QDataStream stream(&buffer_, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 size, messageTypeInt;
    stream >> size;
    stream >> messageTypeInt;

    qDebug() << "got data from" << socket_.peerAddress() << "->" << size
             << "bytes, type" << messageTypeInt << buffer_.size() << "bytes in buffer";

    if(size > buffer_.size()) {
        // wait for more data
        return;
    }

    // take message
    QByteArray message = buffer_.mid(0, size);
    buffer_.remove(0, size);
    MessageType type = (MessageType)messageTypeInt;

    switch (type) {

    case MessageType::AUTH_SESSION_HS1:
        readAuthSessionHS1(message);
        break;

    case MessageType::AUTH_SESSION_INCOMING_HS2:
        readAuthSessionHS2(message);
        break;

    case MessageType::AUTH_LAYER_ENCRYPT_RESP:
        readAuthLayerEncryptResp(message);
        break;

    case MessageType::AUTH_LAYER_DECRYPT_RESP:
        readAuthLayerDecryptResp(message);
        break;

    case MessageType::AUTH_CIPHER_ENCRYPT_RESP:
        readAuthCipherEncryptResp(message);
        break;

    case MessageType::AUTH_CIPHER_DECRYPT_RESP:
        readAuthCipherDecryptResp(message);
        break;

    case MessageType::AUTH_ERROR:
        readAuthError(message);
        qDebug() << "oauth-api: cannot command a onion signal-type ->" << messageTypeInt << "discarding message";
        break;

    default:
        qDebug() << "oauth-api: discarding message of invalid type" << messageTypeInt;
        break;
    }
}

quint32 OAuthApi::getRequestID()
{
    if(OAuthApi::requestID < 65535)
    {
        OAuthApi::requestID++;
    }
    else
    {
        OAuthApi::requestID = 0;
    }
    return OAuthApi::requestID;
}

quint16 OAuthApi::getSessionId(Binding Peer)
{
    for(QVector<Hop>::iterator it = OAuthApi::Hops.begin(); it != OAuthApi::Hops.end(); it++)
    {
        if(it->peer == Peer)
            return it->sessionId;
    }
    return 0;
}

quint16 OAuthApi::getSessionId(quint32 requestId)
{
    for(QVector<Hop>::iterator it = OAuthApi::Hops.begin(); it != OAuthApi::Hops.end(); it++)
    {
        if(it->last_requestId == requestId)
            return it->sessionId;
    }
    return 0;
}

bool OAuthApi::checkRequestId(quint32 requestId)
{
//    for(QVector<Hop>::iterator it = OAuthApi::Hops.begin(); it != OAuthApi::Hops.end(); it++)
//    {
//        if(it->last_requestId == requestId)
//            return true;
//    }

    //return false;
    return true;
}

bool OAuthApi::checkRequestId(quint16 sessionId, quint32 requestId)
{
    for(QVector<Hop>::iterator it = OAuthApi::Hops.begin(); it != OAuthApi::Hops.end(); it++)
    {
        if(it->sessionId == sessionId)
            return (it->last_requestId == requestId);
    }

    return false;
}

void OAuthApi::maybeReconnect()
{
    if(running_) {
        socket_.connectToHost(host_, port_);
    }
}
