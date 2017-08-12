#include "peersampler.h"

PeerSampler::PeerSampler(QObject *parent) : QObject(parent), timer_(this)
{
    timer_.setInterval(500);
    timer_.setSingleShot(false);
    connect(&timer_, &QTimer::timeout, this, &PeerSampler::requestLoop);
    timer_.start();
}

int PeerSampler::requestPeers(int n)
{
    if(n == 0) {
        return -1;
    }

    PeerList request;
    request.nRequested = n;
    request.requestId = nextId_++;
    pendingPeers_.append(request);
    return request.requestId;
}

void PeerSampler::setRpsApi(RPSApi *api)
{
    connect(api, &RPSApi::onPeer, this, PeerSampler::onPeer);
    connect(this, &PeerSampler::requestPeer, api, &RPSApi::requestPeer);
}

void PeerSampler::onPeer(QHostAddress address, quint16 port, QByteArray hostkey)
{
    if(pendingPeers_.isEmpty()) {
        return;
    }

    PeerList &request = pendingPeers_.first();
    Peer peer;
    peer.address = Binding(address, port);
    peer.hostkey = hostkey;
    request.peers.append(peer);
    if(request.peers.size() == request.nRequested) {
        peersArrived(request.requestId, request.peers);
        pendingPeers_.removeAt(0);
    }
}

void PeerSampler::requestLoop()
{
    if(!pendingPeers_.isEmpty()) {
        requestPeer();
    }
}
