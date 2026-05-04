#include "PcapFileReader.h"

#include <QtGlobal>
#include <cmath>

namespace
{
const quint32 LINKTYPE_ETHERNET = 1;
const quint32 PCAPNG_SECTION_HEADER_BLOCK = 0x0A0D0D0A;
const quint32 PCAPNG_INTERFACE_DESCRIPTION_BLOCK = 0x00000001;
const quint32 PCAPNG_SIMPLE_PACKET_BLOCK = 0x00000003;
const quint32 PCAPNG_ENHANCED_PACKET_BLOCK = 0x00000006;
const quint32 MAX_SINGLE_PACKET_SIZE = 16U * 1024U * 1024U;
const quint32 MAX_PCAPNG_BLOCK_SIZE = 64U * 1024U * 1024U;
}

PcapFileReader::PcapFileReader()
    : m_format(FormatUnknown),
      m_byteOrder(LittleEndian),
      m_linkType(LINKTYPE_ETHERNET),
      m_nanosecondPcap(false),
      m_packetCounter(0)
{
}

PcapFileReader::~PcapFileReader()
{
    close();
}

bool PcapFileReader::open(const QString& filePath, QString& errorMessage)
{
    close();
    errorMessage.clear();

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly))
    {
        errorMessage = "Cannot open capture file.";
        return false;
    }

    const QByteArray firstBytes = m_file.read(12);
    if (firstBytes.size() < 4)
    {
        errorMessage = "Capture file is too small.";
        close();
        return false;
    }

    const quint8 b0 = static_cast<quint8>(firstBytes.at(0));
    const quint8 b1 = static_cast<quint8>(firstBytes.at(1));
    const quint8 b2 = static_cast<quint8>(firstBytes.at(2));
    const quint8 b3 = static_cast<quint8>(firstBytes.at(3));

    if (b0 == 0x0A && b1 == 0x0D && b2 == 0x0D && b3 == 0x0A)
    {
        if (firstBytes.size() < 12)
        {
            errorMessage = "Invalid PCAPNG section header.";
            close();
            return false;
        }

        m_file.seek(0);
        const QByteArray sectionFirstBytes = m_file.read(12);
        if (!parsePcapNgSectionHeader(sectionFirstBytes, errorMessage))
        {
            close();
            return false;
        }

        m_format = FormatPcapNg;
        return true;
    }

    m_file.seek(0);
    const QByteArray globalHeader = m_file.read(24);
    if (!parsePcapGlobalHeader(globalHeader, errorMessage))
    {
        close();
        return false;
    }

    m_format = FormatPcap;
    return true;
}

bool PcapFileReader::readNextPacket(RawPacket& packet, QString& errorMessage)
{
    errorMessage.clear();

    if (!m_file.isOpen())
    {
        errorMessage = "Capture file is not open.";
        return false;
    }

    if (m_format == FormatPcap)
    {
        return readNextPcapPacket(packet, errorMessage);
    }

    if (m_format == FormatPcapNg)
    {
        return readNextPcapNgPacket(packet, errorMessage);
    }

    errorMessage = "Unknown capture file format.";
    return false;
}

void PcapFileReader::close()
{
    if (m_file.isOpen())
    {
        m_file.close();
    }

    m_format = FormatUnknown;
    m_byteOrder = LittleEndian;
    m_linkType = LINKTYPE_ETHERNET;
    m_nanosecondPcap = false;
    m_packetCounter = 0;
    m_interfaces.clear();
}

bool PcapFileReader::isOpen() const
{
    return m_file.isOpen();
}

QString PcapFileReader::formatName() const
{
    if (m_format == FormatPcap)
    {
        return "PCAP";
    }

    if (m_format == FormatPcapNg)
    {
        return "PCAPNG";
    }

    return "Unknown";
}

