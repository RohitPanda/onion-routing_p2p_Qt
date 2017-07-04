#include "tests/onionapitester.h"
#include <QTest>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto tests = QList<QObject*>({
        new OnionApiTester()
    });

    bool ok;
    for(auto object : tests) {
        ok &= QTest::qExec(object, argc, argv);
    }
    return ok ? 0 : -1;
}
