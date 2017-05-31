#include <QCoreApplication>
#include "controller.h"

void printUsage()
{
    qDebug() << "onion -c [configfile]";
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QStringList args = a.arguments();
    args.removeFirst(); // program name

    // TODO better argument parsing?
    bool parseOk = false;

    QString configfile;
    while(!args.isEmpty()) {
        QString arg = args.takeFirst();
        if(arg == "-c" && !args.isEmpty()) {
            configfile = args.takeFirst();
            parseOk = true;
        }
    }

    if(!parseOk) {
        printUsage();
        return -1;
    }

    qDebug() << "loading config from" << configfile;

    Controller controller(configfile);
    controller.start();

    return a.exec();
}
