#ifndef API_H
#define API_H

#include <QObject>
#include <QTcpServer>

// represents the local api connection that we host for other modules
class Api : public QObject
{
    Q_OBJECT
public:
    explicit Api(QObject *parent = 0);

    int port() const;
    void setPort(int port);

    QHostAddress interface() const;
    void setInterface(const QHostAddress &interface);

    bool start();

signals:

public slots:

private slots:
    void onConnection();
    void onData(QTcpSocket *socket);

private:
    // data buffers
    QHash<QTcpSocket *, QByteArray> buffers_;

    QTcpServer server_;

    QHostAddress interface_;
    int port_;
};

#endif // API_H
