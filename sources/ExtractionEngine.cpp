#include "ExtractionEngine.h"

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

    quint64 rawValue = 0;
    for (int i = 0; i < field.length; ++i)
    {
        rawValue <<= 8;
        rawValue |= static_cast<quint8>(payload.at(field.byteOffset + i));
    }

    const double finalValue = static_cast<double>(rawValue) * field.resolution;

    if (field.resolution == 1.0)
    {
        return QString::number(static_cast<qulonglong>(rawValue));
    }

    return QString::number(finalValue, 'f', 6).remove(QRegExp("0+$")).remove(QRegExp("\\.$"));
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
