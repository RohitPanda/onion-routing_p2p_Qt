#ifndef PEERTOPEER_H
#define PEERTOPEER_H

#include <QNetworkDatagram>
#include <QObject>
#include <QUdpSocket>

// represents the UDP best effort connection to other onion modules.
class PeerToPeer : public QObject
{
    Q_OBJECT
public:
    explicit PeerToPeer(QObject *parent = 0);

    QHostAddress interface() const;
    void setInterface(const QHostAddress &interface);

    int port() const;
    void setPort(int port);

    bool start();

    // TODO establishing connections to other peers

signals:

private slots:
    void onDatagram();
    void handleDatagram(QNetworkDatagram datagram);

private:
    QUdpSocket socket_;

    QHostAddress interface_;
    int port_;
};

#endif // PEERTOPEER_H
