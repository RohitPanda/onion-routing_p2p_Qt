#ifndef PEERTOPEERMESSAGE_H
#define PEERTOPEERMESSAGE_H

#include <QDataStream>
#include <QHostAddress>

// layout
// fixed-size: 1027 B
// with three byte unencrypted, encrypted payload will be multiple of 128, eliminating padding for encryption
#define MESSAGE_LENGTH 1027
// we allow a hop length of up to 7
//    -> relay data header is 3B -> reserve 12 * 3 = 36B for headers
//    -> maximum unfragmented payload size on client / exit node should be 1024B - 36B = 988B
#define MAX_RELAY_DATA_SIZE 988
//
// first byte indicates build (01) / created (02) / encrypted (03) message
//
// build message:     | 01 | circ_id (2B) | handshake_len (2B) | handshake
// created message:   | 02 | circ_id (2B) | handshake_len (2B) | handshake
// encrypted message: | 03 | circ_id (2B) | <payload>
//
// payload:   | streamId (2B) | celltype (1B) | <command payload>
// celltype can be CMD_DESTROY, RELAY_DATA, RELAY_EXTEND, RELAY_EXTENDED, RELAY_TRUNCATED
//
// command payload:
// | 03 | CMD_DESTROY     | -
// | 03 | RELAY_DATA      | streamId (2B) | digest (4B) | data_size (2B) | data
// | 03 | RELAY_EXTEND    | streamId (2B) | digest (4B) | ip_v (1B) | ip (4B/16B) | port (2B) | handshake_len (2B) | handshake
// | 03 | RELAY_EXTENDED  | streamId (2B) | digest (4B) | handshake_len (2B) | handshake
// | 03 | RELAY_TRUNCATED | streamId (2B) | digest (4B) | ??? (TODO)

class PeerToPeerMessage
{
public:
    PeerToPeerMessage();

    enum Celltype {
        Invalid = 0x00,

        BUILD = 0x01,
        CREATED = 0x02,
        ENCRYPTED = 0x03
    };

    enum Commandtype {
        CMD_INVALID = 0x00,
        RELAY_DATA = 0x01,
        RELAY_EXTEND = 0x02,
        RELAY_EXTENDED = 0x03,
        RELAY_TRUNCATED = 0x04,
        CMD_DESTROY = 0x05,
        CMD_COVER = 0x07
    };

    bool isEncrypted() const { return celltype == ENCRYPTED; }

    QString typeString() const;

    // compute digest after all other data is set
    void calculateDigest();
    bool isValidDigest() const;

    // general msg header
    Celltype celltype = Invalid;
    quint16 circuitId;

    // relay msg header
    Commandtype command = CMD_INVALID;
    quint32 digest = 0x00;
    quint16 streamId = 0x00;

    // relay_extend
    QHostAddress address;
    quint16 port = 0;

    // relay_data, also handshake payload for build/created/extend/extended
    QByteArray data; // payload + payloadSize

    bool malformed = false; // should close connection to this peer
};

// read a 16bit integer for length, and this amount of data after it into target message.
// if the size is bigger than maxSize, content is discarded. Returns number of bytes consumed from stream
int readPayload(QDataStream& stream, PeerToPeerMessage *target, quint16 maxSize = MAX_RELAY_DATA_SIZE);

// write a source into stream, as a 16bit length integer with data following.
// writes maxSize at maximum, truncating source. Returns number of bytes written from array (not including length)
int writePayload(QDataStream& stream, QByteArray source, quint16 maxSize = MAX_RELAY_DATA_SIZE);

QDataStream & operator<< (QDataStream& stream, const PeerToPeerMessage& message);

// peeks at the message in stream, not reading an encrypted message's payload
bool peekMessage(QDataStream& stream, PeerToPeerMessage& message);
// parses the message in stream, assuming already unencrypted payload
bool parseMessage(QDataStream& stream, PeerToPeerMessage& message);


#endif // PEERTOPEERMESSAGE_H
