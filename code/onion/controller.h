#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "onionapi.h"
#include "peertopeer.h"
#include "settings.h"

class Controller
{
public:
    Controller(QString configFile);

    bool start();

private:
    void readHostkey(QString file);

    OnionApi api_;
    PeerToPeer p2p_;

    QByteArray hostkey_;

    QString settingsFile_;
    Settings settings_;
};

#endif // CONTROLLER_H
