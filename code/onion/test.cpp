#include "tests/onionapitester.h"
#include "tests/rpsapitester.h"
#include "tests/peertopeermessagetester.h"
#include "tests/oauthapitester.h"
#include <QTest>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // meta type setup
    qRegisterMetaType<QHostAddress>();

    auto tests = QList<QObject*>({
         new OnionApiTester(),
         new RPSApiTester(),
         new PeerToPeerMessageTester(),
         new OAuthApiTester()
    });

    bool ok = true;
    for(auto object : tests) {
        int no_failed = QTest::qExec(object, argc, argv);
        ok &= no_failed == 0;
    }
    return ok ? 0 : -1;
}
