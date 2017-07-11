#include "tests/onionapitester.h"
#include "tests/rpsapitester.h"
#include <QTest>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // meta type setup
    qRegisterMetaType<QHostAddress>();

    auto tests = QList<QObject*>({
         new OnionApiTester(),
         new RPSApiTester()
    });

    bool ok;
    for(auto object : tests) {
        ok &= QTest::qExec(object, argc, argv);
    }
    return ok ? 0 : -1;
}
