#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "api.h"
#include "peertopeer.h"
#include "settings.h"

class Controller
{
public:
    Controller(QString configFile);

    void start();

private:
    Api api_;
    PeerToPeer p2p_;

    Settings settings_;
};

#endif // CONTROLLER_H
