#include "settings.h"

Settings::Settings(QString filename) : settings_(filename, QSettings::IniFormat)
{

}

void Settings::load()
{
    settings_.sync();

    // TODO figure out exact config format
    settings_.beginGroup("onion");
    p2pInterface_ = QHostAddress();
    settings_.endGroup();
}
