#ifndef TUNNELIDMAPPER_H
#define TUNNELIDMAPPER_H

#include <QHash>
#include "binding.h"

class TunnelIdMapper
{
public:
    TunnelIdMapper();

    struct CircuitBinding {
        CircuitBinding() {}
        CircuitBinding(Binding b, quint16 c) : binding(b), circId(c) {}

        Binding binding;
        quint16 circId;
    };

    quint32 tunnelId(Binding binding, quint16 circId);
    quint32 tunnelId(CircuitBinding id);
    quint16 nextCircId(Binding binding);
    void decompose(quint32 tunnelId, Binding *outBinding, quint16 *outCircId);

    QString describe(quint32 tunnelId);

private:
    QHash<Binding, quint16> nextCircIds_;
    QHash<quint32, CircuitBinding> backward_;
    QHash<CircuitBinding, quint32> forward_;
    quint32 nextTunnelId_ = 10;
};

#endif // TUNNELIDMAPPER_H
