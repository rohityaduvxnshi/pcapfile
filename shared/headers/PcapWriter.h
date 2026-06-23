#ifndef PCAPWRITER_H
#define PCAPWRITER_H

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtGlobal>

// Pure-Qt capture writer. Records synthesized network frames into a .pcapng file
// that both Wireshark AND this suite's own PcapFileReader can read back. A frame
// is Ethernet II + IPv4 + (UDP|TCP) wrapped around an application payload, so a
// payload the simulator transmits round-trips through the parser unchanged
// (the parser's UdpPacketParser strips exactly this Eth/IPv4/UDP framing).
//
// PcapFrame builds the wire bytes; PcapWriter streams them into a pcapng file
// using link type LINKTYPE_ETHERNET (1) and microsecond timestamps (the reader's
// default tsresol), so no per-interface options are needed. All multi-byte fields
// are written with explicit byte stores, so the code is endian-agnostic.

namespace PcapFrame
{
// Build a complete Ethernet II + IPv4 + UDP frame around `payload`. srcIp/dstIp
// are dotted IPv4 strings (invalid → 0.0.0.0). Synthetic locally-administered
// MACs are used. The IPv4 header checksum is computed; the UDP checksum is left 0
// (legal for IPv4). Returns the full frame ready to hand to PcapWriter::writePacket.
QByteArray buildEthIpUdp(const QString& srcIp, quint16 srcPort,
                         const QString& dstIp, quint16 dstPort,
                         const QByteArray& payload);

// Same, but a single TCP PSH/ACK segment (sequence/ack are fixed placeholders).
// The IPv4 and TCP checksums are both computed so Wireshark dissects cleanly.
QByteArray buildEthIpTcp(const QString& srcIp, quint16 srcPort,
                         const QString& dstIp, quint16 dstPort,
                         const QByteArray& payload);
}

class PcapWriter
{
public:
    PcapWriter();
    ~PcapWriter();

    // Open `path` and write the pcapng Section Header Block + one Interface
    // Description Block (LINKTYPE_ETHERNET). Returns false with reason + solution.
    bool openPcapng(const QString& path, QString& errorMessage);

    // Append one Enhanced Packet Block. tsUsec = microseconds since the Unix
    // epoch; `frameBytes` is a full Ethernet frame (see PcapFrame).
    bool writePacket(quint64 tsUsec, const QByteArray& frameBytes, QString& errorMessage);

    void close();
    bool isOpen() const;

private:
    QFile m_file;
    bool m_open;
};

#endif // PCAPWRITER_H
