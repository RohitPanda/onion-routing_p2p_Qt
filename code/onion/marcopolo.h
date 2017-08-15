#ifndef MARCOPOLO_H
#define MARCOPOLO_H

#include "binding.h"
#include "messagetypes.h"
#include "metatypes.h"

#include <QObject>
#include <QTcpSocket>

class MarcoPolo : public QObject
{
    Q_OBJECT
public:
    explicit MarcoPolo(QObject *parent = 0);

    void setMarco(Binding peer);
    void setPolo(bool enable);

    void start();

signals:
    void requestBuildTunnel(QHostAddress peer, quint16 port, QByteArray hostkey, QTcpSocket *requestSource);
    void requestDestroyTunnel(quint32 tunnelId);
    void requestSendTunnel(quint32 tunnelId, QByteArray data);
    void requestBuildCoverTunnel(quint16 randomDataLength);

public slots:
    void onTunnelReady(QTcpSocket *requester, quint32 tunnelId, QByteArray hostkey);
    void onTunnelIncoming(quint32 tunnelId);
    void onTunnelData(quint32 tunnelId, QByteArray data);
    void onTunnelError(quint32 tunnelId, MessageType requestType);

private:
    void marco();

    Binding peer_;
    bool polo_ = false;

    quint32 poloTunnelId = 0;
};

#endif // MARCOPOLO_H
