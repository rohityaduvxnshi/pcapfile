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
    Bool,
    String
};

// Simulator: per-field byte order on the wire. Big is the historical behaviour
// (the encode contract with the parser) and stays the default for every field;
// Little reverses the byte order of the numeric/float bytes only and is opt-in.
enum class FieldEndianness
{
    Big = 0,
    Little = 1
};

// Endianness is meaningful only for multi-byte numeric encodings. String bytes
// are never reversed; Bool is effectively single-byte. Used to decide whether a
// Little selection actually changes the wire bytes.
inline bool fieldDataTypeIsByteswappable(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::Bool:
    case FieldDataType::String:
        return false;
    default:
        return true;
    }
}

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
    case FieldDataType::String:
    default:
        return 0;
    }
}

inline bool fieldDataTypeHasFixedLength(FieldDataType dataType)
{
    return fieldDataTypeNaturalLength(dataType) > 0;
}

// Byte<->word DISPLAY conversions for a message's offset unit (1 word = 2 bytes,
// 1-based). byteOffset is always the canonical stored value; WORDS just shows it
// as ((byteOffset-1)/2)+1 and converts entry back. Field LENGTH is never
// converted — it always stays in bytes.
inline bool offsetUnitIsWords(const QString& unit)
{
    return unit.compare(QStringLiteral("WORDS"), Qt::CaseInsensitive) == 0;
}

inline int byteOffsetToUnit(int byteOffset, const QString& unit)
{
    if (offsetUnitIsWords(unit))
        return ((byteOffset - 1) / 2) + 1;
    return byteOffset;
}

inline int unitToByteOffset(int unitValue, const QString& unit)
{
    if (offsetUnitIsWords(unit))
        return ((unitValue - 1) * 2) + 1;
    return unitValue;
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

    // NMEA: when this field originates from an NMEA sentence (a message whose
    // dataFormat == "NMEA"), nmeaFieldIndex carries the 1-based comma position
    // of the token within the sentence. For all Hex fields this stays 0 and the
    // byteOffset / length / dataType Hex path applies. When non-zero, those Hex
    // members are ignored and the value comes from NmeaDecoder instead.
    int nmeaFieldIndex;

    // NMEA: value kind used to format this field, as the integer value of
    // NmeaValueKind (0 = Text). For predefined sentences the formatting comes
    // from the registry; this is authoritative only for CUSTOM sentences (a
    // formatter not in NmeaSentenceRegistry), where there is no registry entry
    // to consult. Default 0 (Text) for all Hex fields.
    int nmeaValueKind;

    // Simulator: the value to TRANSMIT for this field, entered in the field's
    // own type ("12.5" for a scaled ushort, "hello" for a String, "4807.038"
    // for an NMEA latitude token). PayloadBuilder encodes it to wire bytes as
    // raw = round(value / resolution), big-endian — the exact inverse of the
    // parser's raw * resolution decode. Empty = not yet entered.
    QString sendValueText;

    // Simulator: byte order this field is transmitted in. Default Big preserves
    // the parser encode contract; Little reverses the numeric/float bytes only.
    FieldEndianness endianness;

    FieldDefinition()
        : byteOffset(0),
          byteOffsetcorrect(0),
          length(1),
          dataType(FieldDataType::RawUnsignedBE),
          resolution(1.0),
          resolutionExpression("1"),
          hasBitfieldDecoder(false),
          hasConditionalBitfieldDecoder(false),
          nmeaFieldIndex(0),
          nmeaValueKind(0),
          endianness(FieldEndianness::Big)
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

// v13: per-message compare/verification config. Each section's enable flag controls
// whether its observed/computed column appears in CSV. Expected-value inputs are
// optional within each section — when blank/zero, the section logs the observed
// value without the OK comparison column. See plan v13.
struct CompareOptionsConfig
{
    // Header check
    bool       checkHeader;
    int        headerByteOffset;       // 0-based
    int        headerLength;
    QByteArray expectedHeader;         // decoded bytes; empty = log-only
    QString    headerInputMode;        // "HEX" or "ASCII"
    QString    expectedHeaderText;     // raw user input preserved for round-trip

    // Terminator check
    bool       checkTerminator;
    int        terminatorByteOffset;   // 0-based; -1 = "from end" (payload.size - len)
    int        terminatorLength;
    QByteArray expectedTerminator;
    QString    terminatorInputMode;
    QString    expectedTerminatorText;

    // Checksum check (comparison is always vs the stored byte in payload)
    bool       checkChecksum;
    QString    checksumAlgorithm;      // "XOR" or "SUM"
    int        checksumRangeStart;     // 0-based inclusive
    int        checksumRangeEnd;       // 0-based exclusive
    int        checksumByteOffset;     // 0-based location of stored checksum
    int        checksumLength;         // 1 or 2

    // Refresh rate check (rolling 1-second window)
    bool       checkRefreshRate;
    double     expectedRefreshRateHz;  // 0.0 = log-only
    double     refreshRateToleranceHz;

    // Endianness check
    bool       checkEndianness;
    QString    expectedEndianness;     // "BIG", "LITTLE", or "" = log-only

    // ICD data-length check: a stored length field vs the actual payload size.
    bool       checkDataLength;
    int        dataLengthByteOffset;   // 0-based location of the length field
    int        dataLengthSize;         // 1/2/4 bytes (big-endian)
    int        dataLengthAdjust;       // bytes NOT counted by the field; computed = payload.size - adjust

    // ICD message-ID check: a value at an offset vs an expected ID.
    bool       checkMessageId;
    int        messageIdByteOffset;    // 0-based
    int        messageIdSize;          // 1/2/4 bytes (big-endian)
    quint64    expectedMessageId;

    CompareOptionsConfig()
        : checkHeader(false), headerByteOffset(0), headerLength(0),
          headerInputMode("HEX"),
          checkTerminator(false), terminatorByteOffset(-1), terminatorLength(0),
          terminatorInputMode("HEX"),
          checkChecksum(false), checksumAlgorithm("XOR"),
          checksumRangeStart(0), checksumRangeEnd(0),
          checksumByteOffset(0), checksumLength(1),
          checkRefreshRate(false), expectedRefreshRateHz(0.0), refreshRateToleranceHz(1.0),
          checkEndianness(false), expectedEndianness("BIG"),
          checkDataLength(false), dataLengthByteOffset(0), dataLengthSize(2), dataLengthAdjust(0),
          checkMessageId(false), messageIdByteOffset(0), messageIdSize(2), expectedMessageId(0)
    {
    }
};

#endif // APPTYPES_H