bool PcapFileReader::parsePcapGlobalHeader(const QByteArray& header, QString& errorMessage)
{
    if (header.size() < 24)
    {
        errorMessage = "Invalid PCAP global header.";
        return false;
    }

    const quint8 b0 = static_cast<quint8>(header.at(0));
    const quint8 b1 = static_cast<quint8>(header.at(1));
    const quint8 b2 = static_cast<quint8>(header.at(2));
    const quint8 b3 = static_cast<quint8>(header.at(3));

    if (b0 == 0xA1 && b1 == 0xB2 && b2 == 0xC3 && b3 == 0xD4)
    {
        m_byteOrder = BigEndian;
        m_nanosecondPcap = false;
    }
    else if (b0 == 0xD4 && b1 == 0xC3 && b2 == 0xB2 && b3 == 0xA1)
    {
        m_byteOrder = LittleEndian;
        m_nanosecondPcap = false;
    }
    else if (b0 == 0xA1 && b1 == 0xB2 && b2 == 0x3C && b3 == 0x4D)
    {
        m_byteOrder = BigEndian;
        m_nanosecondPcap = true;
    }
    else if (b0 == 0x4D && b1 == 0x3C && b2 == 0xB2 && b3 == 0xA1)
    {
        m_byteOrder = LittleEndian;
        m_nanosecondPcap = true;
    }
    else
    {
        errorMessage = "Unsupported or invalid capture file format.";
        return false;
    }

    m_linkType = readU32(header, 20, m_byteOrder);
    return true;
}

bool PcapFileReader::parsePcapNgSectionHeader(const QByteArray& firstBytes, QString& errorMessage)
{
    if (firstBytes.size() < 12)
    {
        errorMessage = "Invalid PCAPNG section header.";
        return false;
    }

    const quint8 m0 = static_cast<quint8>(firstBytes.at(8));
    const quint8 m1 = static_cast<quint8>(firstBytes.at(9));
    const quint8 m2 = static_cast<quint8>(firstBytes.at(10));
    const quint8 m3 = static_cast<quint8>(firstBytes.at(11));

    if (m0 == 0x1A && m1 == 0x2B && m2 == 0x3C && m3 == 0x4D)
    {
        m_byteOrder = BigEndian;
    }
    else if (m0 == 0x4D && m1 == 0x3C && m2 == 0x2B && m3 == 0x1A)
    {
        m_byteOrder = LittleEndian;
    }
    else
    {
        errorMessage = "Invalid PCAPNG byte-order magic.";
        return false;
    }

    const quint32 totalLength = readU32(firstBytes, 4, m_byteOrder);
    if (totalLength < 28 || totalLength > MAX_PCAPNG_BLOCK_SIZE)
    {
        errorMessage = "Invalid PCAPNG section length.";
        return false;
    }

    const qint64 remaining = static_cast<qint64>(totalLength) - 12;
    if (m_file.read(remaining).size() != remaining)
    {
        errorMessage = "Truncated PCAPNG section header.";
        return false;
    }

    m_interfaces.clear();
    return true;
}

bool PcapFileReader::readNextPcapPacket(RawPacket& packet, QString& errorMessage)
{
    const QByteArray packetHeader = m_file.read(16);

    if (packetHeader.isEmpty() && m_file.atEnd())
    {
        return false;
    }

    if (packetHeader.size() != 16)
    {
        errorMessage = "Truncated PCAP packet header.";
        return false;
    }

    const quint32 tsSec = readU32(packetHeader, 0, m_byteOrder);
    const quint32 tsSubSec = readU32(packetHeader, 4, m_byteOrder);
    const quint32 capturedLength = readU32(packetHeader, 8, m_byteOrder);

    if (capturedLength == 0 || capturedLength > MAX_SINGLE_PACKET_SIZE)
    {
        errorMessage = "Invalid PCAP packet size.";
        return false;
    }

    const QByteArray packetData = m_file.read(capturedLength);
    if (packetData.size() != static_cast<int>(capturedLength))
    {
        errorMessage = "Truncated PCAP packet data.";
        return false;
    }

    ++m_packetCounter;
    packet.packetNumber = m_packetCounter;
    packet.tsSec = tsSec;
    packet.tsUsec = m_nanosecondPcap ? static_cast<quint32>(tsSubSec / 1000U) : tsSubSec;
    packet.linkType = m_linkType;
    packet.data = packetData;
    return true;
}

