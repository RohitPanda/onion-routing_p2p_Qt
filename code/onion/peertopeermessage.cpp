#include "peertopeermessage.h"
#include <QDebug>

PeerToPeerMessage::PeerToPeerMessage()
{
}

bool PeerToPeerMessage::isRelayMessage() const
{
    switch (celltype) {
    case PeerToPeerMessage::RELAY:
        return true;
    case PeerToPeerMessage::CMD_BUILD:
    case PeerToPeerMessage::CMD_CREATED:
    case PeerToPeerMessage::CMD_DESTROY:
    case PeerToPeerMessage::CMD_COVER:
        return false;
    default:
        qDebug() << "unknown celltype" << celltype;
        return false;
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

QDataStream &operator<<(QDataStream &stream, const PeerToPeerMessage &message)
{
    // layout c.f. header
    // general msg header
    stream << message.tunnelId;
    stream << static_cast<quint8>(message.celltype);

    uint writeOver; // write rest of message to stream
    switch (message.celltype) {
    case PeerToPeerMessage::RELAY:
    {
        // relay msg header
        stream << static_cast<quint8>(message.rcmd);
        stream << message.streamId;
        stream << message.digest;

        switch (message.rcmd) {
        case PeerToPeerMessage::RELAY_DATA:
        {
            int nWrite = message.data.size();
            if(message.data.size() > MESSAGE_LENGTH - 12 - 2) {
                qDebug() << "RELAY_DATA payload does not fit. Truncating...";
                nWrite = MESSAGE_LENGTH - 12 - 2;
            }
            if(message.data.size() > MAX_RELAY_DATA_SIZE) {
                qDebug() << "RELAY_DATA payload size is" << message.data.size() << "should be only"
                         << MAX_RELAY_DATA_SIZE << "message might get truncated for large hop counts.";
            }

            stream << static_cast<quint16>(message.data.size());
            stream.writeRawData(message.data.data(), nWrite);
            writeOver = MESSAGE_LENGTH - 12 - 2 - nWrite;
        }
            break;
        case PeerToPeerMessage::RELAY_EXTEND:
        {
            if(message.address.isNull()) {
                qDebug() << "invalid hostaddress while building RELAY_EXTEND message";
            }
            bool ipv4 = message.address.protocol() == QAbstractSocket::IPv4Protocol;
            stream << static_cast<quint8>(ipv4 ? 4 : 6);
            if(ipv4) {
                stream << message.address.toIPv4Address();
                writeOver = MESSAGE_LENGTH - 12 - 1 - 4 - 2;
            } else {
                Q_IPV6ADDR addr = message.address.toIPv6Address();
                for(int i = 0; i < 16; i++) {
                    stream << addr[i];
                }
                writeOver = MESSAGE_LENGTH - 12 - 1 - 16 - 2;
            }
            stream << message.port;

            stream << static_cast<quint16>(message.data.size());
            stream.writeRawData(message.data.data(), message.data.size());
            writeOver -= 2 + message.data.size();
        }
            break;
        case PeerToPeerMessage::RELAY_EXTENDED:
            // handshake len + payload
            stream << static_cast<quint16>(message.data.size());
            stream.writeRawData(message.data.data(), message.data.size());
            writeOver = MESSAGE_LENGTH - 12 - 2 - message.data.size();
            break;
        case PeerToPeerMessage::RELAY_TRUNCATED:
            writeOver = MESSAGE_LENGTH - 12;
            // TODO
            break;
        default:
            qDebug() << "unknown rcmd value while building RELAY message" << message.rcmd;
            writeOver = MESSAGE_LENGTH - 12;
            break;
        }
    }
        break;
    case PeerToPeerMessage::CMD_BUILD:
    {
        // handshake len + payload
        stream << static_cast<quint16>(message.data.size());
        stream.writeRawData(message.data.data(), message.data.size());
        writeOver = MESSAGE_LENGTH - 5 - 2 - message.data.size();
    }
        break;
    case PeerToPeerMessage::CMD_CREATED:
        // handshake len + payload
        stream << static_cast<quint16>(message.data.size());
        stream.writeRawData(message.data.data(), message.data.size());
        writeOver = MESSAGE_LENGTH - 5 - 2 - message.data.size();
        break;
    case PeerToPeerMessage::CMD_DESTROY:
        writeOver = MESSAGE_LENGTH - 5;
        // TODO
        break;
    case PeerToPeerMessage::CMD_COVER:
        // cover traffic, fill with padding
        writeOver = MESSAGE_LENGTH - 5;
        break;
    default:
        qDebug() << "unknown celltype" << message.celltype;
        break;
    }

    QByteArray padding(writeOver, '?');
    stream.writeRawData(padding.data(), padding.size());
    return stream;
}

QDataStream &operator>>(QDataStream &stream, PeerToPeerMessage &message)
{
    // layout c.f. header
    // general msg header
    stream >> message.tunnelId;
    quint8 cellt;
    stream >> cellt;
    message.celltype = static_cast<PeerToPeerMessage::Celltype>(cellt);

    int consumeOver = -1; // consume rest of message
    if(message.celltype == PeerToPeerMessage::RELAY) {
        // relay msg header
        quint8 rcmd;
        stream >> rcmd >> message.streamId >> message.digest;
        message.rcmd = static_cast<PeerToPeerMessage::Relaytype>(rcmd);
        if(message.rcmd == PeerToPeerMessage::RELAY_DATA) {
            int payloadSize = readPayload(stream, &message.data);
            if(payloadSize == -1) {
                qDebug() << "unreasonable payload size" << payloadSize;
                // TODO close connection
                // reset payloadsize for consumeOver
                payloadSize = 0;
            }

            consumeOver = MESSAGE_LENGTH - 12 - 2 - payloadSize;
        } else if(message.rcmd == PeerToPeerMessage::RELAY_EXTEND) {
            quint8 ipv;
            stream >> ipv;
            if(ipv == 4) {
                quint32 ip;
                stream >> ip;
                message.address.setAddress(ip);
                consumeOver = MESSAGE_LENGTH - 12 - 1 - 4 - 2;
            } else if(ipv == 6) {
                QByteArray ipArr;
                ipArr.resize(16);
                stream.readRawData(ipArr.data(), 16);
                message.address.setAddress(reinterpret_cast<quint8*>(ipArr.data()));
                consumeOver = MESSAGE_LENGTH - 12 - 1 - 16 - 2;
            } else {
                qDebug() << "IPV invalid in RELAY_EXTEND message" << ipv;
                consumeOver = MESSAGE_LENGTH - 12 - 1 - 2;
            }
            stream >> message.port;

            int payloadSize = readPayload(stream, &message.data);
            if(payloadSize == -1) {
                qDebug() << "unreasonable payload size" << payloadSize;
                // TODO close connection
                // reset payloadsize for consumeOver
                payloadSize = 0;
            }
            consumeOver -= 2 + payloadSize;
        } else if(message.rcmd == PeerToPeerMessage::RELAY_EXTENDED) {
            int payloadSize = readPayload(stream, &message.data);
            if(payloadSize == -1) {
                qDebug() << "unreasonable payload size" << payloadSize;
                // TODO close connection
                // reset payloadsize for consumeOver
                payloadSize = 0;
            }
            consumeOver = MESSAGE_LENGTH - 12 - 2 - payloadSize;
        } else if(message.rcmd == PeerToPeerMessage::RELAY_TRUNCATED) {
            // TODO
            consumeOver = MESSAGE_LENGTH - 12;
        } else {
            qDebug() << "unknown rcmd" << message.rcmd;
            consumeOver = MESSAGE_LENGTH - 12;
        }
    } else if(message.celltype == PeerToPeerMessage::CMD_BUILD) {
        int payloadSize = readPayload(stream, &message.data);
        if(payloadSize == -1) {
            qDebug() << "unreasonable payload size" << payloadSize;
            // TODO close connection
            // reset payloadsize for consumeOver
            payloadSize = 0;
        }
        consumeOver = MESSAGE_LENGTH - 5 - 2 - payloadSize;
    } else if(message.celltype == PeerToPeerMessage::CMD_CREATED) {
        int payloadSize = readPayload(stream, &message.data);
        if(payloadSize == -1) {
            qDebug() << "unreasonable payload size" << payloadSize;
            // TODO close connection
            // reset payloadsize for consumeOver
            payloadSize = 0;
        }
        consumeOver = MESSAGE_LENGTH - 5 - 2 - payloadSize;
    } else if(message.celltype == PeerToPeerMessage::CMD_DESTROY) {
        // TODO
        consumeOver = MESSAGE_LENGTH - 5;
    } else if(message.celltype == PeerToPeerMessage::CMD_COVER) {
        // cover: drop payload
        consumeOver = MESSAGE_LENGTH - 5;
    } else {
        qDebug() << "unknown celltype" << message.celltype;
    }

    if(consumeOver < 0) {
        qDebug() << "error or unset consumeOver, got" << consumeOver;
    } else {
        stream.skipRawData(consumeOver);
    }
    return stream;
}

int readPayload(QDataStream &stream, QByteArray *target, quint16 maxSize)
{
    quint16 size;
    stream >> size;

    if(maxSize > 1400) {
        qDebug() << "unreasonable payload size" << size;
        return -1;
    }

    QByteArray data;
    data.resize(size);
    stream.readRawData(data.data(), size);
    *target = data;
    return size;
}
