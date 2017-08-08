#include "peertopeermessage.h"
#include <QDebug>

PeerToPeerMessage::PeerToPeerMessage()
{
}

void PeerToPeerMessage::calculateDigest()
{
    // digest is mock (not crc) for now -> 0
    digest = 0;
}

bool PeerToPeerMessage::isValidDigest() const
{
    // digest is mock for now -> correct if 0
    return digest == 0;
}

QDataStream &operator<<(QDataStream &stream, const PeerToPeerMessage &message)
{
    // first byte
    stream << static_cast<quint8>(message.celltype);
    if(message.celltype == PeerToPeerMessage::BUILD || message.celltype == PeerToPeerMessage::CREATED) {
        // | 01/02 | handshake_len (2B) | handshake
        int written = 1 + 2 + writePayload(stream, message.data);

        QByteArray padding(MESSAGE_LENGTH - written, '?');
        stream.writeRawData(padding.data(), padding.size());
        return stream;
    }

    // encrypted message:
    // | 03 | tId | <command> |   [6B]
    stream << message.tunnelId;
    stream << static_cast<quint8>(message.command);

    int bytesWritten = 6;
    switch (message.command) {
        break;
    case PeerToPeerMessage::RELAY_DATA:
        // | 03 | tId | RELAY_DATA      | streamId (2B) | digest (4B) | data_size (2B) | data
        stream << message.streamId;
        stream << message.digest;
        bytesWritten += 6 + 2 + writePayload(stream, message.data);
        break;
    case PeerToPeerMessage::RELAY_EXTEND:
        // | 03 | tId | RELAY_EXTEND    | streamId (2B) | digest (4B) | ip_v (1B) | ip (4B/16B) |
        // | port (2B) | handshake_len (2B) | handshake
        stream << message.streamId;
        stream << message.digest;
    {
        if(message.address.isNull()) {
            qDebug() << "invalid hostaddress while building RELAY_EXTEND message";
        }
        bool ipv4 = message.address.protocol() == QAbstractSocket::IPv4Protocol;
        stream << static_cast<quint8>(ipv4 ? 4 : 6);
        bytesWritten += 7;
        if(ipv4) {
            stream << message.address.toIPv4Address();
            bytesWritten += 4;
        } else {
            Q_IPV6ADDR addr = message.address.toIPv6Address();
            for(int i = 0; i < 16; i++) {
                stream << addr[i];
            }
            bytesWritten += 16;
        }
        stream << message.port;
        bytesWritten += 2 + 2 + writePayload(stream, message.data);
    }
        break;
    case PeerToPeerMessage::RELAY_EXTENDED:
        // | 03 | tId | RELAY_EXTENDED  | streamId (2B) | digest (4B) | handshake_len (2B) | handshake
        stream << message.streamId;
        stream << message.digest;
        bytesWritten += 6 + 2 + writePayload(stream, message.data);
        break;
    case PeerToPeerMessage::RELAY_TRUNCATED:
        // | 03 | tId | RELAY_TRUNCATED | streamId (2B) | digest (4B) | ??? (TODO)
        stream << message.streamId;
        stream << message.digest;
        bytesWritten += 6;
        break;
    case PeerToPeerMessage::CMD_DESTROY:
        // no more data
        break;
    case PeerToPeerMessage::CMD_TRUNCATED:
        // TODO
        break;
    case PeerToPeerMessage::CMD_INVALID:
    default:
        qDebug() << "invalid command" << message.command;
        break;
    }

    QByteArray padding(MESSAGE_LENGTH - bytesWritten, '?');
    stream.writeRawData(padding.data(), padding.size());
    return stream;
}

int readPayload(QDataStream &stream, PeerToPeerMessage *target, quint16 maxSize)
{
    quint16 size;
    stream >> size;

    if(size > maxSize) {
        qDebug() << "unreasonable payload size to read" << size;
        target->malformed = true;
        return 2;
    }

    QByteArray data;
    data.resize(size);
    stream.readRawData(data.data(), size);
    target->data = data;
    return size + 2;
}

