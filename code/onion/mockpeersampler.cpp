#include "mockpeersampler.h"

MockPeerSampler::MockPeerSampler(QObject *parent) : PeerSampler(parent)
{

}

int MockPeerSampler::requestPeers(int n)
{
    if(n > mockPeers_.size()) {
        qDebug() << "mock peer sampler: requested" << n << "peers, but I have only" << mockPeers_.size();
    }

    int id = nextId_++;
    requests_[id] = n;

    QTimer::singleShot(0, this, &MockPeerSampler::serveNext);

    return id;
}

void MockPeerSampler::setMockPeers(QList<Binding> peers)
{
    mockPeers_ = peers;
}

void MockPeerSampler::serveNext()
{
    if(mockPeers_.isEmpty()) {
        qDebug() << "cannot serve mock peers. I have none.";
        return;
    }

    int id = requests_.firstKey();
    int n = requests_.take(id);

    QList<Peer> peers;
    for(int i = 0; i < n; i++) {
        Peer peer;
        peer.address = mockPeers_[i % mockPeers_.size()];
        peer.hostkey = QString("peerkey_%1").arg(QString::number(i)).toLatin1();
        peers.append(peer);
    }

    peersArrived(id, peers);
}
