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

void OAuthApi::requestAuthSessionStart(Binding peer, QByteArray key)
{

}

void OAuthApi::requestAuthSessionIncomingHS1(QByteArray hsp1)
{

}

void OAuthApi::requestAuthSessionIncomingHS2(QByteArray hsp2)
{

}

void OAuthApi::requestAuthLayerEncrypt(QVector<Binding> peers, QByteArray cleartextPayload)
{

}

void OAuthApi::requestAuthLayerDecrypt(QVector<Binding> peers, QByteArray encryptedPayload)
{

}

void OAuthApi::requestAuthCipherEncrypt(Binding peer, QByteArray payload, quint16 payload_type)
{

}

void OAuthApi::requestAuthCipherDecrypt(Binding peer, QByteArray payload, quint16 payload_type)
{

}

void OAuthApi::requestAuthSessionClose(Binding peer)
{

}

void OAuthApi::readAuthSessionHS1()
{

}

void OAuthApi::readAuthSessionHS2()
{

}

void OAuthApi::readAuthLayerEncryptResp()
{

}

void OAuthApi::readAuthLayerDecryptResp()
{

}

void OAuthApi::readAuthCipherEncryptResp()
{

}

void OAuthApi::readAuthCipherDecryptResp()
{

}

void OAuthApi::readAuthError()
{

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
        readAuthSessionHS1();
        break;

    case MessageType::AUTH_SESSION_INCOMING_HS2:
        readAuthSessionHS2();
        break;

    case MessageType::AUTH_LAYER_ENCRYPT_RESP:
        readAuthLayerEncryptResp();
        break;

    case MessageType::AUTH_LAYER_DECRYPT_RESP:
        readAuthLayerDecryptResp();
        break;

    case MessageType::AUTH_CIPHER_ENCRYPT_RESP:
        readAuthCipherEncryptResp();
        break;

    case MessageType::AUTH_CIPHER_DECRYPT_RESP:
        readAuthCipherDecryptResp();
        break;

    case MessageType::AUTH_ERROR:
        readAuthError();
        qDebug() << "oauth-api: cannot command a onion signal-type ->" << messageTypeInt << "discarding message";
        break;

    default:
        qDebug() << "oauth-api: discarding message of invalid type" << messageTypeInt;
        break;
    }
}

quint16 OAuthApi::getSessionId(Binding Peer)
{

}

bool OAuthApi::checkRequestId(quint16 sessionId, quint16 requestId)
{

}

void OAuthApi::maybeReconnect()
{
    if(running_) {
        socket_.connectToHost(host_, port_);
    }
}
