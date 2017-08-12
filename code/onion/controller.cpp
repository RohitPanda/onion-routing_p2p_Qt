#include "controller.h"

#include <QFileInfo>
#include <QDir>

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
    // TODO

    // setup peersampler
    rpsApiProxy_.setRpsApi(&rpsApi_);

    // setup p2p
    Binding p2pAddr = settings_.p2pAddress();
    p2p_.setInterface(p2pAddr.address);
    p2p_.setPort(p2pAddr.port);
    p2p_.setNHops(2);

    // connect to rps api
    p2p_.setPeerSampler(&rpsApiProxy_);

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
    // TODO


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

    return true;
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
