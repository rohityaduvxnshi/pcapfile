#include "ExtractionEngine.h"

#include "BitfieldDecoder.h"
#include "ConditionalBitfieldDecoder.h"

#include <QMap>
#include <QRegularExpression>

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
    return QString::number(value, 'f', 6)
        .remove(QRegularExpression("0+$"))
        .remove(QRegularExpression("\\.$"));
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
    // Phase 1: read all raw quint64 values so conditional decoders can look up
    // their controller field regardless of field order in the list.
    QMap<QString, quint64> rawValues;
    QMap<QString, bool> fieldValid;

    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);
        if (field.byteOffsetcorrect >= 0 && field.length > 0 && field.length <= 8
            && field.byteOffsetcorrect + field.length <= payload.size())
        {
            rawValues[field.name] = readUnsignedBigEndianRawValue(payload, field.byteOffsetcorrect, field.length);
            fieldValid[field.name] = true;
        }
    }

    // Phase 2: build output row — order must match columnHeaders() exactly.
    QStringList values;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);

        // Main resolved value (unchanged behavior)
        values << valueFromPayload(payload, field);

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
            const QString ctrlName = field.conditionalDecoder.controllerFieldName;
            const quint64 ctrlVal = rawValues.value(ctrlName, 0);
            const bool ctrlFound = fieldValid.value(ctrlName, false);
            const QByteArray depBytes = fieldBytesFromPayload(payload, field);

            values += ConditionalBitfieldDecoder::decode(depBytes, ctrlVal, ctrlFound, field.conditionalDecoder);
        }
    }

    Q_ASSERT(values.size() == columnHeaders(fields).size());

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
