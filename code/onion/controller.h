#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QTcpSocket>
#include "onionapi.h"
#include "peertopeer.h"
#include "settings.h"
#include "oauthapi.h"
#include "rpsapi.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QString configFile);

    bool start();

    void setOverrideHost(QHostAddress address);
    void setOverridePort(int port);
    void setMockPeers(QList<Binding> peers);
    void setMockOauth(bool enable);
    void setMarcoPolo(Binding marco, bool polo);
    void setVerbose(bool v);

private:
    bool mockRPS() const { return !mockPeers_.isEmpty(); }
    bool marcoPolo() const { return marco_.isValid() || polo_; }
    void setupMarcoPolo();

    QByteArray readHostkey(QString file);

    OnionApi onionApi_;
    PeerToPeer p2p_;
    RPSApi rpsApi_;
    OAuthApi *oAuthApi_ = nullptr;

    PeerSampler *rpsApiProxy_ = nullptr;

    QByteArray hostkey_;

    QString settingsFile_;
    Settings settings_;

    QHostAddress overrideHost_;
    int overridePort_ = -1;
    QList<Binding> mockPeers_;
    bool mockOAuth_ = false;
    Binding marco_;
    bool polo_ = false;
    bool verbose_ = false;
};

#endif // CONTROLLER_H
