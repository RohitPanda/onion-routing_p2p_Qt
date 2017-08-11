#include "tunnelidmapper.h"

TunnelIdMapper::TunnelIdMapper()
{

}

quint32 TunnelIdMapper::tunnelId(Binding binding, quint16 circId)
{
    return tunnelId(CircuitBinding(binding, circId));
}

quint32 TunnelIdMapper::tunnelId(TunnelIdMapper::CircuitBinding id)
{
    if(!forward_.contains(id)) {
        forward_[id] = nextTunnelId_++;
    }

    return forward_[id];
}

quint16 TunnelIdMapper::nextCircId(Binding binding)
{
    if(!nextCircIds_.contains(binding)) {
        nextCircIds_[binding] = 5;
    }

    return nextCircIds_[binding]++;
}

void TunnelIdMapper::decompose(quint32 tunnelId, Binding *outBinding, quint16 *outCircId)
{
    if(!backward_.contains(tunnelId)) {
        qDebug() << "decompose with nonexisting tunnelid";
        *outBinding = Binding();
        *outCircId = 0;
        return;
    }

    CircuitBinding id = backward_[tunnelId];
    *outBinding = id.binding;
    *outCircId = id.circId;
}

QString TunnelIdMapper::describe(quint32 tunnelId)
{
    if(!backward_.contains(tunnelId)) {
        return "<invalid tunnelid>";
    }

    CircuitBinding id = backward_[tunnelId];
    return QString("%1@x%2").arg(id.binding.toString(), QString::number(id.circId, 16).rightJustified(4, '0'));
}

uint qHash(TunnelIdMapper::CircuitBinding key)
{
    return qHash(key.binding) ^ qHash(key.circId);
}

bool TunnelIdMapper::CircuitBinding::operator ==(const TunnelIdMapper::CircuitBinding &other) const
{
    return binding == other.binding && circId == other.circId;
}
