#ifndef SESSIONKEYSTORE_H
#define SESSIONKEYSTORE_H

#include <QHash>
#include "binding.h"

class SessionKeystore
{
public:
    SessionKeystore();

    void set(quint32 tunnelId, quint16 sessionId);
    void remove(quint32 tunnelId);

    quint16 get(quint32 tunnelId);
    bool has(quint32 tunnelId);

private:
    QHash<quint32, quint16> data_;
};

#endif // SESSIONKEYSTORE_H
