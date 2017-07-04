#include "settings.h"

Settings::Settings(QString filename) : settings_(filename, QSettings::IniFormat)
{

}

bool Settings::load()
{
    bool ok = true;
    settings_.sync();

    // TODO figure out exact config format
    // as in https://gitlab.lrz.de/voidphone/testing/blob/master/config/bootstrap.conf
    settings_.beginGroup("onion");

    hostkeyFile_ = settings_.value("hostkey").toString();

    QString tmp = settings_.value("listen_address").toString();
    ok &= readBinding(tmp, &p2pAddress_, "[onion]->listen_address");

    tmp = settings_.value("api_address").toString();
    ok &= readBinding(tmp, &onionApiAddress_, "[onion]->api_address");

    settings_.endGroup();

    ok &= readBinding(settings_.value("rps/api_address").toString(), &rpsApiAddress_, "[rps]->api_address");
    ok &= readBinding(settings_.value("auth/api_address").toString(), &authApiAddress_, "[auth]->api_address");

    return ok;
}

bool Settings::readBinding(QString str, Binding *binding, QString errorPos) const
{
    int idx = str.lastIndexOf(':'); // last because ipv6
    if(idx == -1) {
        qDebug() << str << "is invalid, use <ipaddr>:<port>. Check" << errorPos;
        return false;
    }

    QString ip = str.mid(0, idx);
    QString port = str.mid(idx + 1); // safe against out of bounds

    QHostAddress addr(ip);
    if(addr.isNull()) {
        qDebug() << ip << "is not a valid IP Address. Check" << errorPos;
        return false;
    }

    bool ok;
    int intPort = port.toInt(&ok);
    if(!ok || intPort <= 0 || intPort > 65535) {
        qDebug() << port << "is not a valid port. Check" << errorPos;
        return false;
    }

    binding->address = addr;
    binding->port = (quint16)intPort;
    return true;
}

QString Settings::hostkeyFile() const
{
    return hostkeyFile_;
}

void Settings::dump() const
{
    qDebug() << "Read configuration as:\n";
    qDebug() << "\t[onion]/hostkey:" << hostkeyFile_;
    qDebug() << "\t[onion]/listen_address:" << p2pAddress_.toString();
    qDebug() << "\t[onion]/api_address:" << onionApiAddress_.toString();
    qDebug() << "\t[rps]/api_address:" << rpsApiAddress_.toString();
    qDebug() << "\t[auth]/api_address:" << authApiAddress_.toString();
    qDebug() << "\n";
}

Binding Settings::authApiAddress() const
{
    return authApiAddress_;
}

Binding Settings::rpsApiAddress() const
{
    return rpsApiAddress_;
}

Binding Settings::onionApiAddress() const
{
    return onionApiAddress_;
}

Binding Settings::p2pAddress() const
{
    return p2pAddress_;
}
