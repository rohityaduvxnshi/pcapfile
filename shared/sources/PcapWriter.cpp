#include "PcapWriter.h"

#include <QHostAddress>

namespace
{
// --- little-endian append helpers (pcapng block fields) --------------------
void appendLE16(QByteArray& out, quint16 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
}

void appendLE32(QByteArray& out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 24) & 0xFF));
}

// --- big-endian append helpers (network/wire fields) -----------------------
void appendBE16(QByteArray& out, quint16 v)
{
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>(v & 0xFF));
}

void appendBE32(QByteArray& out, quint32 v)
{
    out.append(static_cast<char>((v >> 24) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>(v & 0xFF));
}

// IPv4 string -> 32-bit address (host order). Invalid text -> 0.
quint32 ipv4Value(const QString& text)
{
    QHostAddress addr;
    if (!addr.setAddress(text.trimmed()))
        return 0;
    return addr.toIPv4Address();
}

// Standard ones-complement checksum over a byte range (IPv4 / TCP).
quint16 onesComplementSum(const QByteArray& data)
{
    quint32 sum = 0;
    const int n = data.size();
    for (int i = 0; i + 1 < n; i += 2)
        sum += (static_cast<quint16>(static_cast<quint8>(data.at(i))) << 8)
               | static_cast<quint8>(data.at(i + 1));
    if (n & 1)
        sum += static_cast<quint16>(static_cast<quint8>(data.at(n - 1))) << 8;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<quint16>(~sum & 0xFFFF);
}

QByteArray ethernetHeader()
{
    QByteArray eth;
    // Destination MAC 02:00:00:00:00:02, source MAC 02:00:00:00:00:01 (both
    // locally-administered unicast), EtherType 0x0800 = IPv4.
    static const char dst[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
    static const char src[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
    eth.append(dst, 6);
    eth.append(src, 6);
    appendBE16(eth, 0x0800);
    return eth;
}

// Build the 20-byte IPv4 header with a correct header checksum.
QByteArray ipv4Header(quint8 protocol, quint16 transportPlusPayloadLen,
                      const QString& srcIp, const QString& dstIp)
{
    QByteArray ip;
    ip.append(static_cast<char>(0x45));                 // version 4, IHL 5
    ip.append(static_cast<char>(0x00));                 // DSCP/ECN
    appendBE16(ip, static_cast<quint16>(20 + transportPlusPayloadLen)); // total length
    appendBE16(ip, 0x0000);                             // identification
    appendBE16(ip, 0x4000);                             // flags: Don't Fragment
    ip.append(static_cast<char>(64));                   // TTL
    ip.append(static_cast<char>(protocol));             // protocol
    appendBE16(ip, 0x0000);                             // checksum placeholder
    appendBE32(ip, ipv4Value(srcIp));
    appendBE32(ip, ipv4Value(dstIp));
    const quint16 sum = onesComplementSum(ip);
    ip[10] = static_cast<char>((sum >> 8) & 0xFF);
    ip[11] = static_cast<char>(sum & 0xFF);
    return ip;
}
}

QByteArray PcapFrame::buildEthIpUdp(const QString& srcIp, quint16 srcPort,
                                    const QString& dstIp, quint16 dstPort,
                                    const QByteArray& payload)
{
    const quint16 udpLen = static_cast<quint16>(8 + payload.size());

    QByteArray udp;
    appendBE16(udp, srcPort);
    appendBE16(udp, dstPort);
    appendBE16(udp, udpLen);
    appendBE16(udp, 0x0000); // checksum 0 (legal for IPv4 UDP)

    QByteArray frame = ethernetHeader();
    frame += ipv4Header(17, udpLen, srcIp, dstIp);
    frame += udp;
    frame += payload;
    return frame;
}

QByteArray PcapFrame::buildEthIpTcp(const QString& srcIp, quint16 srcPort,
                                    const QString& dstIp, quint16 dstPort,
                                    const QByteArray& payload)
{
    const quint16 tcpLen = static_cast<quint16>(20 + payload.size());

    QByteArray tcp;
    appendBE16(tcp, srcPort);
    appendBE16(tcp, dstPort);
    appendBE32(tcp, 1);          // sequence number (placeholder)
    appendBE32(tcp, 1);          // acknowledgement number
    tcp.append(static_cast<char>(0x50)); // data offset 5 (20 bytes), reserved 0
    tcp.append(static_cast<char>(0x18)); // flags: PSH | ACK
    appendBE16(tcp, 0xFFFF);     // window
    appendBE16(tcp, 0x0000);     // checksum placeholder
    appendBE16(tcp, 0x0000);     // urgent pointer
    tcp += payload;

    // TCP checksum over the pseudo-header + segment.
    QByteArray pseudo;
    appendBE32(pseudo, ipv4Value(srcIp));
    appendBE32(pseudo, ipv4Value(dstIp));
    pseudo.append(static_cast<char>(0x00));
    pseudo.append(static_cast<char>(6)); // protocol TCP
    appendBE16(pseudo, tcpLen);
    const quint16 sum = onesComplementSum(pseudo + tcp);
    tcp[16] = static_cast<char>((sum >> 8) & 0xFF);
    tcp[17] = static_cast<char>(sum & 0xFF);

    QByteArray frame = ethernetHeader();
    frame += ipv4Header(6, tcpLen, srcIp, dstIp);
    frame += tcp;
    return frame;
}

PcapWriter::PcapWriter()
    : m_open(false)
{
}

PcapWriter::~PcapWriter()
{
    close();
}

bool PcapWriter::openPcapng(const QString& path, QString& errorMessage)
{
    close();

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        errorMessage = QString("Cannot open '%1' for writing: %2. "
                               "Solution: pick a folder you can write to and make sure the "
                               "file is not open in another program.")
                           .arg(path, m_file.errorString());
        return false;
    }

    // Section Header Block (type 0x0A0D0D0A).
    QByteArray shb;
    appendLE32(shb, 0x0A0D0D0A);
    appendLE32(shb, 28);            // total length
    appendLE32(shb, 0x1A2B3C4D);    // byte-order magic (little-endian here)
    appendLE16(shb, 1);             // major version
    appendLE16(shb, 0);             // minor version
    appendLE32(shb, 0xFFFFFFFF);    // section length (unknown) - low 32
    appendLE32(shb, 0xFFFFFFFF);    // section length (unknown) - high 32
    appendLE32(shb, 28);            // total length (trailer)

    // Interface Description Block (type 0x00000001), LINKTYPE_ETHERNET = 1.
    QByteArray idb;
    appendLE32(idb, 0x00000001);
    appendLE32(idb, 20);            // total length
    appendLE16(idb, 1);             // link type (Ethernet)
    appendLE16(idb, 0);             // reserved
    appendLE32(idb, 262144);        // snaplen
    appendLE32(idb, 20);            // total length (trailer)

    if (m_file.write(shb) != shb.size() || m_file.write(idb) != idb.size())
    {
        errorMessage = QString("Failed to write the capture header to '%1': %2. "
                               "Solution: ensure there is free disk space and try again.")
                           .arg(path, m_file.errorString());
        m_file.close();
        return false;
    }

    m_open = true;
    return true;
}

bool PcapWriter::writePacket(quint64 tsUsec, const QByteArray& frameBytes, QString& errorMessage)
{
    if (!m_open)
    {
        errorMessage = "The capture file is not open. Solution: call openPcapng() first.";
        return false;
    }

    const int capLen = frameBytes.size();
    const int pad = (4 - (capLen % 4)) % 4;
    const quint32 blockLen = static_cast<quint32>(32 + capLen + pad);

    QByteArray epb;
    appendLE32(epb, 0x00000006);                 // Enhanced Packet Block
    appendLE32(epb, blockLen);
    appendLE32(epb, 0);                          // interface id
    appendLE32(epb, static_cast<quint32>(tsUsec >> 32));        // timestamp high
    appendLE32(epb, static_cast<quint32>(tsUsec & 0xFFFFFFFF)); // timestamp low
    appendLE32(epb, static_cast<quint32>(capLen)); // captured length
    appendLE32(epb, static_cast<quint32>(capLen)); // original length
    epb.append(frameBytes);
    for (int i = 0; i < pad; ++i)
        epb.append(static_cast<char>(0));
    appendLE32(epb, blockLen);                   // total length (trailer)

    if (m_file.write(epb) != epb.size())
    {
        errorMessage = QString("Failed to write a packet to the capture file: %1. "
                               "Solution: ensure there is free disk space and try again.")
                           .arg(m_file.errorString());
        return false;
    }
    return true;
}

void PcapWriter::close()
{
    if (m_file.isOpen())
        m_file.close();
    m_open = false;
}

bool PcapWriter::isOpen() const
{
    return m_open;
}
