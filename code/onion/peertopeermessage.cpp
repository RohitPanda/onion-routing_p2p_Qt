#include "peertopeermessage.h"
#include <QDebug>

PeerToPeerMessage::PeerToPeerMessage()
{
}

QString PeerToPeerMessage::typeString() const
{
    switch (celltype) {
    case PeerToPeerMessage::BUILD:
        return "BUILD";
    case PeerToPeerMessage::CREATED:
        return "CREATED";
    case PeerToPeerMessage::ENCRYPTED:
        switch (command) {
        case PeerToPeerMessage::CMD_INVALID:
            return "ENCRYPTED <??>";
        case PeerToPeerMessage::RELAY_DATA:
            return "ENCRYPTED -> RELAY_DATA";
        case PeerToPeerMessage::RELAY_EXTEND:
            return "ENCRYPTED -> RELAY_EXTEND";
        case PeerToPeerMessage::RELAY_EXTENDED:
            return "ENCRYPTED -> RELAY_EXTENDED";
        case PeerToPeerMessage::RELAY_TRUNCATED:
            return "ENCRYPTED -> RELAY_TRUNCATED";
        case PeerToPeerMessage::CMD_DESTROY:
            return "ENCRYPTED -> CMD_DESTROY";
        case PeerToPeerMessage::CMD_COVER:
            return "ENCRYPTED -> CMD_COVER";
        default:
            return "ENCRYPTED <invalid>";
        }
        break;
    case PeerToPeerMessage::Invalid:
    default:
        return "<invalid celltype>";
    }
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

PeerToPeerMessage PeerToPeerMessage::fromDatagram(QNetworkDatagram dgram)
{
    PeerToPeerMessage msg = fromBytes(dgram.data());
    msg.sender = Binding(dgram.senderAddress(), dgram.senderPort());
    return msg;
}

PeerToPeerMessage PeerToPeerMessage::fromBytes(QByteArray fullPacket)
{
    if(fullPacket.size() != MESSAGE_LENGTH) {
        qDebug() << "invalid packet size in fromBytes, got" << fullPacket.size();
        PeerToPeerMessage response;
        response.malformed = true;
        return response;
    }

    // parse messagetype byte
    QDataStream stream(fullPacket);
    stream.setByteOrder(QDataStream::BigEndian);

    // first byte
    quint8 ctypeChar;
    quint16 circId;
    stream >> ctypeChar >> circId;
    PeerToPeerMessage::Celltype ctype = static_cast<PeerToPeerMessage::Celltype>(ctypeChar);

    // for 01/02 parse full packet, otherwise forward to fromDecrypted
    if(ctype == PeerToPeerMessage::BUILD || ctype == PeerToPeerMessage::CREATED) {
        // | 01/02 | circId | handshake_len (2B) | handshake
        QByteArray handshake;
        bool ok = readPayload(stream, &handshake);
        PeerToPeerMessage result;
        result.celltype = ctype;
        result.circuitId = circId;
        result.data = handshake;
        result.malformed = !ok;

        return result;
    }

    if(ctype != PeerToPeerMessage::ENCRYPTED) {
        qDebug() << "invalid celltype fromBytes, got" << ctype();
        PeerToPeerMessage response;
        response.malformed = true;
        return response;
    }

    QByteArray payload = fullPacket.mid(3);
    PeerToPeerMessage fromEncrypted = fromEncryptedPayload(payload, false); // should still be encrypted
    // set what we parsed
    fromEncrypted.circuitId = circId;
    fromEncrypted.celltype = ctype;
    return fromEncrypted;
}

PeerToPeerMessage PeerToPeerMessage::fromEncryptedPayload(QByteArray encryptedMessage)
{
    QDataStream stream(encryptedMessage);
    stream.setByteOrder(QDataStream::BigEndian);

    // parse relay header
    PeerToPeerMessage message;
    message.celltype = PeerToPeerMessage::ENCRYPTED;
    quint8 commandChar;
    stream << commandChar << message.digest << message.streamId;

    if(!message.isValidDigest()) {
        // message is still encrypted
        message.command = PeerToPeerMessage::CMD_INVALID;
        return message;
    }

    // parse fully, stream is now at start of command payload
    message.command = static_cast<PeerToPeerMessage::Commandtype>(commandChar);

    switch (message.command) {
    case PeerToPeerMessage::RELAY_DATA:
        // data_size (2B) | data
        message.malformed = !readPayload(stream, &message.data);
        break;
    case PeerToPeerMessage::RELAY_EXTEND:
        // ip_v (1B) | ip (4B/16B) | port (2B) | handshake_len (2B) | handshake
    {
        quint8 ipv;
        stream >> ipv;
        if(ipv == 4) {
            quint32 ip;
            stream >> ip;
            message.address.setAddress(ip);
        } else if(ipv == 6) {
            QByteArray ipArr;
            ipArr.resize(16);
            stream.readRawData(ipArr.data(), 16);
            message.address.setAddress(reinterpret_cast<quint8*>(ipArr.data()));
        } else {
            qDebug() << "IPV invalid in RELAY_EXTEND message" << ipv;
            message.malformed = true;
        }
        stream >> message.port;
        bool ok = readPayload(stream, &message.data);
        if(!ok) {
            message.malformed = true;
        }
    }
        break;
    case PeerToPeerMessage::RELAY_EXTENDED:
        // handshake_len (2B) | handshake
        message.malformed = !readPayload(stream, &message);
        break;
    case PeerToPeerMessage::RELAY_TRUNCATED:
    case PeerToPeerMessage::CMD_DESTROY:
    case PeerToPeerMessage::CMD_COVER:
        // <no payload>
        break;
    case PeerToPeerMessage::CMD_INVALID:
    default:
        qDebug() << "Reading invalid message command from valid digest o.0" << message.command;
        message.malformed = true;
        break;
    }

    return message;
}

PeerToPeerMessage PeerToPeerMessage::fromEncryptedPayload(QByteArray encryptedMessage, quint16 circId)
{
    PeerToPeerMessage message = fromEncryptedPayload(encryptedMessage);
    message.circuitId = circId;
    return message;
}

QByteArray PeerToPeerMessage::toEncryptedPayload() const
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    // write header
    stream << static_cast<quint8>(command) << digest << streamId;

    // write command payload
    switch (command) {
    case PeerToPeerMessage::RELAY_DATA:
        // data_size (2B) | data
        writePayload(stream, data);
        break;
    case PeerToPeerMessage::RELAY_EXTEND:
        // ip_v (1B) | ip (4B/16B) | port (2B) | handshake_len (2B) | handshake
    {
        if(address.isNull()) {
            qDebug() << "invalid hostaddress while building RELAY_EXTEND message";
        }
        bool ipv4 = address.protocol() == QAbstractSocket::IPv4Protocol;
        stream << static_cast<quint8>(ipv4 ? 4 : 6);
        if(ipv4) {
            stream << address.toIPv4Address();
        } else {
            Q_IPV6ADDR addr = address.toIPv6Address();
            for(int i = 0; i < 16; i++) {
                stream << addr[i];
            }
        }
        stream << port;
        writePayload(stream, data);
    }
        break;
    case PeerToPeerMessage::RELAY_EXTENDED:
        // handshake_len (2B) | handshake
        writePayload(stream, data);
        break;
    case PeerToPeerMessage::RELAY_TRUNCATED:
    case PeerToPeerMessage::CMD_DESTROY:
    case PeerToPeerMessage::CMD_COVER: // padding also fills data up, so no extra random data.
        // no more data to write
        break;
    case PeerToPeerMessage::CMD_INVALID:
    default:
        qDebug() << "invalid command" << command;
        break;
    }

    return pad(payload, MESSAGE_LENGTH - 3); // we're not a full packet
}

