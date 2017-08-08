#ifndef PEERTOPEER_H
#define PEERTOPEER_H

#include <QNetworkDatagram>
#include <QObject>
#include <QUdpSocket>
#include <QTcpSocket>

#include "binding.h"
#include "messagetypes.h"
#include "peertopeermessage.h"

// represents the UDP best effort connection to other onion modules.
class PeerToPeer : public QObject
{
    Q_OBJECT
public:
    explicit PeerToPeer(QObject *parent = 0);

    QHostAddress interface() const;
    void setInterface(const QHostAddress &interface);

    int port() const;
    void setPort(int port);

    bool start();

    int nHops() const;
    void setNHops(int nHops);
    
public slots:
    void buildTunnel(QHostAddress destinationAddr, quint16 destinationPort, QTcpSocket *requestId);
    void destroyTunnel(quint32 tunnelId);
    bool sendData(quint32 tunnelId, QByteArray data);
    void coverTunnel(quint16 size);

signals:
    void tunnelReady(QTcpSocket *requestId, quint32 tunnelId, QByteArray hostkey);
    void tunnelIncoming(quint32 tunnelId, QByteArray hostkey);
    void tunnelData(quint32 tunnelId, QByteArray data);
    void tunnelError(quint32 tunnelId, MessageType lastMessage);

private:    // structs
    enum HopStatus {
        Unconnected,
        BuildSent,
        Created,
        Error
    };

    struct HopState {
        QByteArray ourHandshake;
        QByteArray peerHostkey;
        Binding peer;
        HopStatus status = Unconnected;
        // TODO
    };

    // state of a circuit (src==us, a, b, ..., dest)
    struct CircuitState {
        QVector<HopState> hopStates;

        QByteArray buffer;
    };

    struct TunnelState {
        Binding peer;
        Binding nextHop;

        quint32 tunnelIdPeer = 0;
        quint32 tunnelIdNextHop = 0;

        bool hasNextHop() const { return nextHop.isValid(); }
    };

private slots:
    void onDatagram();
    void handleDatagram(QNetworkDatagram datagram);

    void handleCircuitMessage(PeerToPeerMessage message, CircuitState *circuit);
    void handleTunnelMessage(PeerToPeerMessage message, TunnelState *tunnel);
    void handleUnknownMessage(PeerToPeerMessage message);

private:

    // we're source here, tunnelId is src<->a
    QHash<quint32, CircuitState> circuits_;
    // we're in the path, thus two valid tunnelIds: a <-> us <-> b
    // saved inside TunnelState
    QList<TunnelState> tunnels_;
    quint32 nextTunnelId_ = 1;

    QUdpSocket socket_;

    QHostAddress interface_;
    int port_;
    int nHops_ = 2;
};

#endif // PEERTOPEER_H
