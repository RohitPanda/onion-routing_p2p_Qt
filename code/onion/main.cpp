#include <QCoreApplication>
#include "controller.h"

#define ASSERT_ARG() if(args.isEmpty()) { qDebug() << "ran out of args"; parseOk = false; break; }

void printUsage()
{
    qDebug() << "onion -c [configfile]";
    qDebug() << "options: --mock-peer <ip>:<port>  to fake peers instead of an rps module";
    qDebug() << "         --mock-auth              to use fake onion auth instead of module";
    qDebug() << "         --marco <ip>:<port>      to connect to <peer> and send marco messages";
    qDebug() << "         --polo                   to listen for marco messages and send back polos";
    qDebug() << "         --host <ip>              override config onion p2p host with <ip>";
    qDebug() << "         --port <port>            override config onion p2p port with <port>";
}

Binding parse(QString peer) {
    int idx = peer.lastIndexOf(':');
    if(idx == -1) {
        qDebug() << "invalid peer address after. Syntax: <address>:<port>";
        return Binding();
    } else {
        QString host = peer.mid(0, idx);
        QString port = peer.mid(idx + 1);
        QHostAddress add(host);
        int p = port.toInt();
        if(add.isNull()) {
            qDebug() << "invalid host address." << host << "is not an ip address";
            return Binding();
        } else if(p <= 0 || p > 65535) {
            qDebug() << "invalid port" << port;
            return Binding();
        } else {
            return Binding(add, p);
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // meta type setup
    qRegisterMetaType<QHostAddress>();


    QStringList args = a.arguments();
    args.removeFirst(); // program name

    bool parseOk = true;

    QString configfile;
    QList<Binding> mockPeers;
    bool mockOAuth = false;
    Binding marco;
    bool polo = false;
    QHostAddress overrideAddress;
    int overridePort = -1;

    while(!args.isEmpty()) {
        QString arg = args.takeFirst();
        if(arg == "-c" && !args.isEmpty()) {
            ASSERT_ARG();
            configfile = args.takeFirst();
        } else if(arg == "--mock-peer") {
            ASSERT_ARG();
            Binding peer = parse(args.takeFirst());
            if(peer.isValid()) {
                mockPeers.append(peer);
            } else {
                parseOk = false;
            }
        } else if(arg == "--mock-auth") {
            mockOAuth = true;
        } else if(arg == "--marco") {
            ASSERT_ARG();
            Binding peer = parse(args.takeFirst());
            if(peer.isValid()) {
                marco = peer;
            } else {
                parseOk = false;
            }
        } else if(arg == "--polo") {
            polo = true;
        } else if(arg == "--host") {
            ASSERT_ARG();
            overrideAddress.setAddress(args.takeFirst());
        } else if(arg == "--port") {
            ASSERT_ARG();
            overridePort = args.takeFirst().toInt();
        }
    }

    parseOk = parseOk && !configfile.isEmpty();
    if(!parseOk) {
        printUsage();
        return -1;
    }

    qDebug() << "loading config from" << configfile;

    Controller controller(configfile);

    if(!overrideAddress.isNull()) {
        controller.setOverrideHost(overrideAddress);
    }
    if(overridePort > 0 && overridePort <= 65535) {
        controller.setOverridePort(overridePort);
    }
    controller.setMarcoPolo(marco, polo);
    controller.setMockOauth(mockOAuth);
    controller.setMockPeers(mockPeers);

    if(!controller.start()) {
        return -1;
    }

    return a.exec();
}
