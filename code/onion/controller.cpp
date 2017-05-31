#include "controller.h"

Controller::Controller(QString configFile) : settings_(configFile)
{

}

void Controller::start()
{
    settings_.load();

    // TODO get parameters, setup api and p2p
    // TODO make api for RPS, get peers and establish connection to them
}
