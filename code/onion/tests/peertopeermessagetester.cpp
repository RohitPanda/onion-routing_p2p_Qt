#include "peertopeermessagetester.h"

PeerToPeerMessageTester::PeerToPeerMessageTester(QObject *parent) : QObject(parent)
{

}

void PeerToPeerMessageTester::testCmdBuild()
{
    PeerToPeerMessage message;
    message.tunnelId = 19201;
    message.celltype = PeerToPeerMessage::CMD_BUILD;

    message.data = QByteArray("SRC-OR1-HOSTKEY");

    verifyWritePayload(message, QByteArray::fromHex("00004b0102000F5352432d4f52312d484f53544b4559"));

    // backwards:
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("0005e34402000F4f52312d5352432d484f53544b4559"));

    QCOMPARE(out.tunnelId, (quint32)385860);
    QCOMPARE(out.celltype, PeerToPeerMessage::CMD_BUILD);
    QCOMPARE(out.data, QByteArray("OR1-SRC-HOSTKEY"));
}

void PeerToPeerMessageTester::testCmdCreated()
{
    PeerToPeerMessage message;
    message.tunnelId = 263527;
    message.celltype = PeerToPeerMessage::CMD_CREATED;
    message.data = QByteArray("OR1-SRC-HOSTKEY");

    verifyWritePayload(message, QByteArray::fromHex("0004056703000F4f52312d5352432d484f53544b4559"));

    // backwards:
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("0004056703000F4f52312d5352432d484f53544b4559"));

    QCOMPARE(out.tunnelId, (quint32)263527);
    QCOMPARE(out.celltype, PeerToPeerMessage::CMD_CREATED);
    QCOMPARE(out.data, QByteArray("OR1-SRC-HOSTKEY"));
}

