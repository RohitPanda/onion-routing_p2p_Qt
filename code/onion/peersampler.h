#ifndef PEERSAMPLER_H
#define PEERSAMPLER_H

#include <QObject>
#include <QTimer>
#include "rpsapi.h"

// aggregates multiple peers from a RPS Api and delivers them back
class PeerSampler : public QObject
{
    Q_OBJECT
public:
    explicit PeerSampler(QObject *parent = 0);

    int requestPeers(int n);
    void setRpsApi(RPSApi *api);

    struct Peer {
        Binding address;
        QByteArray hostkey;
    };
signals:
    void peersArrived(int id, QList<Peer> peers);
    void requestPeer();

private slots:
    void onPeer(QHostAddress address, quint16 port, QByteArray hostkey);
    void requestLoop();

private:
    struct PeerList {
        QList<Peer> peers;
        int nRequested;
        int requestId;
    };

    QList<PeerList> pendingPeers_;
    int nextId_ = 1;

    QTimer timer_;
};

#endif // PEERSAMPLER_H
