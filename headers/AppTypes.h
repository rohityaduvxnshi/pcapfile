#ifndef APPTYPES_H
#define APPTYPES_H

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct BitDecodeRule
{
    QString label;
    QList<int> bitPositions;
    QMap<quint64, QString> valueMeanings;
    bool reserved;
    QString unknownBehavior;
    bool enabled;

    BitDecodeRule()
        : reserved(false),
          unknownBehavior("UNKNOWN"),
          enabled(true)
    {
    }
};

struct ConditionalBitDecodeProfile
{
    QString profileName;
    quint64 controllerValue;
    QList<BitDecodeRule> bitDecodeRules;

    ConditionalBitDecodeProfile()
        : controllerValue(0)
    {
    }
};

struct ConditionalBitfieldDecoderConfig
{
    QString controllerFieldName;
    QString unknownBehavior;
    QList<ConditionalBitDecodeProfile> profiles;

    ConditionalBitfieldDecoderConfig()
        : unknownBehavior("UNKNOWN_CONTROLLER")
    {
    }
};

struct FieldDefinition
{
    QString name;
    int byteOffset;
    int length;
    double resolution;
    QString resolutionExpression;
    bool hasBitfieldDecoder;
    QList<BitDecodeRule> bitDecodeRules;
    bool hasConditionalBitfieldDecoder;
    ConditionalBitfieldDecoderConfig conditionalDecoder;

    FieldDefinition()
        : byteOffset(0),
          length(1),
          resolution(1.0),
          resolutionExpression("1"),
          hasBitfieldDecoder(false),
          hasConditionalBitfieldDecoder(false)
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
