#include "ExtractionEngine.h"

#include <QRegExp>

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
        .remove(QRegExp("0+$"))
        .remove(QRegExp("\\.$"));
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
    QStringList values;
    for (int i = 0; i < fields.size(); ++i)
    {
        values << valueFromPayload(payload, fields.at(i));
    }

    return values;
}
