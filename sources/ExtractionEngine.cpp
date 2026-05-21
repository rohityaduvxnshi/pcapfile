#include "ExtractionEngine.h"

#include "BitfieldDecoder.h"
#include "ConditionalBitfieldDecoder.h"

#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QVarLengthArray>

#include <cstring>

namespace
{
quint64 readUnsignedBigEndianRawValue(const QByteArray& payload, int byteOffsetcorrect, int length)
{
    quint64 rawValue = 0;

    for (int i = 0; i < length; ++i)
    {
        rawValue <<= 8;
        rawValue |= static_cast<quint8>(payload.at(byteOffsetcorrect + i));
    }

    return rawValue;
}

QString formatCalculatedValue(double value)
{
    QString s = QString::number(value, 'f', 6);
    int end = s.size();
    while (end > 0 && s.at(end - 1) == QLatin1Char('0')) --end;
    if (end > 0 && s.at(end - 1) == QLatin1Char('.')) --end;
    s.truncate(end);
    return s;
}

bool shouldApplyResolution(double resolution)
{
    return !qFuzzyCompare(resolution, 1.0);
}

QString formatUnsignedValue(quint64 rawValue, double resolution)
{
    if (shouldApplyResolution(resolution))
        return formatCalculatedValue(static_cast<double>(rawValue) * resolution);

    return QString::number(static_cast<qulonglong>(rawValue));
}

qint64 signExtendRawValue(quint64 rawValue, int bits)
{
    if (bits >= 64)
    {
        qint64 signedValue = 0;
        std::memcpy(&signedValue, &rawValue, sizeof(signedValue));
        return signedValue;
    }

    const quint64 mask = (1ULL << bits) - 1ULL;
    rawValue &= mask;

    const quint64 signBit = 1ULL << (bits - 1);
    if ((rawValue & signBit) == 0)
        return static_cast<qint64>(rawValue);

    const quint64 magnitude = ((~rawValue) & mask) + 1ULL;
    return -static_cast<qint64>(magnitude);
}

QString formatSignedValue(quint64 rawValue, int bits, double resolution)
{
    const qint64 signedValue = signExtendRawValue(rawValue, bits);

    if (shouldApplyResolution(resolution))
        return formatCalculatedValue(static_cast<double>(signedValue) * resolution);

    return QString::number(static_cast<qlonglong>(signedValue));
}

QByteArray fieldBytesFromPayload(const QByteArray& payload, const FieldDefinition& field)
{
    if (field.byteOffsetcorrect < 0 || field.length <= 0) return QByteArray();
    if (field.byteOffsetcorrect + field.length > payload.size()) return QByteArray();
    return payload.mid(field.byteOffsetcorrect, field.length);
}

// Mirrors the switch in ExtractionEngine::valueFromPayload but takes a
// pre-read raw value so callers can avoid decoding the same bytes twice.
// Caller has already validated bounds (offset >= 0, length 1..8, in range).
QString formatRawValue(quint64 rawValue, const FieldDefinition& field)
{
    switch (field.dataType)
    {
    case FieldDataType::RawUnsignedBE:
    case FieldDataType::Uint8:
    case FieldDataType::Uint16:
    case FieldDataType::Uint32:
    case FieldDataType::Uint64:
        return formatUnsignedValue(rawValue, field.resolution);

    case FieldDataType::Int8:
        return formatSignedValue(rawValue, 8, field.resolution);
    case FieldDataType::Int16:
        return formatSignedValue(rawValue, 16, field.resolution);
    case FieldDataType::Int32:
        return formatSignedValue(rawValue, 32, field.resolution);
    case FieldDataType::Int64:
        return formatSignedValue(rawValue, 64, field.resolution);

    case FieldDataType::Float32:
    {
        const quint32 raw32 = static_cast<quint32>(rawValue);
        float value = 0.0f;
        std::memcpy(&value, &raw32, sizeof(value));

        double calculatedValue = static_cast<double>(value);
        if (shouldApplyResolution(field.resolution))
            calculatedValue *= field.resolution;

        return formatCalculatedValue(calculatedValue);
    }

    case FieldDataType::Float64:
    {
        double value = 0.0;
        std::memcpy(&value, &rawValue, sizeof(value));

        if (shouldApplyResolution(field.resolution))
            value *= field.resolution;

        return formatCalculatedValue(value);
    }

    case FieldDataType::Bool:
        return rawValue == 0 ? "false" : "true";
    }

    return "N/A";
}

#ifndef QT_NO_DEBUG
// Counts the total CSV columns produced by valuesFromPayload(fields) without
// allocating any QString. Must stay in sync with ExtractionEngine::columnHeaders
// and ConditionalBitfieldDecoder::columnHeaders.
int computeExpectedColumnCount(const QList<FieldDefinition>& fields)
{
    int total = 0;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);
        total += 1; // main value column

        if (field.hasBitfieldDecoder)
            total += field.bitDecodeRules.size();

        if (field.hasConditionalBitfieldDecoder)
        {
            total += 1; // <DepField>_Profile column
            const ConditionalBitfieldDecoderConfig& cfg = field.conditionalDecoder;
            for (int p = 0; p < cfg.profiles.size(); ++p)
            {
                total += cfg.profiles.at(p).bitDecodeRules.size();
                total += cfg.profiles.at(p).exclusionRules.size();
            }
        }
    }
    return total;
}
#endif
}

