#include "rpsapi.h"

#include <QDataStream>
#include <QTimer>

RPSApi::RPSApi(QObject *parent) : QObject(parent), socket_(this)
{
    connect(&socket_, &QTcpSocket::readyRead, this, &RPSApi::onData);
    connect(&socket_, &QTcpSocket::stateChanged, [=](QAbstractSocket::SocketState state) {
        if(state == QTcpSocket::UnconnectedState) {
            QTimer::singleShot(2000, this, &RPSApi::maybeReconnect);
        }
        if(state == QTcpSocket::ConnectedState) {
            qDebug() << "RPSApi connected";
        }
    });
    connect(&socket_, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error), this, [=]() {
        qDebug() << "RPSApi error:" << socket_.errorString();
    });
}

void RPSApi::setHost(QHostAddress address, quint16 port)
{
    host_ = address;
    port_ = port;
}

void RPSApi::setHost(Binding binding)
{
    host_ = binding.address;
    port_ = binding.port;
}

void RPSApi::start()
{
    running_ = true;
    socket_.connectToHost(host_, port_);
}

void RPSApi::requestPeer()
{
    if(!socket_.isOpen()) {
        qDebug() << "RPS Api unconnected, cannot request peer.";
        return;
    }

    QByteArray message;
    QDataStream stream(&message, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << (quint16)4;
    stream << (quint16)MessageType::RPS_QUERY;

    if(!socket_.write(message)) {
        qDebug() << "Failed to write RPS_QUERY:" << socket_.errorString();
    }
}

void RPSApi::maybeReconnect()
{
    if(running_) {
        socket_.connectToHost(host_, port_);
    }
}

void RPSApi::onData()
{
    buffer_.append(socket_.readAll());

    if(buffer_.size() < 4) {
        return;
    }

    QDataStream stream(&buffer_, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 size, typeInt;
    stream >> size;
    stream >> typeInt;

    if(buffer_.size() < size) {
        return;
    }

    MessageType type = (MessageType)typeInt;
    if(type != MessageType::RPS_PEER) {
        qDebug() << "RPS_API got message" << typeInt << "dropping.";
        buffer_ = buffer_.mid(size);
        return;
    }

    if(size < 12) {
        qDebug() << "Malformed RPS_PEER message";
        buffer_ = buffer_.mid(size);
        return;
    }

    quint16 port, ipV;
    stream >> port;
    stream >> ipV;

    QHostAddress peerAddress;
    QByteArray hostkey;

    bool ipV4 = (ipV & 0x0001) == 0x0001;
    if(ipV4) {
        quint32 ip;
        stream >> ip;
        peerAddress.setAddress(ip);
        hostkey = buffer_.mid(12, size - 12);
    } else {
        if(size < 24) {
            qDebug() << "Malformed RPS_PEER message";
            buffer_ = buffer_.mid(size);
            return;
        }

        quint8 *ip = reinterpret_cast<quint8*>(buffer_.data() + 8);
        peerAddress.setAddress(ip);
        hostkey = buffer_.mid(24, size - 24);
    }

    buffer_ = buffer_.mid(size);
    if(peerAddress.isNull()) {
        qDebug() << "invalid host address in RPS_PEER message";
        return;
    }

    emit onPeer(peerAddress, port, hostkey);
}
