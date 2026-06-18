#include "UdpPacketParser.h"

#include <QDateTime>

namespace
{
const quint32 LINK_ETHERNET = 1;

quint16 r16(const QByteArray& data, int offset)
{
    const quint16 b0 = static_cast<quint8>(data.at(offset));
    const quint16 b1 = static_cast<quint8>(data.at(offset + 1));
    return static_cast<quint16>((b0 << 8) | b1);
}

QString ipText(const QByteArray& data, int offset)
{
    return QString("%1.%2.%3.%4")
        .arg(static_cast<quint8>(data.at(offset)))
        .arg(static_cast<quint8>(data.at(offset + 1)))
        .arg(static_cast<quint8>(data.at(offset + 2)))
        .arg(static_cast<quint8>(data.at(offset + 3)));
}

QString timeText(quint64 seconds, quint32 microseconds)
{
    if (seconds == 0 && microseconds == 0)
    {
        return "N/A";
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds), Qt::UTC);
#else
    QDateTime dt;
    dt.setTimeSpec(Qt::UTC);
    dt.setTime_t(static_cast<uint>(seconds));
#endif

    return dt.toString("yyyy-MM-dd HH:mm:ss") + QString(".%1 UTC").arg(microseconds, 6, 10, QChar('0'));
}
}

ParsedUdpPacket UdpPacketParser::parsePacket(const RawPacket& packet)
{
    ParsedUdpPacket parsed;
    parsed.timestamp = timeText(packet.tsSec, packet.tsUsec);

    if (packet.linkType != LINK_ETHERNET)
    {
        parsed.error = "Unsupported link type.";
        return parsed;
    }

    const QByteArray& data = packet.data;

    if (data.size() < 14)
    {
        parsed.error = "Packet is smaller than Ethernet header.";
        return parsed;
    }

    int etherTypeOffset = 12;
    quint16 etherType = r16(data, etherTypeOffset);
    int networkOffset = 14;

    if (etherType == 0x8100 || etherType == 0x88A8)
    {
        if (data.size() < 18)
        {
            parsed.error = "Truncated VLAN Ethernet frame.";
            return parsed;
        }

        etherTypeOffset = 16;
        etherType = r16(data, etherTypeOffset);
        networkOffset = 18;
    }

    if (etherType != 0x0800)
    {
        parsed.error = "Not an IPv4 packet.";
        return parsed;
    }

    if (data.size() < networkOffset + 20)
    {
        parsed.error = "Packet is smaller than minimum IPv4 header.";
        return parsed;
    }

    const quint8 versionIhl = static_cast<quint8>(data.at(networkOffset));
    const int version = (versionIhl >> 4) & 0x0F;
    const int ihl = versionIhl & 0x0F;
    const int ipHeaderLength = ihl * 4;

    if (version != 4)
    {
        parsed.error = "IP version is not IPv4.";
        return parsed;
    }

    if (ipHeaderLength < 20)
    {
        parsed.error = "Invalid IPv4 header length.";
        return parsed;
    }

    if (data.size() < networkOffset + ipHeaderLength)
    {
        parsed.error = "Truncated IPv4 header.";
        return parsed;
    }

    const quint16 totalLength = r16(data, networkOffset + 2);
    if (totalLength < ipHeaderLength + 8)
    {
        parsed.error = "Invalid IPv4 total length.";
        return parsed;
    }

    if (data.size() < networkOffset + static_cast<int>(totalLength))
    {
        parsed.error = "Truncated IPv4 packet.";
        return parsed;
    }

    const quint16 flagsAndFragment = r16(data, networkOffset + 6);
    const int fragmentOffset = flagsAndFragment & 0x1FFF;
    const bool moreFragments = (flagsAndFragment & 0x2000) != 0;
    if (fragmentOffset != 0 || moreFragments)
    {
        parsed.error = "Fragmented IPv4 packet skipped.";
        return parsed;
    }

    const quint8 protocol = static_cast<quint8>(data.at(networkOffset + 9));
    if (protocol != 17)
    {
        parsed.error = "Not a UDP packet.";
        return parsed;
    }

    parsed.sourceIp = ipText(data, networkOffset + 12);
    parsed.destinationIp = ipText(data, networkOffset + 16);

    const int udpOffset = networkOffset + ipHeaderLength;
    if (data.size() < udpOffset + 8)
    {
        parsed.error = "Truncated UDP header.";
        return parsed;
    }

    parsed.sourcePort = r16(data, udpOffset);
    parsed.destinationPort = r16(data, udpOffset + 2);
    const quint16 udpLength = r16(data, udpOffset + 4);

    if (udpLength < 8)
    {
        parsed.error = "Invalid UDP length.";
        return parsed;
    }

    const int udpEnd = udpOffset + static_cast<int>(udpLength);
    const int ipEnd = networkOffset + static_cast<int>(totalLength);

    if (udpEnd > ipEnd || udpEnd > data.size())
    {
        parsed.error = "Truncated UDP payload.";
        return parsed;
    }

    const int payloadOffset = udpOffset + 8;
    const int payloadLength = static_cast<int>(udpLength) - 8;

    parsed.udpPayload = data.mid(payloadOffset, payloadLength);
    parsed.payloadSize = payloadLength;
    parsed.valid = true;
    return parsed;
}
