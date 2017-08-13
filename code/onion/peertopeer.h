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
#include "tunnelidmapper.h"
#include "peersampler.h"

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

    void setPeerSampler(PeerSampler *sampler);
    
public slots:
    // from OnionApi
    void buildTunnel(QHostAddress destinationAddr, quint16 destinationPort, QByteArray hostkey, QTcpSocket *requestId);
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
    void tunnelIncoming(quint32 tunnelId);
    void tunnelData(quint32 tunnelId, QByteArray data);
    void tunnelError(quint32 tunnelId, MessageType lastMessage);

    // for AuthApi
    void requestEncrypt(quint32 requestId, quint16 sessionId, QByteArray payload);
    void requestDecrypt(quint32 requestId, quint16 sessionId, QByteArray payload);

    void requestLayeredEncrypt(quint32 requestId, QVector<quint16> sessionIds, QByteArray payload);
    void requestLayeredDecrypt(quint32 requestId, QVector<quint16> sessionIds, QByteArray payload);


    void requestStartSession(quint32 requestId, QByteArray hostkey);
    void sessionIncomingHS1(quint32 requestId, QByteArray handshake);
    void sessionIncomingHS2(quint32 requestId, quint16 sessionId, QByteArray handshake);

    void requestEndSession(quint16 session);

private:    // structs
    enum HopStatus {
        Unconnected,
        BuildSent,
        Created,
        Error
    };

    struct HopState {
        QByteArray peerHostkey;
        QByteArray peerHandshakeHS1; // us -> him
        Binding peer;
        quint16 circuitId = 0;
        quint32 tunnelId = 0;

        HopStatus status = Unconnected;

        quint16 sessionKey; // with this peer
    };

    // state of a circuit (src==us, a, b, ..., dest)
    struct CircuitState {
        QVector<HopState> hopStates;

        quint32 circuitApiTunnelId; // is the tunnelId for last hop.
        MessageType lastMessage;

        int remainingCoverData = 0; // if this is a cover tunnel
        QTcpSocket *requesterId = nullptr;
    };

    struct TunnelState {
        Binding previousHop;
        Binding nextHop;

        quint16 circIdPreviousHop = 0;
        quint16 circIdNextHop = 0;

        quint32 tunnelIdPreviousHop = 0;
        quint32 tunnelIdNextHop = 0;

        bool hasNextHop() const { return nextHop.isValid(); }
        bool operator ==(const TunnelState &other) const;
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
        QVector<HopState> remainingHops; // remaining for encryption/decryption
        quint16 circuitId; // original circid

        Binding peer; // source
        Binding nextHop; // dest - if applicable
        quint16 nextHopCircuitId; // dest - if applicable
        int operations = 0; // number of decrypts/encrypts on this request

        QString debugString;
    };

    struct PeerSample {
        bool isBuildTunnel = false;
        PeerSampler::Peer dest;
        quint16 coverTrafficBytes;
        QTcpSocket *requesterId = nullptr;
    };

    struct BuildTunnelPeer {
        Binding peer;
        QByteArray hostkey;
        QByteArray handshake;
        quint32 authRequestId = 0;
        quint16 sessionId;
    };

    struct CircuitHandshakes {
        bool isBuildTunnel = false;
        quint16 coverTrafficBytes;
        QList<BuildTunnelPeer> peers;
        QTcpSocket *requesterId = nullptr;
    };

private slots:
    void onDatagram();
    void handleDatagram(QNetworkDatagram datagram);

    void handleBuild(PeerToPeerMessage message);
    void handleCreated(PeerToPeerMessage message);

    // it is actually for us and decrypted
    void handleMessage(PeerToPeerMessage message, quint32 originatorTunnelId);

    void continueLayeredDecrypt(OnionAuthRequest request, QByteArray payload);
    void continueLayeredEncrypt(OnionAuthRequest request, QByteArray payload);

    void forwardEncryptedMessage(Binding to, quint16 circuitId, QByteArray payload);
    void sendPeerToPeerMessage(PeerToPeerMessage unencrypted, Binding target);
    void sendPeerToPeerMessage(PeerToPeerMessage unencrypted, QVector<HopState> tunnel);

    void tearCircuit(quint32 tunnelId); // sends destroy messages along the circuit
    void cleanCircuit(quint32 tunnelId); // cleans up resources

    void peersArrived(int id, QList<PeerSampler::Peer> peers);
    void continueBuildingTunnel(quint32 id);
    void sendCoverData(quint32 tunnelId);

    void disconnectPeer(Binding who);
private:
    TunnelIdMapper tunnelIds_;

    // maps a peer to our auth session id with that peer
    SessionKeystore sessions_;

    // all hashed by requestId as sent to auth
    QHash<quint32, OnionAuthRequest> encryptQueue_;
    QHash<quint32, OnionAuthRequest> decryptQueue_;
    QHash<quint32, quint32> incomingTunnels_; // hashes auth reqId -> tunnelId
    quint32 nextAuthRequest_ = 1;

    // hashes tunnelId of CREATED message to tunnelId of previous hop for a relay_extend
    QHash<quint32, quint32> pendingTunnelExtensions_;

    QHash<int, PeerSample> pendingPeerSamples_;
    QList<CircuitHandshakes> pendingCircuitHandshakes_;

    // we're source here, tunnelId is src<->a
    // tunnelId propagated to API is src<->dst!!
    QHash<quint32, CircuitState> circuits_;
    // we're in the path, thus two valid tunnelIds: a <-> us <-> b
    QList<TunnelState> tunnels_;
    quint32 nextTunnelId_ = 1;

    TunnelState *findTunnelByPreviousHopId(quint32 tId);
    TunnelState *findTunnelByNextHopId(quint32 tId);

    QUdpSocket socket_;

    QHostAddress interface_;
    int port_;
    int nHops_ = 2;

    PeerSampler *peerSampler_ = nullptr;
};

#endif // PEERTOPEER_H
