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

    QByteArray data = datagram.data();
    if(data.size() != MESSAGE_LENGTH) {
        qDebug() << "P2P data with invalid length" << data.size() << "should be" << MESSAGE_LENGTH;
        // TODO close connection to peer
    }

    qDebug() << "P2P data from" << datagram.senderAddress() << ":" << datagram.senderPort();

    // peek a P2PMessage
    QDataStream stream(data);
    PeerToPeerMessage message;
    peekMessage(stream, message);

    if(message.malformed) {
        qDebug() << "P2P malformed message from" << datagram.senderAddress() << ":"
                 << datagram.senderPort() << "; closing connection.";
        // TODO close connection to peer
    }

    // look up tunnelId in our circuits
    if(circuits_.contains(message.tunnelId)) {
        CircuitState &ref = circuits_[message.tunnelId];
        handleCircuitMessage(message, &ref);
        return;
    }

    // look up tunnelId in our tunnels
    for(QList<TunnelState>::iterator it = tunnels_.begin(); it != tunnels_.end(); it++) {
        if(it->tunnelIdPeer == message.tunnelId || it->tunnelIdNextHop == message.tunnelId) {
            handleTunnelMessage(message, &*it);
            return;
        }
    }

    // not found, could be a new incoming tunnel
    handleUnknownMessage(message);
}

void PeerToPeer::handleCircuitMessage(PeerToPeerMessage message, CircuitState *circuit)
{

}

void PeerToPeer::handleTunnelMessage(PeerToPeerMessage message, PeerToPeer::TunnelState *tunnel)
{

}

void PeerToPeer::handleUnknownMessage(PeerToPeerMessage message)
{

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
    return true;
}

void PeerToPeer::coverTunnel(quint16 size)
{

}