int writePayload(QDataStream &stream, QByteArray source, quint16 maxSize)
{
    quint16 size = source.size();
    if(source.size() > maxSize) {
        qDebug() << "unreasonable payload size to write" << source.size() << "truncating to" << maxSize;
        size = maxSize;
    }

    stream << size;
    stream.writeRawData(source.data(), size);
    return size;
}

bool peekMessage(QDataStream &stream, PeerToPeerMessage &message)
{
    // first byte
    quint8 cellt;
    stream >> cellt;
    message.celltype = static_cast<PeerToPeerMessage::Celltype>(cellt);

    if(message.celltype == PeerToPeerMessage::BUILD || message.celltype == PeerToPeerMessage::CREATED) {
        // | 01/02 | handshake_len (2B) | handshake
        int read = readPayload(stream, &message);

        // read padding
        stream.skipRawData(MESSAGE_LENGTH - 1 - read);
        return true;
    }

    // encrypted message
    // -> don't read payload
    stream.skipRawData(MESSAGE_LENGTH - 1);
    return true;
}

bool parseMessage(QDataStream &stream, PeerToPeerMessage &message)
{
    // first byte
    quint8 cellt;
    stream >> cellt;
    message.celltype = static_cast<PeerToPeerMessage::Celltype>(cellt);

    if(message.celltype == PeerToPeerMessage::BUILD || message.celltype == PeerToPeerMessage::CREATED) {
        // | 01/02 | handshake_len (2B) | handshake
        int read = readPayload(stream, &message);

        // read padding
        stream.skipRawData(MESSAGE_LENGTH - 1 - read);
        return true;
    }

    // encrypted message:
    // | 03 | tId | <command> |   [6B]
    quint8 cmnd;
    stream >> message.tunnelId >> cmnd;
    message.command = static_cast<PeerToPeerMessage::Commandtype>(cmnd);

    int bytesRead = 6;
    switch (message.command) {
        break;
    case PeerToPeerMessage::RELAY_DATA:
        // | 03 | tId | RELAY_DATA      | streamId (2B) | digest (4B) | data_size (2B) | data
        stream >> message.streamId;
        stream >> message.digest;
        bytesRead += 6;
        bytesRead += readPayload(stream, &message);
        break;
    case PeerToPeerMessage::RELAY_EXTEND:
        // | 03 | tId | RELAY_EXTEND    | streamId (2B) | digest (4B) | ip_v (1B) | ip (4B/16B) | port (2B) | handshake_len (2B) | handshake
    {
        quint8 ipv;
        stream >> message.streamId;
        stream >> message.digest;
        stream >> ipv;
        bytesRead += 7;
        if(ipv == 4) {
            quint32 ip;
            stream >> ip;
            message.address.setAddress(ip);
            bytesRead += 4;
        } else if(ipv == 6) {
            QByteArray ipArr;
            ipArr.resize(16);
            stream.readRawData(ipArr.data(), 16);
            message.address.setAddress(reinterpret_cast<quint8*>(ipArr.data()));
            bytesRead += 16;
        } else {
            qDebug() << "IPV invalid in RELAY_EXTEND message" << ipv;
            message.malformed = true;
        }
        stream >> message.port;
        bytesRead += 2;
        bytesRead += readPayload(stream, &message);
    }
        break;
    case PeerToPeerMessage::RELAY_EXTENDED:
        // | 03 | tId | RELAY_EXTENDED  | streamId (2B) | digest (4B) | handshake_len (2B) | handshake
        stream >> message.streamId;
        stream >> message.digest;
        bytesRead += 6;
        bytesRead += readPayload(stream, &message);
        break;
    case PeerToPeerMessage::RELAY_TRUNCATED:
        // | 03 | tId | RELAY_TRUNCATED | streamId (2B) | digest (4B) | ??? (TODO)
        stream >> message.streamId;
        stream >> message.digest;
        bytesRead += 6;
        // TODO
        break;
    case PeerToPeerMessage::CMD_DESTROY:
        // no more data
        break;
    case PeerToPeerMessage::CMD_TRUNCATED:
        // TODO
        break;
    case PeerToPeerMessage::CMD_INVALID:
    default:
        qDebug() << "Reading invalid message command" << message.command;
        message.malformed = true;
        break;
    }

    return !message.malformed;
}