bool PcapFileReader::readNextPcapNgPacket(RawPacket& packet, QString& errorMessage)
{
    while (!m_file.atEnd())
    {
        const QByteArray blockHeader = m_file.read(8);
        if (blockHeader.isEmpty() && m_file.atEnd())
        {
            return false;
        }

        if (blockHeader.size() != 8)
        {
            errorMessage = "Truncated PCAPNG block header.";
            return false;
        }

        quint32 blockType = readU32(blockHeader, 0, m_byteOrder);
        quint32 totalLength = readU32(blockHeader, 4, m_byteOrder);

        if (blockType == PCAPNG_SECTION_HEADER_BLOCK)
        {
            const qint64 currentPos = m_file.pos();
            const QByteArray nextFour = m_file.peek(4);
            if (nextFour.size() == 4)
            {
                const quint8 a = static_cast<quint8>(nextFour.at(0));
                const quint8 b = static_cast<quint8>(nextFour.at(1));
                const quint8 c = static_cast<quint8>(nextFour.at(2));
                const quint8 d = static_cast<quint8>(nextFour.at(3));

                if (a == 0x1A && b == 0x2B && c == 0x3C && d == 0x4D)
                {
                    m_byteOrder = BigEndian;
                }
                else if (a == 0x4D && b == 0x3C && c == 0x2B && d == 0x1A)
                {
                    m_byteOrder = LittleEndian;
                }
            }
            m_file.seek(currentPos);
            totalLength = readU32(blockHeader, 4, m_byteOrder);
            m_interfaces.clear();
        }

        if (totalLength < 12 || totalLength > MAX_PCAPNG_BLOCK_SIZE)
        {
            errorMessage = "Invalid PCAPNG block length.";
            return false;
        }

        const qint64 remaining = static_cast<qint64>(totalLength) - 8;
        const QByteArray rest = m_file.read(remaining);
        if (rest.size() != remaining)
        {
            errorMessage = "Truncated PCAPNG block.";
            return false;
        }

        if (rest.size() < 4)
        {
            errorMessage = "Invalid PCAPNG block.";
            return false;
        }

        const quint32 trailingLength = readU32(rest, rest.size() - 4, m_byteOrder);
        if (trailingLength != totalLength)
        {
            errorMessage = "PCAPNG block length mismatch.";
            return false;
        }

        const QByteArray blockBody = rest.left(rest.size() - 4);

        if (blockType == PCAPNG_INTERFACE_DESCRIPTION_BLOCK)
        {
            parsePcapNgInterfaceBlock(blockBody);
            continue;
        }

        if (blockType == PCAPNG_ENHANCED_PACKET_BLOCK)
        {
            if (parsePcapNgEnhancedPacketBlock(blockBody, packet))
            {
                return true;
            }
            continue;
        }

        if (blockType == PCAPNG_SIMPLE_PACKET_BLOCK)
        {
            if (parsePcapNgSimplePacketBlock(blockBody, packet))
            {
                return true;
            }
            continue;
        }
    }

    return false;
}

void PcapFileReader::parsePcapNgInterfaceBlock(const QByteArray& blockBody)
{
    if (blockBody.size() < 8)
    {
        return;
    }

    InterfaceInfo info;
    info.linkType = readU16(blockBody, 0, m_byteOrder);
    info.unitsPerSecond = 1000000.0;

    int offset = 8;
    while (offset + 4 <= blockBody.size())
    {
        const quint16 optionCode = readU16(blockBody, offset, m_byteOrder);
        const quint16 optionLength = readU16(blockBody, offset + 2, m_byteOrder);
        offset += 4;

        if (optionCode == 0)
        {
            break;
        }

        if (offset + optionLength > blockBody.size())
        {
            break;
        }

        if (optionCode == 9 && optionLength >= 1)
        {
            const quint8 value = static_cast<quint8>(blockBody.at(offset));
            const int exponent = value & 0x7F;
            if ((value & 0x80) != 0)
            {
                info.unitsPerSecond = std::pow(2.0, static_cast<double>(exponent));
            }
            else
            {
                info.unitsPerSecond = std::pow(10.0, static_cast<double>(exponent));
            }

            if (info.unitsPerSecond <= 0.0)
            {
                info.unitsPerSecond = 1000000.0;
            }
        }

        offset += padded32Length(optionLength);
    }

    m_interfaces.append(info);
}

