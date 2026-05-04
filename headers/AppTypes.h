#ifndef APPTYPES_H
#define APPTYPES_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct FieldDefinition
{
    QString name;
    int byteOffset;
    int length;
    double resolution;

    FieldDefinition()
        : byteOffset(0),
          length(1),
          resolution(1.0)
    {
    }
};

struct RawPacket
{
    quint64 packetNumber;
    quint64 tsSec;
    quint32 tsUsec;
    quint32 linkType;
    QByteArray data;

    RawPacket()
        : packetNumber(0),
          tsSec(0),
          tsUsec(0),
          linkType(1)
    {
    }
};

struct ParsedUdpPacket
{
    bool valid;
    QString timestamp;
    QString sourceIp;
    QString destinationIp;
    int sourcePort;
    int destinationPort;
    int payloadSize;
    QByteArray udpPayload;
    QString error;

    ParsedUdpPacket()
        : valid(false),
          sourcePort(-1),
          destinationPort(-1),
          payloadSize(0)
    {
    }
};

#endif // APPTYPES_H
