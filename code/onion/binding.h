#ifndef BINDING_H
#define BINDING_H

#include <QHostAddress>

struct Binding {
    QHostAddress address;
    quint16 port = 0;

    QString toString() const { return QString("%1:%2").arg(address.toString(), QString::number(port)); }
};



#endif // BINDING_H
