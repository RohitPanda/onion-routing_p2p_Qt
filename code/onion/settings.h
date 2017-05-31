#ifndef SETTINGS_H
#define SETTINGS_H

#include <QHostAddress>
#include <QSettings>

class Settings
{
public:
    Settings(QString filename);

    void load();

private:
    QSettings settings_;

    QHostAddress p2pInterface_;
    int p2pPort_;
};

#endif // SETTINGS_H
