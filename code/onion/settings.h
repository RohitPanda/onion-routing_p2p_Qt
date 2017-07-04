#ifndef SETTINGS_H
#define SETTINGS_H

#include <QHostAddress>
#include <QSettings>
#include "binding.h"

class Settings
{
public:
    Settings(QString filename);

    bool load();

    Binding p2pAddress() const;
    Binding onionApiAddress() const;
    Binding rpsApiAddress() const;
    Binding authApiAddress() const;
    QString hostkeyFile() const;

    void dump() const;
private:
    bool readBinding(QString str, Binding *binding, QString errorPos) const;

    QSettings settings_;

    Binding p2pAddress_;
    Binding onionApiAddress_;
    Binding rpsApiAddress_;
    Binding authApiAddress_;
    QString hostkeyFile_;
};

#endif // SETTINGS_H
