#ifndef PEERTOPEER_H
#define PEERTOPEER_H

#include <QNetworkDatagram>
#include <QObject>
#include <QUdpSocket>
#include <QTcpSocket>

#include "binding.h"
#include "messagetypes.h"
#include "peertopeermessage.h"
#include "sessionkeystore.h"

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
    // from OnionApi
    void buildTunnel(QHostAddress destinationAddr, quint16 destinationPort, QTcpSocket *requestId);
    void destroyTunnel(quint32 tunnelId);
    bool sendData(quint32 tunnelId, QByteArray data);
    void coverTunnel(quint16 size);

    // from AuthApi
    void onEncrypted(quint32 requestId, quint16 sessionId, QByteArray payload);
    void onDecrypted(quint32 requestId, QByteArray payload);

    void onSessionHS1(quint32 requestId, quint16 sessionId, QByteArray handshake);
    void onSessionHS2(quint32 requestId, quint16 sessionId, QByteArray handshake);

signals:
    // for OnionApi
    void tunnelReady(QTcpSocket *requestId, quint32 tunnelId, QByteArray hostkey);
    void tunnelIncoming(quint32 tunnelId, QByteArray hostkey);
    void tunnelData(quint32 tunnelId, QByteArray data);
    void tunnelError(quint32 tunnelId, MessageType lastMessage);

    // for AuthApi
    void requestEncrypt(quint32 requestId, quint16 sessionId, QByteArray payload);
    void requestDecrypt(quint32 requestId, quint16 sessionId, QByteArray payload);

    void requestStartSession(quint32 requestId, QByteArray hostkey);
    void sessionIncomingHS1(quint32 requestId, QByteArray handshake);
    void sessionIncomingHS2(quint32 requestId, quint16 sessionId, QByteArray handshake);

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

        quint16 sessionKey; // with this peer
        // TODO
    };

    // state of a circuit (src==us, a, b, ..., dest)
    struct CircuitState {
        QVector<HopState> hopStates;

        QByteArray buffer;
    };

    struct TunnelState {
        Binding previousHop;
        Binding nextHop;

        quint32 tunnelIdPreviousHop = 0;
        quint32 tunnelIdNextHop = 0;

        bool hasNextHop() const { return nextHop.isValid(); }
    };

    struct OnionAuthRequest {
        // save additional data to payload here
        enum ReqType {
            LayeredDecrypt,
            LayeredEncrypt,
            DecryptOnce,
            EncryptOnce
        };

        ReqType type;
        QByteArray preface; // unencrypted original header of the packet
        QVector<HopState> remainingHops; // remaining for encryption/decryption
        quint32 tunnelId; // original tunnelid

        Binding peer; // source
        Binding nextHop; // dest - if applicable
        quint32 nextHopTunnelId; // dest - if applicable
        int operations = 0; // number of decrypts/encrypts on this request
    };
private slots:
    void onDatagram();
    void handleDatagram(QNetworkDatagram datagram);

    void handleBuild(const QNetworkDatagram &datagram, PeerToPeerMessage message);
    void handleCreated(const QNetworkDatagram &datagram, PeerToPeerMessage message);

    // it is actually for us and decrypted
    void handleMessage(PeerToPeerMessage message);

    void continueLayeredDecrypt(OnionAuthRequest request, QByteArray payload);
    void continueLayeredEncrypt(OnionAuthRequest request, QByteArray payload);

    // todo shouldn't I be inside PeerToPeerMessage?
    void forwardEncryptedMessage(Binding to, quint32 tunnelId, QByteArray payload);
private:
    // maps a peer to our auth session id with that peer
    SessionKeystore sessions_;

    // all hashed by requestId as sent to auth
    QHash<quint32, OnionAuthRequest> encryptQueue_;
    QHash<quint32, OnionAuthRequest> decryptQueue_;
    QHash<quint32, Binding> incomingTunnels_;
    quint32 nextAuthRequest_ = 1;

    // we're source here, tunnelId is src<->a
    QHash<quint32, CircuitState> circuits_;
    // we're in the path, thus two valid tunnelIds: a <-> us <-> b
    QList<TunnelState> tunnels_;
    quint32 nextTunnelId_ = 1;

    QUdpSocket socket_;

    QHostAddress interface_;
    int port_;
    int nHops_ = 2;
};

#endif // PEERTOPEER_H
