#include "ExtractionEngine.h"

#include "BitfieldDecoder.h"
#include "ConditionalBitfieldDecoder.h"

#include <QMap>
#include <QRegularExpression>

namespace
{
quint64 readUnsignedBigEndianRawValue(const QByteArray& payload, int byteOffset, int length)
{
    quint64 rawValue = 0;

    for (int i = 0; i < length; ++i)
    {
        rawValue <<= 8;
        rawValue |= static_cast<quint8>(payload.at(byteOffset + i));
    }

    return rawValue;
}

QString formatCalculatedValue(double value)
{
    return QString::number(value, 'f', 6)
        .remove(QRegularExpression("0+$"))
        .remove(QRegularExpression("\\.$"));
}

QByteArray fieldBytesFromPayload(const QByteArray& payload, const FieldDefinition& field)
{
    if (field.byteOffset < 0 || field.length <= 0) return QByteArray();
    if (field.byteOffset + field.length > payload.size()) return QByteArray();
    return payload.mid(field.byteOffset, field.length);
}
}

QString ExtractionEngine::valueFromPayload(const QByteArray& payload, const FieldDefinition& field)
{
    if (field.byteOffset < 0 || field.length <= 0 || field.length > 8)
    {
        return "N/A";
    }

    if (field.byteOffset + field.length > payload.size())
    {
        return "N/A";
    }

    // Step 1: Read selected PCAP/PCAPNG UDP payload bytes as raw hex bytes.
    // Step 2: Convert those bytes to an unsigned decimal integer using big-endian order.
    // Example: bytes 0x16 0x05 become decimal 5637.
    const quint64 rawDecimalValue = readUnsignedBigEndianRawValue(payload, field.byteOffset, field.length);

    // Step 3: Multiply the raw decimal value by the already solved resolution value.
    // Example: 5637 * (180 / 2^15) = 30.9649658203125.
    const double calculatedValue = static_cast<double>(rawDecimalValue) * field.resolution;

    // Step 4: Store/display the final calculated value in the output field / CSV cell.
    if (field.resolution == 1.0)
    {
        return QString::number(static_cast<qulonglong>(rawDecimalValue));
    }

    return formatCalculatedValue(calculatedValue);
}

QStringList ExtractionEngine::valuesFromPayload(const QByteArray& payload, const QList<FieldDefinition>& fields)
{
    // Phase 1: build raw value map for controller field lookups
    QMap<QString, quint64> rawValues;
    QMap<QString, bool> fieldValid;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);
        if (field.byteOffset >= 0 && field.length > 0 && field.length <= 8
            && field.byteOffset + field.length <= payload.size())
        {
            rawValues[field.name] = readUnsignedBigEndianRawValue(payload, field.byteOffset, field.length);
            fieldValid[field.name] = true;
        }
    }

    // Phase 2: build output row
    QStringList values;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);
        values << valueFromPayload(payload, field);

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

        if (field.hasConditionalBitfieldDecoder)
        {
            const QString ctrlName = field.conditionalDecoder.controllerFieldName;
            const quint64 ctrlVal = rawValues.value(ctrlName, 0);
            const bool ctrlFound = fieldValid.value(ctrlName, false);
            const QByteArray depBytes = fieldBytesFromPayload(payload, field);
            values += ConditionalBitfieldDecoder::decode(depBytes, ctrlVal, ctrlFound, field.conditionalDecoder);
        }
    }

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
                if (!rule.enabled) continue;
                headers << QString("%1_%2").arg(field.name).arg(BitfieldDecoder::sanitizeColumnLabel(rule.label));
            }
        }

        if (field.hasConditionalBitfieldDecoder)
            headers += ConditionalBitfieldDecoder::columnHeaders(field.name, field.conditionalDecoder);
    }
    return headers;
}
