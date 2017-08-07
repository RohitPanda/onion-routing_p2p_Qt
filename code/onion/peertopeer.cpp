#include "peertopeer.h"

PeerToPeer::PeerToPeer(QObject *parent) : QObject(parent), socket_(this)
{
    connect(&socket_, &QUdpSocket::readyRead, this, &PeerToPeer::onDatagram);
}

QHostAddress PeerToPeer::interface() const
{
    return interface_;
}

void PeerToPeer::setInterface(const QHostAddress &interface)
{
    interface_ = interface;
}

int PeerToPeer::port() const
{
    return port_;
}

void PeerToPeer::setPort(int port)
{
    port_ = port;
}

bool PeerToPeer::start()
{
    return socket_.bind(port_);
}

void PeerToPeer::onDatagram()
{
    while (socket_.hasPendingDatagrams()) {
        handleDatagram(socket_.receiveDatagram());
    }
}

void PeerToPeer::handleDatagram(QNetworkDatagram datagram)
{
    if(!datagram.isValid()) {
        qDebug() << "error receiving p2p datagram.";
        return;
    }

    qDebug() << "P2P data from" << datagram.senderAddress() << ":" << datagram.senderPort();
    // TODO handle data, figure out if incoming connection or existing communication
}

int PeerToPeer::nHops() const
{
    return nHops_;
}

void PeerToPeer::setNHops(int nHops)
{
    nHops_ = nHops;
}

void PeerToPeer::buildTunnel(QHostAddress destinationAddr, quint16 destinationPort, QTcpSocket *requestId)
{

}

void PeerToPeer::destroyTunnel(quint32 tunnelId)
{

}

bool PeerToPeer::sendData(quint32 tunnelId, QByteArray data)
{

}

void PeerToPeer::coverTunnel(quint16 size)
{

}
