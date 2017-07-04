#ifndef API_H
#define API_H

#include "binding.h"

#include <QObject>
#include <QTcpServer>

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
    void requestBuildTunnel(QHostAddress peer, quint16 port, QByteArray hostkey);
    void requestDestroyTunnel(quint32 tunnelId);
    void requestSendTunnel(quint32 tunnelId, QByteArray data);
    void requestBuildCoverTunnel(quint16 randomDataLength);

public slots:

private slots:
    void onConnection();
    void onData(QTcpSocket *socket);

    void readTunnelBuild(QByteArray message, QTcpSocket *client);
    void readTunnelDestroy(QByteArray message, QTcpSocket *client);
    void readTunnelData(QByteArray message, QTcpSocket *client);
    void readTunnelCover(QByteArray message, QTcpSocket *client);

private:
    // data buffers
    QHash<QTcpSocket *, QByteArray> buffers_;

    int nClients_ = 0;

    QTcpServer server_;

    QHostAddress interface_;
    int port_;
};

Q_DECLARE_METATYPE(QHostAddress);

#endif // API_H
