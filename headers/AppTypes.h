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

struct ConditionalBitExclusionRule
{
    QList<int> mutuallyExclusiveBits;
    QString validationLabel;
    QString invalidMessage;
};

struct ConditionalBitDecodeProfile
{
    QString profileName;
    quint64 controllerValue;
    QList<BitDecodeRule> bitDecodeRules;
    QList<ConditionalBitExclusionRule> exclusionRules;

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

enum class FieldDataType
{
    RawUnsignedBE = 0,
    Uint8,
    Int8,
    Uint16,
    Int16,
    Uint32,
    Int32,
    Uint64,
    Int64,
    Float32,
    Float64,
    Bool
};

inline int fieldDataTypeNaturalLength(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::Bool:
    case FieldDataType::Uint8:
    case FieldDataType::Int8:
        return 1;
    case FieldDataType::Uint16:
    case FieldDataType::Int16:
        return 2;
    case FieldDataType::Uint32:
    case FieldDataType::Int32:
    case FieldDataType::Float32:
        return 4;
    case FieldDataType::Uint64:
    case FieldDataType::Int64:
    case FieldDataType::Float64:
        return 8;
    case FieldDataType::RawUnsignedBE:
    default:
        return 0;
    }
}

inline bool fieldDataTypeHasFixedLength(FieldDataType dataType)
{
    return fieldDataTypeNaturalLength(dataType) > 0;
}

struct FieldDefinition
{
    QString name;
    int byteOffset;
    int byteOffsetcorrect;
    int length;
    FieldDataType dataType;
    double resolution;
    QString resolutionExpression;
    bool hasBitfieldDecoder;
    QList<BitDecodeRule> bitDecodeRules;
    bool hasConditionalBitfieldDecoder;
    ConditionalBitfieldDecoderConfig conditionalDecoder;

    FieldDefinition()
        : byteOffset(0),
          byteOffsetcorrect(0),
          length(1),
          dataType(FieldDataType::RawUnsignedBE),
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
