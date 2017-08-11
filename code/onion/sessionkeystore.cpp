#include "sessionkeystore.h"

SessionKeystore::SessionKeystore()
{

}

void SessionKeystore::set(quint32 tunnelId, quint16 sessionId)
{
    data_[tunnelId] = sessionId;
}

void SessionKeystore::remove(quint32 tunnelId)
{
    data_.remove(tunnelId);
}

quint16 SessionKeystore::get(quint32 tunnelId)
{
    return data_.value(binding);
}

bool SessionKeystore::has(quint32 tunnelId)
{
    return data_.contains(tunnelId);
}