QString ExtractionEngine::valueFromPayload(const QByteArray& payload, const FieldDefinition& field)
{
    const int expectedLength = fieldDataTypeNaturalLength(field.dataType);
    if (expectedLength > 0 && field.length != expectedLength)
    {
        return "N/A";
    }

    if (field.byteOffsetcorrect < 0 || field.length <= 0 || field.length > 8)
    {
        return "N/A";
    }

    if (field.byteOffsetcorrect + field.length > payload.size())
    {
        return "N/A";
    }

    const quint64 rawDecimalValue = readUnsignedBigEndianRawValue(payload, field.byteOffsetcorrect, field.length);

    switch (field.dataType)
    {
    case FieldDataType::RawUnsignedBE:
    case FieldDataType::Uint8:
    case FieldDataType::Uint16:
    case FieldDataType::Uint32:
    case FieldDataType::Uint64:
        return formatUnsignedValue(rawDecimalValue, field.resolution);

    case FieldDataType::Int8:
        return formatSignedValue(rawDecimalValue, 8, field.resolution);
    case FieldDataType::Int16:
        return formatSignedValue(rawDecimalValue, 16, field.resolution);
    case FieldDataType::Int32:
        return formatSignedValue(rawDecimalValue, 32, field.resolution);
    case FieldDataType::Int64:
        return formatSignedValue(rawDecimalValue, 64, field.resolution);

    case FieldDataType::Float32:
    {
        const quint32 raw32 = static_cast<quint32>(rawDecimalValue);
        float value = 0.0f;
        std::memcpy(&value, &raw32, sizeof(value));

        double calculatedValue = static_cast<double>(value);
        if (shouldApplyResolution(field.resolution))
            calculatedValue *= field.resolution;

        return formatCalculatedValue(calculatedValue);
    }

    case FieldDataType::Float64:
    {
        double value = 0.0;
        std::memcpy(&value, &rawDecimalValue, sizeof(value));

        if (shouldApplyResolution(field.resolution))
            value *= field.resolution;

        return formatCalculatedValue(value);
    }

    case FieldDataType::Bool:
        return rawDecimalValue == 0 ? "false" : "true";
    }

    return "N/A";
}

QStringList ExtractionEngine::valuesFromPayload(const QByteArray& payload, const QList<FieldDefinition>& fields)
{
    const int fieldCount = fields.size();

    // Phase 1: read each field's raw quint64 ONCE into indexed buffers,
    // so the main value pass and the conditional decoder's controller
    // lookup can share the same decoded numbers without redoing the
    // big-endian read or allocating a string-keyed QMap per row.
    QVarLengthArray<quint64, 16> raw(fieldCount);
    QVarLengthArray<bool, 16> ok(fieldCount);
    QHash<QString, int> nameToIndex;
    nameToIndex.reserve(fieldCount);

    for (int i = 0; i < fieldCount; ++i)
    {
        const FieldDefinition& field = fields.at(i);
        // Last write wins, matching the old QMap insert semantics.
        nameToIndex.insert(field.name, i);

        if (field.byteOffsetcorrect >= 0 && field.length > 0 && field.length <= 8
            && field.byteOffsetcorrect + field.length <= payload.size())
        {
            raw[i] = readUnsignedBigEndianRawValue(payload, field.byteOffsetcorrect, field.length);
            ok[i] = true;
        }
        else
        {
            raw[i] = 0;
            ok[i] = false;
        }
    }

    // Phase 2: build output row — order must match columnHeaders() exactly.
    QStringList values;
    for (int i = 0; i < fieldCount; ++i)
    {
        const FieldDefinition& field = fields.at(i);

        // Main resolved value — preserves valueFromPayload() semantics:
        //   * out-of-bounds / length<=0 / length>8 -> "N/A"
        //   * typed field with length != natural length -> "N/A"
        //   * otherwise format from the pre-read raw value.
        if (!ok[i])
        {
            values << QStringLiteral("N/A");
        }
        else
        {
            const int expectedLength = fieldDataTypeNaturalLength(field.dataType);
            if (expectedLength > 0 && field.length != expectedLength)
                values << QStringLiteral("N/A");
            else
                values << formatRawValue(raw[i], field);
        }

        // Static bitfield decoder (unchanged behavior)
        if (field.hasBitfieldDecoder)
        {
            const QByteArray fieldBytes = fieldBytesFromPayload(payload, field);
            for (int r = 0; r < field.bitDecodeRules.size(); ++r)
            {
                if (fieldBytes.isEmpty())
                    values << QString();
                else
                    values << BitfieldDecoder::decodeRule(fieldBytes, field.bitDecodeRules.at(r));
            }
        }

        // Conditional bitfield decoder
        if (field.hasConditionalBitfieldDecoder)
        {
            const QString& ctrlName = field.conditionalDecoder.controllerFieldName;
            const int ctrlIndex = nameToIndex.value(ctrlName, -1);
            const bool ctrlFound = (ctrlIndex >= 0) && ok[ctrlIndex];
            const quint64 ctrlVal = ctrlFound ? raw[ctrlIndex] : 0;
            const QByteArray depBytes = fieldBytesFromPayload(payload, field);

            values += ConditionalBitfieldDecoder::decode(depBytes, ctrlVal, ctrlFound, field.conditionalDecoder);
        }
    }

#ifndef QT_NO_DEBUG
    Q_ASSERT(values.size() == computeExpectedColumnCount(fields));
#endif

    return values;
}

QStringList ExtractionEngine::columnHeaders(const QList<FieldDefinition>& fields)
{
    QStringList headers;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);

        headers << field.name;

        if (field.hasBitfieldDecoder)
        {
            for (int r = 0; r < field.bitDecodeRules.size(); ++r)
            {
                const BitDecodeRule& rule = field.bitDecodeRules.at(r);
                headers << field.name + "_" + BitfieldDecoder::sanitizeColumnLabel(rule.label);
            }
        }

        if (field.hasConditionalBitfieldDecoder)
        {
            headers += ConditionalBitfieldDecoder::columnHeaders(field.name, field.conditionalDecoder);
        }
    }
    return headers;
}
