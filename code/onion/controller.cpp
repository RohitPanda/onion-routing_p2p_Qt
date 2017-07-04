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
    readHostkey(settings_.hostkeyFile());

    // setup onion api
    onionApi_.setBinding(settings_.onionApiAddress());

    // TODO setup p2p

    bool debugOnion = true;
    if(debugOnion) {
        connect(&onionApi_, &OnionApi::requestBuildCoverTunnel, [=](quint16 b) {
            qDebug() << "OnionApi::requestBuildCoverTunnel(" << b << ")";
        });
        connect(&onionApi_, &OnionApi::requestBuildTunnel, [=](QHostAddress peer, quint16 port, QByteArray hostkey) {
            qDebug().noquote() << "OnionApi::requestBuildTunnel(" << peer.toString() << ":" << port << ")"
                               << "\nkey:" << hostkey.toHex();
        });
        connect(&onionApi_, &OnionApi::requestDestroyTunnel, [=](quint32 tunnelId) {
            qDebug() << "OnionApi::requestDestroyTunnel(" << tunnelId << ")";
        });
        connect(&onionApi_, &OnionApi::requestSendTunnel, [=](quint32 tunnelId, QByteArray data) {
            qDebug() << "OnionApi::requestSendTunnel(" << tunnelId << ")\ndata:" << data.toHex();
        });
    }

    // TODO make api for RPS
    // TODO get peers from RPS

    // start everything
    if(onionApi_.start()) {
        qDebug() << "onion api running on" << settings_.onionApiAddress().toString();
    } else {
        qDebug() << "could not start onion api:" << onionApi_.socketError();
        return false;
    }

    // TODO start p2p
    // TODO connect p2p

    return true;
}

void Controller::readHostkey(QString file)
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
        return;
    }

    QFile f(info.absoluteFilePath());
    if(!f.open(QFile::ReadOnly)) {
        qDebug() << "Cannot open" << info.absoluteFilePath() << ":" << f.errorString();
        return;
    }

    hostkey_ = f.readAll();
}
