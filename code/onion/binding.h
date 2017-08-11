#ifndef BINDING_H
#define BINDING_H

#include <QHostAddress>
#include <QHash>

struct Binding {
    Binding() { }
    Binding(QHostAddress a, quint16 p) : address(a), port(p) { }

    QHostAddress address;
    quint16 port = 0;

    QString toString() const { return QString("%1:%2").arg(address.toString(), QString::number(port)); }
    bool isValid() const { return !address.isNull() && port != 0; }

    bool operator ==(const Binding &other) const {
        return address == other.address && port == other.port;
    }
};


inline uint qHash(Binding key) { return qHash(key.address) ^ qHash(key.port); }


#endif // BINDING_H
