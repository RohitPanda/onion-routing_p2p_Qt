#ifndef API_H
#define API_H

#include "binding.h"
#include "messagetypes.h"

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

// represents the local api connection that we host for other modules
class OnionApi : public QObject
{
    Q_OBJECT
public:
    explicit OnionApi(QObject *parent = 0);

    int port() const;
    void setPort(int port);

    QHostAddress interface() const;
    void setInterface(const QHostAddress &interface);

    void setBinding(Binding binding);

    bool start();

    QString socketError() const;

signals:
    void requestBuildTunnel(QHostAddress peer, quint16 port, QByteArray hostkey, QTcpSocket *requestSource);
    void requestDestroyTunnel(quint32 tunnelId);
    void requestSendTunnel(quint32 tunnelId, QByteArray data);
    void requestBuildCoverTunnel(quint16 randomDataLength);

public slots:
    // emits requestDestroyTunnel if requester disconnected during tunnel setup
    void sendTunnelReady(QTcpSocket *requester, quint32 tunnelId, QByteArray hostkey);
    void sendTunnelIncoming(quint32 tunnelId, QByteArray hostkey);
    void sendTunnelData(quint32 tunnelId, QByteArray data);
    void sendTunnelError(quint32 tunnelId, MessageType requestType);

private slots:
    void onConnection();
    void onData(QTcpSocket *socket);

    void readTunnelBuild(QByteArray message, QTcpSocket *client);
    void readTunnelDestroy(QByteArray message, QTcpSocket *client);
    void readTunnelData(QByteArray message, QTcpSocket *client);
    void readTunnelCover(QByteArray message, QTcpSocket *client);

private:
    // internal cleanup
    void onTunnelDestroyed(quint32 tunnelId);

    // data buffers
    QHash<QTcpSocket *, QByteArray> buffers_;

    // tunnel -> client mapping
    QHash<quint32, QTcpSocket *> tunnelMapping_;

    int nClients_ = 0;

    QTcpServer server_;

    QHostAddress interface_;
    int port_;
};

Q_DECLARE_METATYPE(QHostAddress);

#endif // API_H
