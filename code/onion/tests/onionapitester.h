#ifndef ONIONAPITESTER_H
#define ONIONAPITESTER_H

#include <QObject>

class OnionApiTester : public QObject
{
    Q_OBJECT
public:
    explicit OnionApiTester(QObject *parent = 0);

private slots:
    // test methods here
    void testBuildTunnel();

};

#endif // ONIONAPITESTER_H