QByteArray PeerToPeerMessage::toBytes() const
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::BigEndian);

    if(celltype == BUILD || celltype == CREATED) {
        // 01/02 | circId | handshake_len | handshake
        stream << static_cast<quint8>(celltype);
        stream << circuitId;
        writePayload(stream, data);
        return pad(packet, MESSAGE_LENGTH);
    }

    if(celltype == ENCRYPTED) {
        stream << static_cast<quint8>(celltype);
        stream << circuitId;
        QByteArray payload = toEncryptedPayload();
        stream.writeRawData(payload.data(), payload.size());
        return packet;
    }

    qDebug() << "invalid celltype in toBytes" << celltype;
    return QByteArray();
}

QNetworkDatagram PeerToPeerMessage::toDatagram(Binding target) const
{
    QByteArray data = toBytes();
    if(!target.isValid()) {
        target = sender;
    }

    if(!target.isValid()) {
        qDebug() << "toDatagram -> invalid target and unset sender in P2PMessage. Datagram will fail to send.";
    }

    QNetworkDatagram dgram(data, target.address, target.port);
    return dgram;
}

QByteArray PeerToPeerMessage::pad(QByteArray packet, int length)
{
    QByteArray padding('?', length - packet.size());
    padding.prepend(packet);
    return padding;
}

bool readPayload(QDataStream &stream, QByteArray *target, quint16 maxSize)
{
    quint16 size;
    stream >> size;

    if(size > maxSize) {
        qDebug() << "unreasonable payload size to read" << size;
        return false;
    }

    QByteArray data;
    data.resize(size);
    stream.readRawData(data.data(), size);
    *target = data;
    return true;
}

bool writePayload(QDataStream &stream, QByteArray source, quint16 maxSize)
{
    quint16 size = source.size();
    if(source.size() > maxSize) {
        qDebug() << "unreasonable payload size to write" << source.size() << "truncating to" << maxSize;
        size = maxSize;
    }

    stream << size;
    stream.writeRawData(source.data(), size);
    return size == source.size();
}

