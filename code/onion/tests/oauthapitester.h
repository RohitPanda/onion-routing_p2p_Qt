#ifndef OAUTHAPITESTER_H
#define OAUTHAPITESTER_H

#include <QObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QTcpServer>
#include <functional>

class OAuthApiTester : public QObject
{
    Q_OBJECT
public:
    explicit OAuthApiTester(QObject *parent = 0);

signals:

public slots:

private slots:


private:
    QTcpServer *tcpServer(QHostAddress addr, quint16 port,
                          std::function<void(QByteArray)> callback,
                          std::function<void(QTcpSocket*)> connection);
};

#endif // OAUTHAPITESTER_H
