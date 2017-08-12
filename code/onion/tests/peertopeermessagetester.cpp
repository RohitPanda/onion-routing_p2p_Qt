#include "peertopeermessagetester.h"

PeerToPeerMessageTester::PeerToPeerMessageTester(QObject *parent) : QObject(parent)
{

}

void PeerToPeerMessageTester::testCmdBuild()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeBuild(768, QByteArray("SRC-OR1-HOSTKEY"));

    verifyWritePayload(message, QByteArray::fromHex("010300000F5352432d4f52312d484f53544b4559"));

    // backwards:
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("010300000F4f52312d5352432d484f53544b4559"));

    QCOMPARE(out.circuitId, (quint16)768);
    QCOMPARE(out.celltype, PeerToPeerMessage::BUILD);
    QCOMPARE(out.data, QByteArray("OR1-SRC-HOSTKEY"));
}

void PeerToPeerMessageTester::testCmdCreated()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeCreated(768, QByteArray("OR1-SRC-HOSTKEY"));

    verifyWritePayload(message, QByteArray::fromHex("020300000F4f52312d5352432d484f53544b4559"));

    // backwards:
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("020300000F4f52312d5352432d484f53544b4559"));

    QCOMPARE(out.circuitId, (quint16)768);
    QCOMPARE(out.celltype, PeerToPeerMessage::CREATED);
    QCOMPARE(out.data, QByteArray("OR1-SRC-HOSTKEY"));
}

void PeerToPeerMessageTester::testCmdDestroy()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeCommandDestroy(3840);

    verifyWritePayload(message, QByteArray::fromHex("030F0005000000000000"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F0005000000000000"));

    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::CMD_DESTROY);
}

void PeerToPeerMessageTester::testCmdCover()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeCommandCover(3840);

    verifyWritePayload(message, QByteArray::fromHex("030F0007000000000000"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F0007000000000000"));
    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::CMD_COVER);
}

void PeerToPeerMessageTester::testRelayData()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeRelayData(3840, 1792, "HALLO123");

    verifyWritePayload(message, QByteArray::fromHex("030F0001000000000700000848414c4c4f313233"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F0001000000000700000848414c4c4f313233"));

    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::RELAY_DATA);
    QCOMPARE(out.streamId, (quint16)1792);
    QCOMPARE(out.data, QByteArray("HALLO123"));
}

void PeerToPeerMessageTester::testRelayExtend4()
{
    Binding target(QHostAddress::LocalHost, 8080);
    PeerToPeerMessage message = PeerToPeerMessage::makeRelayExtend(3840, 4352, target, "HOSTKEY_");

    verifyWritePayload(message, QByteArray::fromHex("030F0002000000001100047f0000011f900008484f53544b45595f"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F0002000000001100047f0000011f900008484f53544b45595f"));

    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::RELAY_EXTEND);
    QCOMPARE(out.streamId, (quint16)4352);
    QCOMPARE(out.data, QByteArray("HOSTKEY_"));
    QCOMPARE(out.address.protocol(), QAbstractSocket::IPv4Protocol);
    QCOMPARE(out.address, QHostAddress(QHostAddress::LocalHost));
    QCOMPARE(out.port, (quint16)8080);
}

void PeerToPeerMessageTester::testRelayExtend6()
{
    Binding target(QHostAddress::LocalHostIPv6, 8080);
    PeerToPeerMessage message = PeerToPeerMessage::makeRelayExtend(3840, 4352, target, "HOSTKEY_");

    verifyWritePayload(message, QByteArray::fromHex("030F000200000000110006000000000000000000000000000000011f900008484f53544b45595f"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F000200000000110006000000000000000000000000000000011f900008484f53544b45595f"));

    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::RELAY_EXTEND);
    QCOMPARE(out.streamId, (quint16)4352);
    QCOMPARE(out.data, QByteArray("HOSTKEY_"));
    QCOMPARE(out.address.protocol(), QAbstractSocket::IPv6Protocol);
    QCOMPARE(out.address, QHostAddress(QHostAddress::LocalHostIPv6));
    QCOMPARE(out.port, (quint16)8080);
}

void PeerToPeerMessageTester::testRelayExtended()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeRelayExtended(3840, 4352, "HALLO123");

    verifyWritePayload(message, QByteArray::fromHex("030F0003000000001100000848414c4c4f313233"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F0003000000001100000848414c4c4f343536"));

    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::RELAY_EXTENDED);
    QCOMPARE(out.streamId, (quint16)4352);
    QCOMPARE(out.data, QByteArray("HALLO456"));
}

void PeerToPeerMessageTester::testRelayTruncated()
{
    PeerToPeerMessage message = PeerToPeerMessage::makeRelayTruncated(3840, 4352);

    verifyWritePayload(message, QByteArray::fromHex("030F0004000000001100"));

    // backwards
    PeerToPeerMessage out = verifyReadPayload(QByteArray::fromHex("030F0004000000001100"));

    QCOMPARE(out.circuitId, (quint16)3840);
    QCOMPARE(out.celltype, PeerToPeerMessage::ENCRYPTED);
    QCOMPARE(out.command, PeerToPeerMessage::RELAY_TRUNCATED);
    QCOMPARE(out.streamId, (quint16)4352);
}

void PeerToPeerMessageTester::verifyWritePayload(PeerToPeerMessage message, QByteArray expectedPayload)
{
    int size = expectedPayload.size();
    QByteArray arr = message.toBytes();

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
    PeerToPeerMessage out = PeerToPeerMessage::fromBytes(payload);

    QTest::qVerify(!out.malformed, "!out.malformed", "Malformed message parsed", __FILE__, __LINE__);
    if(out.celltype == PeerToPeerMessage::ENCRYPTED) {
        QTest::qVerify(out.isValidDigest(), "out.isValidDigest()", "Encrypted message parsed", __FILE__, __LINE__);
    }
    return out;
}
