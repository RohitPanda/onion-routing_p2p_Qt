#ifndef MOCKPEERSAMPLER_H
#define MOCKPEERSAMPLER_H

#include "peersampler.h"
#include <QObject>

class MockPeerSampler : public PeerSampler
{
    Q_OBJECT
public:
    explicit MockPeerSampler(QObject *parent = 0);

    virtual int requestPeers(int n) override;

    void setMockPeers(QList<Binding> peers);

private:
    void serveNext();

    int nextId_ = 1;
    QList<Binding> mockPeers_;
    QMap<int, int> requests_; // id => n

};

#endif // MOCKPEERSAMPLER_H
