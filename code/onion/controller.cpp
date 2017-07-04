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

    // TODO get parameters, setup api and p2p
    // TODO make api for RPS, get peers and establish connection to them
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
