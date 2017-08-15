#include "controller.h"
#include "mockoauthapi.h"
#include "marcopolo.h"

#include <QFileInfo>
#include <QDir>
#include "mockpeersampler.h"

Controller::Controller(QString configFile) :
    settingsFile_(configFile), settings_(configFile)
{

}

bool Controller::start()
{
    bool settingsOk = settings_.load();
    if(!settingsOk) {
        qDebug() << "errors in settings files, see above";
        return false;
    }

    settings_.dump();

    // validate our hostkey
    hostkey_ = readHostkey(settings_.hostkeyFile());

    // setup onion api
    onionApi_.setBinding(settings_.onionApiAddress());

    // setup rps api
    rpsApi_.setHost(settings_.rpsApiAddress().address, settings_.rpsApiAddress().port);

    // setup oauth api
    if(mockOAuth_) {
        oAuthApi_ = new MockOAuthApi(this);
    } else {
        oAuthApi_ = new OAuthApi(this);
        oAuthApi_->setHost(settings_.authApiAddress().address, settings_.authApiAddress().port);
    }

    // setup peersampler
    if(mockRPS()) {
        MockPeerSampler *mps = new MockPeerSampler(this);
        mps->setMockPeers(mockPeers_);
        rpsApiProxy_ = mps;
    } else {
        rpsApiProxy_ = new PeerSampler(this);
        rpsApiProxy_->setRpsApi(&rpsApi_);
    }

    // setup p2p
    Binding p2pAddr = settings_.p2pAddress();
    if(!overrideHost_.isNull()) {
        p2pAddr.address = overrideHost_;
    }
    if(overridePort_ > 0 && overridePort_ <= 65535) {
        p2pAddr.port = overridePort_;
    }

    p2p_.setInterface(p2pAddr.address);
    p2p_.setPort(p2pAddr.port);
    p2p_.setNHops(2);

    // connect to rps api
    p2p_.setPeerSampler(rpsApiProxy_);

    // connect to onion api
    connect(&onionApi_, &OnionApi::requestBuildTunnel, &p2p_, &PeerToPeer::buildTunnel);
    connect(&onionApi_, &OnionApi::requestDestroyTunnel, &p2p_, &PeerToPeer::destroyTunnel);
    connect(&onionApi_, &OnionApi::requestSendTunnel, &p2p_, &PeerToPeer::sendData);
    connect(&onionApi_, &OnionApi::requestBuildCoverTunnel, &p2p_, &PeerToPeer::coverTunnel);
    connect(&p2p_, &PeerToPeer::tunnelReady, &onionApi_, &OnionApi::sendTunnelReady);
    connect(&p2p_, &PeerToPeer::tunnelIncoming, &onionApi_, &OnionApi::sendTunnelIncoming);
    connect(&p2p_, &PeerToPeer::tunnelData, &onionApi_, &OnionApi::sendTunnelData);
    connect(&p2p_, &PeerToPeer::tunnelError, &onionApi_, &OnionApi::sendTunnelError);

    // connect to oauth api
    connect(oAuthApi_, &OAuthApi::recvSessionHS1, &p2p_, &PeerToPeer::onSessionHS1);
    connect(oAuthApi_, &OAuthApi::recvSessionHS2, &p2p_, &PeerToPeer::onSessionHS2);
    connect(oAuthApi_, &OAuthApi::recvEncrypted, &p2p_, &PeerToPeer::onEncrypted);
    connect(oAuthApi_, &OAuthApi::recvDecrypted, &p2p_, &PeerToPeer::onDecrypted);

    connect(&p2p_, &PeerToPeer::requestEncrypt, oAuthApi_, &OAuthApi::requestAuthCipherEncrypt);
    connect(&p2p_, &PeerToPeer::requestDecrypt, oAuthApi_, &OAuthApi::requestAuthCipherDecrypt);
    connect(&p2p_, &PeerToPeer::requestStartSession, oAuthApi_, &OAuthApi::requestAuthSessionStart);
    connect(&p2p_, &PeerToPeer::sessionIncomingHS1, oAuthApi_, &OAuthApi::requestAuthSessionIncomingHS1);
    connect(&p2p_, &PeerToPeer::sessionIncomingHS2, oAuthApi_, &OAuthApi::requestAuthSessionIncomingHS2);
    connect(&p2p_, &PeerToPeer::requestEndSession, oAuthApi_, &OAuthApi::requestAuthSessionClose);
    connect(&p2p_, &PeerToPeer::requestLayeredEncrypt, oAuthApi_, &OAuthApi::requestAuthLayerEncrypt);
    connect(&p2p_, &PeerToPeer::requestLayeredDecrypt, oAuthApi_, &OAuthApi::requestAuthLayerDecrypt);

    setupMarcoPolo();

//    bool debugOnion = true;
//    if(debugOnion) {
//        connect(&onionApi_, &OnionApi::requestBuildCoverTunnel, [=](quint16 b) {
//            qDebug() << "OnionApi::requestBuildCoverTunnel(" << b << ")";
//        });
//        connect(&onionApi_, &OnionApi::requestBuildTunnel, [=](QHostAddress peer, quint16 port, QByteArray hostkey) {
//            qDebug().noquote() << "OnionApi::requestBuildTunnel(" << peer.toString() << ":" << port << ")"
//                               << "\nkey:" << hostkey.toHex();
//        });
//        connect(&onionApi_, &OnionApi::requestDestroyTunnel, [=](quint32 tunnelId) {
//            qDebug() << "OnionApi::requestDestroyTunnel(" << tunnelId << ")";
//        });
//        connect(&onionApi_, &OnionApi::requestSendTunnel, [=](quint32 tunnelId, QByteArray data) {
//            qDebug() << "OnionApi::requestSendTunnel(" << tunnelId << ")\ndata:" << data.toHex();
//        });
//    }

    // start everything
    if(onionApi_.start()) {
        qDebug() << "onion api running on" << settings_.onionApiAddress().toString();
    } else {
        qDebug() << "could not start onion api:" << onionApi_.socketError();
        return false;
    }

    if(p2p_.start()) {
        qDebug() << "p2p running on" << settings_.onionApiAddress().toString();
    } else {
        qDebug() << "could not start p2p";
        return false;
    }

    if(!mockOAuth_) {
        oAuthApi_->start();
    }

    if(!mockRPS()) {
        rpsApi_.start();
    }

    return true;
}

