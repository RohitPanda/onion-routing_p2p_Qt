#ifndef PEERTOPEERMESSAGE_H
#define PEERTOPEERMESSAGE_H

#include "binding.h"

#include <QDataStream>
#include <QHostAddress>
#include <QNetworkDatagram>

// layout
// fixed-size: 1027 B
// with three byte unencrypted, encrypted payload will be multiple of 128, eliminating padding for encryption
#define MESSAGE_LENGTH 1027
#define UNENCRYPTED_HEADER_LEN 3
// we dont wrap packets, but encrypt the payload consequtively => no hop limit
#define MAX_RELAY_DATA_SIZE 1024
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
// | 03 | CMD_DESTROY     | digest (4B) | reserved (2B) // to fit header size
// | 03 | CMD_COVER       | digest (4B) | reserved (2B) // to fit header size
// | 03 | RELAY_DATA      | digest (4B) | streamId (2B) | data_size (2B) | data
// | 03 | RELAY_EXTEND    | digest (4B) | streamId (2B) | ip_v (1B) | ip (4B/16B) | port (2B) | handshake_len (2B) | handshake
// | 03 | RELAY_EXTENDED  | digest (4B) | streamId (2B) | handshake_len (2B) | handshake
// | 03 | RELAY_TRUNCATED | digest (4B) | streamId (2B) | --

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

    Binding sender; // parsed from QNetworkDatagram if present

public:
    // factory shorthands
    static PeerToPeerMessage makeBuild(quint16 circId, QByteArray handshake);
    static PeerToPeerMessage makeCreated(quint16 circId, QByteArray handshake);
    static PeerToPeerMessage makeRelayData(quint16 circId, quint16 streamId, QByteArray data);
    static PeerToPeerMessage makeRelayExtend(quint16 circId, quint16 streamId, Binding targetAddress, QByteArray handshake);
    static PeerToPeerMessage makeRelayExtended(quint16 circId, quint16 streamId, QByteArray handshake);
    static PeerToPeerMessage makeRelayTruncated(quint16 circId, quint16 streamId);
    static PeerToPeerMessage makeCommandDestroy(quint16 circId);
    static PeerToPeerMessage makeCommandCover(quint16 circId);

    // parsing
    static PeerToPeerMessage fromDatagram(QNetworkDatagram dgram);
    static PeerToPeerMessage fromBytes(QByteArray fullPacket);

    // parse the payload of a 03 | circId | <encryptedMessage> msg
    static PeerToPeerMessage fromEncryptedPayload(QByteArray encryptedMessage); // ignores the preface 03 | circId
    static PeerToPeerMessage fromEncryptedPayload(QByteArray encryptedMessage, quint16 circId);

    // exporting
    QByteArray toEncryptedPayload() const; // make a packet without header
    QByteArray toBytes() const; // make a full packet
    QNetworkDatagram toDatagram(Binding target = Binding()) const; // uses target or this.sender if sender is invalid

    // compose a full message from the encrypted payload
    static QByteArray composeEncrypted(quint16 circId, QByteArray encryptedPayload);
private:
    static QByteArray pad(QByteArray packet, int length);
};

// read a 16bit integer for length, and this amount of data after it into target message.
// if the size is bigger than maxSize, content is discarded. Returns number of bytes consumed from stream
bool readPayload(QDataStream& stream, QByteArray *target, quint16 maxSize = MAX_RELAY_DATA_SIZE);

// write a source into stream, as a 16bit length integer with data following.
// writes maxSize at maximum, truncating source. Returns true if source was written completely
bool writePayload(QDataStream& stream, QByteArray source, quint16 maxSize = MAX_RELAY_DATA_SIZE);


#endif // PEERTOPEERMESSAGE_H
