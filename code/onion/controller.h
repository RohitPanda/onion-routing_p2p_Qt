#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QTcpSocket>
#include "onionapi.h"
#include "peertopeer.h"
#include "settings.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QString configFile);

    bool start();

private:
    void readHostkey(QString file);

    OnionApi onionApi_;
    PeerToPeer p2p_;

    QByteArray hostkey_;

    QString settingsFile_;
    Settings settings_;
};

#endif // CONTROLLER_H