void PeerToPeerMessageTester::testCmdDestroy()
{
    PeerToPeerMessage message;
    message.tunnelId = 111623;
    message.celltype = PeerToPeerMessage::CMD_DESTROY;

    verifyWritePayload(message, QByteArray::fromHex("0001B40704"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("0001B40704"));

    QCOMPARE(out.tunnelId, (quint32)111623);
    QCOMPARE(out.celltype, PeerToPeerMessage::CMD_DESTROY);
}

void PeerToPeerMessageTester::testCmdCover()
{
    PeerToPeerMessage message;
    message.tunnelId = 111623;
    message.celltype = PeerToPeerMessage::CMD_COVER;

    verifyWritePayload(message, QByteArray::fromHex("0001B40705"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("0001B40705"));
    QCOMPARE(out.tunnelId, (quint32)111623);
    QCOMPARE(out.celltype, PeerToPeerMessage::CMD_COVER);
}

void PeerToPeerMessageTester::testRelayData()
{
    PeerToPeerMessage message;
    message.tunnelId = 5879;
    message.celltype = PeerToPeerMessage::RELAY;
    message.rcmd = PeerToPeerMessage::RELAY_DATA;
    message.streamId = 17;
    message.data = "HALLO123";
    message.calculateDigest();

    verifyWritePayload(message, QByteArray::fromHex("000016f70101001100000000000848414c4c4f313233"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("000005390101001100000000000848414c4c4f343536"));

    QCOMPARE(out.tunnelId, (quint32)1337);
    QCOMPARE(out.celltype, PeerToPeerMessage::RELAY);
    QCOMPARE(out.rcmd, PeerToPeerMessage::RELAY_DATA);
    QCOMPARE(out.streamId, (quint16)17);
    QVERIFY(out.isValidDigest());
    QCOMPARE(out.data, QByteArray("HALLO456"));
}

void PeerToPeerMessageTester::testRelayExtend4()
{
    PeerToPeerMessage message;
    message.tunnelId = 5879;
    message.celltype = PeerToPeerMessage::RELAY;
    message.rcmd = PeerToPeerMessage::RELAY_EXTEND;
    message.streamId = 17;
    message.port = 8080;
    message.address.setAddress(QHostAddress::LocalHost);
    message.data = "HOSTKEY_";
    message.calculateDigest();

    verifyWritePayload(message, QByteArray::fromHex("000016f70102001100000000047f0000011f900008484f53544b45595f"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("000016f70102001100000000047f0000011f900008484f53544b45595f"));

    QCOMPARE(out.tunnelId, (quint32)5879);
    QCOMPARE(out.celltype, PeerToPeerMessage::RELAY);
    QCOMPARE(out.rcmd, PeerToPeerMessage::RELAY_EXTEND);
    QCOMPARE(out.streamId, (quint16)17);
    QVERIFY(out.isValidDigest());
    QCOMPARE(out.data, QByteArray("HOSTKEY_"));
    QCOMPARE(out.address.protocol(), QAbstractSocket::IPv4Protocol);
    QCOMPARE(out.address, QHostAddress(QHostAddress::LocalHost));
    QCOMPARE(out.port, (quint16)8080);
}

void PeerToPeerMessageTester::testRelayExtend6()
{
    PeerToPeerMessage message;
    message.tunnelId = 5879;
    message.celltype = PeerToPeerMessage::RELAY;
    message.rcmd = PeerToPeerMessage::RELAY_EXTEND;
    message.streamId = 17;
    message.port = 8080;
    message.address.setAddress(QHostAddress::LocalHostIPv6);
    message.data = "HOSTKEY_";
    message.calculateDigest();

    verifyWritePayload(message, QByteArray::fromHex("000016f7010200110000000006000000000000000000000000000000011f900008484f53544b45595f"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("000016f7010200110000000006000000000000000000000000000000011f900008484f53544b45595f"));

    QCOMPARE(out.tunnelId, (quint32)5879);
    QCOMPARE(out.celltype, PeerToPeerMessage::RELAY);
    QCOMPARE(out.rcmd, PeerToPeerMessage::RELAY_EXTEND);
    QCOMPARE(out.streamId, (quint16)17);
    QVERIFY(out.isValidDigest());
    QCOMPARE(out.data, QByteArray("HOSTKEY_"));
    QCOMPARE(out.address.protocol(), QAbstractSocket::IPv6Protocol);
    QCOMPARE(out.address, QHostAddress(QHostAddress::LocalHostIPv6));
    QCOMPARE(out.port, (quint16)8080);
}

void PeerToPeerMessageTester::testRelayExtended()
{
    PeerToPeerMessage message;
    message.tunnelId = 5879;
    message.celltype = PeerToPeerMessage::RELAY;
    message.rcmd = PeerToPeerMessage::RELAY_EXTENDED;
    message.streamId = 17;
    message.data = "HALLO123";
    message.calculateDigest();

    verifyWritePayload(message, QByteArray::fromHex("000016f70103001100000000000848414c4c4f313233"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("000005390103001100000000000848414c4c4f343536"));

    QCOMPARE(out.tunnelId, (quint32)1337);
    QCOMPARE(out.celltype, PeerToPeerMessage::RELAY);
    QCOMPARE(out.rcmd, PeerToPeerMessage::RELAY_EXTENDED);
    QCOMPARE(out.streamId, (quint16)17);
    QVERIFY(out.isValidDigest());
    QCOMPARE(out.data, QByteArray("HALLO456"));
}

void PeerToPeerMessageTester::testRelayTruncated()
{
    PeerToPeerMessage message;
    message.tunnelId = 5879;
    message.celltype = PeerToPeerMessage::RELAY;
    message.rcmd = PeerToPeerMessage::RELAY_TRUNCATED;
    message.streamId = 17;
    message.calculateDigest();

    verifyWritePayload(message, QByteArray::fromHex("000016f70104001100000000"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("000005390104001100000000"));

    QCOMPARE(out.tunnelId, (quint32)1337);
    QCOMPARE(out.celltype, PeerToPeerMessage::RELAY);
    QCOMPARE(out.rcmd, PeerToPeerMessage::RELAY_TRUNCATED);
    QCOMPARE(out.streamId, (quint16)17);
    QVERIFY(out.isValidDigest());
}

void PeerToPeerMessageTester::verifyWritePayload(PeerToPeerMessage message, QByteArray expectedPayload)
{
    int size = expectedPayload.size();
    QByteArray arr;
    QDataStream stream(&arr, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << message;

    QByteArray payload = arr.left(size);
    QByteArray padding = arr.mid(size);
    QCOMPARE(arr.size(), MESSAGE_LENGTH);
    QCOMPARE(payload, expectedPayload);
    for(int i = 0; i < padding.size(); i++) {
        QCOMPARE(padding.at(i), '?');
    }

}

PeerToPeerMessage PeerToPeerMessageTester::verifyReadPayload(QByteArray payload)
{
    payload.append(QByteArray(MESSAGE_LENGTH - payload.size(), '?')); // pad
    QDataStream stream(payload);
    PeerToPeerMessage out;
    stream >> out;

    QTest::qVerify(stream.atEnd(), "", "Stream did not consume full packet", __FILE__, __LINE__);

    return out;
}