void Controller::setOverrideHost(QHostAddress address)
{
    overrideHost_ = address;
}

void Controller::setOverridePort(int port)
{
    overridePort_ = port;
}

void Controller::setMockPeers(QList<Binding> peers)
{
    mockPeers_ = peers;
}

void Controller::setMockOauth(bool enable)
{
    mockOAuth_ = enable;
}

void Controller::setMarcoPolo(Binding marco, bool polo)
{
    marco_ = marco;
    polo_ = polo;
}

void Controller::setupMarcoPolo()
{
    if(marco_.isValid() || polo_) {
        MarcoPolo *marcopolo = new MarcoPolo(this);
        marcopolo->setMarco(marco_);
        marcopolo->setPolo(polo_);

        // marcopolo connects onionapi-lik
        connect(marcopolo, &MarcoPolo::requestBuildTunnel, &p2p_, &PeerToPeer::buildTunnel);
        connect(marcopolo, &MarcoPolo::requestDestroyTunnel, &p2p_, &PeerToPeer::destroyTunnel);
        connect(marcopolo, &MarcoPolo::requestSendTunnel, &p2p_, &PeerToPeer::sendData);
        connect(marcopolo, &MarcoPolo::requestBuildCoverTunnel, &p2p_, &PeerToPeer::coverTunnel);
        connect(&p2p_, &PeerToPeer::tunnelReady, marcopolo, &MarcoPolo::onTunnelReady);
        connect(&p2p_, &PeerToPeer::tunnelIncoming, marcopolo, &MarcoPolo::onTunnelIncoming);
        connect(&p2p_, &PeerToPeer::tunnelData, marcopolo, &MarcoPolo::onTunnelData);
        connect(&p2p_, &PeerToPeer::tunnelError, marcopolo, &MarcoPolo::onTunnelError);

        marcopolo->start();
    }
}

QByteArray Controller::readHostkey(QString file)
{
    QStringList checked;

    QFileInfo info(file);
    checked.append(info.absoluteFilePath());
    if(!info.exists()) {
        // not absolute, not relative to executable directory
        if(info.isRelative()) {
            // check relative to config file location
            QDir configDir = QFileInfo(settingsFile_).dir();
            info.setFile(configDir.filePath(file));
            checked.append(info.absoluteFilePath());
        }
    }

    if(!info.exists()) {
        qDebug() << "Did not find host key file from" << file << "; checked " << checked.join(" | ");
        return QByteArray();
    }

    QFile f(info.absoluteFilePath());
    if(!f.open(QFile::ReadOnly)) {
        qDebug() << "Cannot open" << info.absoluteFilePath() << ":" << f.errorString();
        return QByteArray();
    }

    return f.readAll();
}