bool PcapFileReader::parsePcapNgEnhancedPacketBlock(const QByteArray& blockBody, RawPacket& packet)
{
    if (blockBody.size() < 20)
    {
        return false;
    }

    const quint32 interfaceId = readU32(blockBody, 0, m_byteOrder);
    if (interfaceId >= static_cast<quint32>(m_interfaces.size()))
    {
        return false;
    }

    const InterfaceInfo interfaceInfo = m_interfaces.at(static_cast<int>(interfaceId));
    if (interfaceInfo.linkType != LINKTYPE_ETHERNET)
    {
        return false;
    }

    const quint32 timestampHigh = readU32(blockBody, 4, m_byteOrder);
    const quint32 timestampLow = readU32(blockBody, 8, m_byteOrder);
    const quint32 capturedLength = readU32(blockBody, 12, m_byteOrder);

    if (capturedLength == 0 || capturedLength > MAX_SINGLE_PACKET_SIZE)
    {
        return false;
    }

    if (blockBody.size() < 20 + static_cast<int>(capturedLength))
    {
        return false;
    }

    const quint64 rawTimestamp = combineU32ToU64(timestampHigh, timestampLow);
    const double secondsDouble = static_cast<double>(rawTimestamp) / interfaceInfo.unitsPerSecond;
    quint64 tsSec = static_cast<quint64>(secondsDouble);
    quint32 tsUsec = static_cast<quint32>((secondsDouble - static_cast<double>(tsSec)) * 1000000.0 + 0.5);
    if (tsUsec >= 1000000U)
    {
        ++tsSec;
        tsUsec = 0;
    }

    ++m_packetCounter;
    packet.packetNumber = m_packetCounter;
    packet.tsSec = tsSec;
    packet.tsUsec = tsUsec;
    packet.linkType = interfaceInfo.linkType;
    packet.data = blockBody.mid(20, static_cast<int>(capturedLength));
    return true;
}

bool PcapFileReader::parsePcapNgSimplePacketBlock(const QByteArray& blockBody, RawPacket& packet)
{
    if (blockBody.size() < 4)
    {
        return false;
    }

    quint32 linkType = LINKTYPE_ETHERNET;
    if (!m_interfaces.isEmpty())
    {
        linkType = m_interfaces.first().linkType;
    }

    if (linkType != LINKTYPE_ETHERNET)
    {
        return false;
    }

    const quint32 originalPacketLength = readU32(blockBody, 0, m_byteOrder);
    if (originalPacketLength == 0 || originalPacketLength > MAX_SINGLE_PACKET_SIZE)
    {
        return false;
    }

    if (blockBody.size() < 4 + static_cast<int>(originalPacketLength))
    {
        return false;
    }

    ++m_packetCounter;
    packet.packetNumber = m_packetCounter;
    packet.tsSec = 0;
    packet.tsUsec = 0;
    packet.linkType = linkType;
    packet.data = blockBody.mid(4, static_cast<int>(originalPacketLength));
    return true;
}

quint16 PcapFileReader::readU16(const QByteArray& data, int offset, ByteOrder order)
{
    const quint16 b0 = static_cast<quint8>(data.at(offset));
    const quint16 b1 = static_cast<quint8>(data.at(offset + 1));

    if (order == BigEndian)
    {
        return static_cast<quint16>((b0 << 8) | b1);
    }

    return static_cast<quint16>((b1 << 8) | b0);
}

quint32 PcapFileReader::readU32(const QByteArray& data, int offset, ByteOrder order)
{
    const quint32 b0 = static_cast<quint8>(data.at(offset));
    const quint32 b1 = static_cast<quint8>(data.at(offset + 1));
    const quint32 b2 = static_cast<quint8>(data.at(offset + 2));
    const quint32 b3 = static_cast<quint8>(data.at(offset + 3));

    if (order == BigEndian)
    {
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    }

    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

quint64 PcapFileReader::combineU32ToU64(quint32 high, quint32 low)
{
    return (static_cast<quint64>(high) << 32) | static_cast<quint64>(low);
}

int PcapFileReader::padded32Length(int length)
{
    return (length + 3) & ~3;
}
