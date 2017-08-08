#ifndef PEERTOPEERMESSAGE_H
#define PEERTOPEERMESSAGE_H

#include <QDataStream>
#include <QHostAddress>

// layout
// fixed-size: 1024 B
#define MESSAGE_LENGTH 1024
// we allow a hop length of up to 7
//    -> relay data header is 12B -> reserve 12 * 7 = 84B for headers
//    -> maximum unfragmented payload size on client / exit node should be 940B
#define MAX_RELAY_DATA_SIZE 940
//
// general message header
//    tunnelId (4B)
//    celltype (CMD/RELAY) (1B)
//    payload + padding (1019B)
//
// control messages: BUILD
//    general message header (5B)
//    handshake_len (2B)
//    handshake
//
// control messages: CREATED
//    general message header (5B)
//    handshake_len (2B)
//    handshake
//
// control messages: DESTROY
//    general message header (5B)
//    ?? TODO needs authentication, since we're not using tls but plain udp
//
// control messages: COVER
//    general message header (5B)
//    ---
//
// relay message header
//    general message header (5B)
//    RCMD (1B)
//    streamId (2B)
//    digest (4B)
//
// relay messages: DATA
//    relay message header (12B)
//    payload_size (2B)
//    payload + padding
//
// relay messages: EXTEND
//    relay message header (12B)
//    IP_V = 4 | 6 (1B)
//    addr  (4B/16B)
//    port  (2B)
//    handshake_len (2B)
//    handshake
//
// relay messages: EXTENDED
//    relay message header (12B)
//    handshake_len (2B)
//    handshake
//
// relay messages: TRUNCATED
//    relay message header (12B)
//    ??



class PeerToPeerMessage
{
public:
    PeerToPeerMessage();

    enum Celltype {
        Invalid = 0x00,

        RELAY = 0x01,
        CMD_BUILD = 0x02,
        CMD_CREATED = 0x03,
        CMD_DESTROY = 0x04,
        CMD_COVER = 0x05
    };

    enum Relaytype {
        RELAY_Invalid = 0x00,
        RELAY_DATA = 0x01,
        RELAY_EXTEND = 0x02,
        RELAY_EXTENDED = 0x03,
        RELAY_TRUNCATED = 0x04
    };

    bool isRelayMessage() const;
    // compute digest after all other data is set
    void calculateDigest();
    bool isValidDigest() const;

    // general msg header
    Celltype celltype = Invalid;
    quint32 tunnelId;

    // relay msg header
    Relaytype rcmd = RELAY_Invalid;
    quint32 digest = 0x00;
    quint16 streamId = 0x00;

    // relay_extend
    QHostAddress address;
    quint16 port = 0;

    // relay_data, also handshake payload for build/created/extend/extended
    QByteArray data; // payload + payloadSize

    bool malformed = false; // should close connection to this peer
};

// read a 16bit integer for length, and this amount of data after it into target.
// if the size is bigger than maxSize, nothing is read. Returns length of byte array read or -1.
int readPayload(QDataStream& stream, QByteArray *target, quint16 maxSize = 1400);
QDataStream & operator<< (QDataStream& stream, const PeerToPeerMessage& message);
QDataStream & operator>> (QDataStream& stream, PeerToPeerMessage& message);

#endif // PEERTOPEERMESSAGE_H
