#ifndef PCAPFILEREADER_H
#define PCAPFILEREADER_H

#include "AppTypes.h"

#include <QFile>
#include <QVector>

class PcapFileReader
{
public:
    PcapFileReader();
    ~PcapFileReader();

    bool open(const QString& filePath, QString& errorMessage);
    bool readNextPacket(RawPacket& packet, QString& errorMessage);
    void close();
    bool isOpen() const;
    QString formatName() const;

private:
    enum CaptureFormat
    {
        FormatUnknown,
        FormatPcap,
        FormatPcapNg
    };

    enum ByteOrder
    {
        BigEndian,
        LittleEndian
    };

    struct InterfaceInfo
    {
        quint32 linkType;
        double unitsPerSecond;

        InterfaceInfo()
            : linkType(1),
              unitsPerSecond(1000000.0)
        {
        }
    };

    bool readNextPcapPacket(RawPacket& packet, QString& errorMessage);
    bool readNextPcapNgPacket(RawPacket& packet, QString& errorMessage);
    bool parsePcapGlobalHeader(const QByteArray& header, QString& errorMessage);
    bool parsePcapNgSectionHeader(const QByteArray& firstBytes, QString& errorMessage);
    void parsePcapNgInterfaceBlock(const QByteArray& blockBody);
    bool parsePcapNgEnhancedPacketBlock(const QByteArray& blockBody, RawPacket& packet);
    bool parsePcapNgSimplePacketBlock(const QByteArray& blockBody, RawPacket& packet);

    static quint16 readU16(const QByteArray& data, int offset, ByteOrder order);
    static quint32 readU32(const QByteArray& data, int offset, ByteOrder order);
    static quint64 combineU32ToU64(quint32 high, quint32 low);
    static int padded32Length(int length);

    QFile m_file;
    CaptureFormat m_format;
    ByteOrder m_byteOrder;
    quint32 m_linkType;
    bool m_nanosecondPcap;
    quint64 m_packetCounter;
    QVector<InterfaceInfo> m_interfaces;
};

#endif // PCAPFILEREADER_H
