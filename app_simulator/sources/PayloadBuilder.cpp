#include "PayloadBuilder.h"

#include "NmeaSentenceRegistry.h"

#include <algorithm>
#include <cstring>

namespace
{
// Same display rounding as the parser's ExtractionEngine so a value typed
// here reads back identically after a decode round-trip.
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

quint64 maskForLength(int length)
{
    if (length >= 8)
        return ~0ULL;
    return (1ULL << (8 * length)) - 1ULL;
}

// Inverse counterpart of the parser's signExtendRawValue.
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

// UI label for error texts; mirrors the type combo labels.
QString typeDisplayName(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::RawUnsignedBE: return "Raw Unsigned BE";
    case FieldDataType::Uint8:         return "uchar";
    case FieldDataType::Int8:          return "char";
    case FieldDataType::Uint16:        return "ushort";
    case FieldDataType::Int16:         return "short";
    case FieldDataType::Uint32:        return "uint";
    case FieldDataType::Int32:         return "int";
    case FieldDataType::Uint64:        return "ulong";
    case FieldDataType::Int64:         return "long";
    case FieldDataType::Float32:       return "float";
    case FieldDataType::Float64:       return "double";
    case FieldDataType::Bool:          return "bool";
    case FieldDataType::String:        return "string";
    }
    return "unknown";
}

bool isUnsignedFamily(FieldDataType dataType)
{
    return dataType == FieldDataType::RawUnsignedBE
        || dataType == FieldDataType::Uint8
        || dataType == FieldDataType::Uint16
        || dataType == FieldDataType::Uint32
        || dataType == FieldDataType::Uint64;
}

bool isSignedFamily(FieldDataType dataType)
{
    return dataType == FieldDataType::Int8
        || dataType == FieldDataType::Int16
        || dataType == FieldDataType::Int32
        || dataType == FieldDataType::Int64;
}

bool parseDoubleText(const QString& text, double& out)
{
    bool ok = false;
    out = text.toDouble(&ok); // C locale: '.' decimal separator
    return ok;
}

bool isPrintableAscii(const QString& text)
{
    for (int i = 0; i < text.size(); ++i)
    {
        const ushort u = text.at(i).unicode();
        if (u < 0x20 || u > 0x7E)
            return false;
    }
    return true;
}

// Apply the field's wire byte order to already-encoded big-endian bytes. The
// numeric/float encoders below always build big-endian first (the parser
// contract); a Little field simply reverses those bytes. String bytes are never
// reversed and never reach here; a single byte reverses to itself.
void applyFieldByteOrder(QByteArray& bytes, const FieldDefinition& field)
{
    if (field.endianness == FieldEndianness::Little && bytes.size() > 1)
        std::reverse(bytes.begin(), bytes.end());
}
} // namespace

bool PayloadBuilder::rawFromTypedValue(const FieldDefinition& field,
                                       const QString& valueText,
                                       quint64& rawOut,
                                       QString& reason,
                                       QString& solution)
{
    rawOut = 0;
    const QString text = valueText.trimmed();
    const QString typeName = typeDisplayName(field.dataType);

    if (text.isEmpty())
    {
        reason = "no value has been entered.";
        solution = "Type the value to transmit (use 0 to send empty/zero bytes).";
        return false;
    }

    if (field.length < 1 || field.length > 8)
    {
        reason = QString("Length %1 is not valid for numeric encoding.").arg(field.length);
        solution = "Set Length between 1 and 8 bytes for numeric types (String fields may be longer).";
        return false;
    }

    if (field.dataType == FieldDataType::Bool)
    {
        const QString lower = text.toLower();
        if (lower == "1" || lower == "true")
        {
            rawOut = 1;
            return true;
        }
        if (lower == "0" || lower == "false")
        {
            rawOut = 0;
            return true;
        }
        reason = QString("'%1' is not a valid bool value.").arg(text);
        solution = "Use 1 / 0 or true / false.";
        return false;
    }

    if (isUnsignedFamily(field.dataType))
    {
        if (text.startsWith('-'))
        {
            reason = QString("the value is negative but %1 is an unsigned type.").arg(typeName);
            solution = "Choose a signed type (char / short / int / long) or send a value >= 0.";
            return false;
        }

        const quint64 maxValue = maskForLength(field.length);

        if (!shouldApplyResolution(field.resolution))
        {
            bool ok = false;
            quint64 value = 0;
            if (text.startsWith("0x", Qt::CaseInsensitive))
                value = text.mid(2).toULongLong(&ok, 16);
            else
                value = text.toULongLong(&ok, 10);

            if (!ok)
            {
                reason = QString("'%1' is not a valid %2 number.").arg(text).arg(typeName);
                solution = "Type a whole number (decimal, or hex with a 0x prefix).";
                return false;
            }

            if (field.length < 8 && value > maxValue)
            {
                reason = QString("value %1 does not fit in %2 byte(s) as %3 (maximum %4).")
                             .arg(text).arg(field.length).arg(typeName)
                             .arg(QString::number(static_cast<qulonglong>(maxValue)));
                solution = "Increase the field Length, pick a wider type, or send a smaller value.";
                return false;
            }

            rawOut = value;
            return true;
        }

        double value = 0.0;
        if (!parseDoubleText(text, value))
        {
            reason = QString("'%1' is not a valid number.").arg(text);
            solution = "Type a plain number using . as the decimal separator (e.g. 12.5).";
            return false;
        }

        const double scaled = value / field.resolution;
        if (scaled < -0.5)
        {
            reason = QString("the value scales to a negative raw value (%1 / %2) but %3 is unsigned.")
                         .arg(text).arg(field.resolution).arg(typeName);
            solution = "Choose a signed type or send a value >= 0.";
            return false;
        }
        if (scaled > 9.0e18 || (field.length < 8 && scaled > static_cast<double>(maxValue) + 0.5))
        {
            reason = QString("the value scales to %1 / resolution %2 which does not fit in %3 byte(s).")
                         .arg(text).arg(field.resolution).arg(field.length);
            solution = "Increase the field Length, raise the resolution, or send a smaller value.";
            return false;
        }

        rawOut = static_cast<quint64>(qRound64(scaled)) & maskForLength(field.length);
        return true;
    }

    if (isSignedFamily(field.dataType))
    {
        const int bits = field.length * 8;
        const qint64 minValue = (field.length >= 8)
            ? Q_INT64_C(-9223372036854775807) - 1
            : -(Q_INT64_C(1) << (bits - 1));
        const qint64 maxValue = (field.length >= 8)
            ? Q_INT64_C(9223372036854775807)
            : (Q_INT64_C(1) << (bits - 1)) - 1;

        qint64 signedValue = 0;

        if (!shouldApplyResolution(field.resolution))
        {
            bool ok = false;
            signedValue = text.toLongLong(&ok, 10);
            if (!ok)
            {
                reason = QString("'%1' is not a valid %2 number.").arg(text).arg(typeName);
                solution = "Type a whole decimal number (negative values are allowed).";
                return false;
            }
        }
        else
        {
            double value = 0.0;
            if (!parseDoubleText(text, value))
            {
                reason = QString("'%1' is not a valid number.").arg(text);
                solution = "Type a plain number using . as the decimal separator (e.g. -12.5).";
                return false;
            }
            const double scaled = value / field.resolution;
            if (scaled > 9.0e18 || scaled < -9.0e18)
            {
                reason = QString("the value scales to %1 / resolution %2 which is out of range.")
                             .arg(text).arg(field.resolution);
                solution = "Raise the resolution or send a smaller value.";
                return false;
            }
            signedValue = qRound64(scaled);
        }

        if (signedValue < minValue || signedValue > maxValue)
        {
            reason = QString("value %1 does not fit in %2 byte(s) as %3 (range %4 to %5).")
                         .arg(text).arg(field.length).arg(typeName)
                         .arg(QString::number(static_cast<qlonglong>(minValue)))
                         .arg(QString::number(static_cast<qlonglong>(maxValue)));
            solution = "Increase the field Length, pick a wider type, or send a smaller value.";
            return false;
        }

        rawOut = static_cast<quint64>(signedValue) & maskForLength(field.length);
        return true;
    }

    reason = QString("%1 fields cannot be entered as a raw integer.").arg(typeName);
    solution = "float / double / string fields are encoded from their typed value directly.";
    return false;
}

QString PayloadBuilder::typedValueFromRaw(const FieldDefinition& field, quint64 rawValue)
{
    rawValue &= maskForLength(field.length);

    if (field.dataType == FieldDataType::Bool)
        return rawValue == 0 ? "false" : "true";

    if (isUnsignedFamily(field.dataType))
    {
        if (shouldApplyResolution(field.resolution))
            return formatCalculatedValue(static_cast<double>(rawValue) * field.resolution);
        return QString::number(static_cast<qulonglong>(rawValue));
    }

    if (isSignedFamily(field.dataType))
    {
        const qint64 signedValue = signExtendRawValue(rawValue, field.length * 8);
        if (shouldApplyResolution(field.resolution))
            return formatCalculatedValue(static_cast<double>(signedValue) * field.resolution);
        return QString::number(static_cast<qlonglong>(signedValue));
    }

    if (field.dataType == FieldDataType::Float32 && field.length == 4)
    {
        const quint32 raw32 = static_cast<quint32>(rawValue);
        float value = 0.0f;
        std::memcpy(&value, &raw32, sizeof(value));
        double calculated = static_cast<double>(value);
        if (shouldApplyResolution(field.resolution))
            calculated *= field.resolution;
        return formatCalculatedValue(calculated);
    }

    if (field.dataType == FieldDataType::Float64 && field.length == 8)
    {
        double value = 0.0;
        std::memcpy(&value, &rawValue, sizeof(value));
        if (shouldApplyResolution(field.resolution))
            value *= field.resolution;
        return formatCalculatedValue(value);
    }

    return QString();
}

bool PayloadBuilder::encodeFieldValue(const FieldDefinition& field,
                                      const QString& valueText,
                                      QByteArray& bytesOut,
                                      QString& reason,
                                      QString& solution)
{
    bytesOut.clear();

    if (field.dataType == FieldDataType::String)
    {
        if (field.length <= 0)
        {
            reason = "the field Length must be at least 1 byte.";
            solution = "Set Length to the number of bytes reserved for the text.";
            return false;
        }
        const QByteArray utf8 = valueText.toUtf8();
        if (utf8.size() > field.length)
        {
            reason = QString("the text is %1 byte(s) in UTF-8 but the field Length is only %2.")
                         .arg(utf8.size()).arg(field.length);
            solution = QString("Shorten the text or increase the field Length to at least %1 bytes.")
                           .arg(utf8.size());
            return false;
        }
        bytesOut = utf8;
        bytesOut.append(QByteArray(field.length - utf8.size(), char(0)));
        return true;
    }

    if (field.dataType == FieldDataType::Float32 || field.dataType == FieldDataType::Float64)
    {
        const bool isFloat32 = (field.dataType == FieldDataType::Float32);
        const int requiredLength = isFloat32 ? 4 : 8;
        if (field.length != requiredLength)
        {
            reason = QString("%1 fields must be exactly %2 bytes long, but Length is %3.")
                         .arg(typeDisplayName(field.dataType)).arg(requiredLength).arg(field.length);
            solution = QString("Set Length to %1, or use a scaled integer type for other sizes.")
                           .arg(requiredLength);
            return false;
        }

        const QString text = valueText.trimmed();
        if (text.isEmpty())
        {
            reason = "no value has been entered.";
            solution = "Type the value to transmit (use 0 to send zero).";
            return false;
        }

        double value = 0.0;
        if (!parseDoubleText(text, value))
        {
            reason = QString("'%1' is not a valid number.").arg(text);
            solution = "Type a plain number using . as the decimal separator (e.g. 12.5).";
            return false;
        }

        const double rawDouble = value / field.resolution;
        quint64 rawBits = 0;
        if (isFloat32)
        {
            const float rawFloat = static_cast<float>(rawDouble);
            quint32 bits32 = 0;
            std::memcpy(&bits32, &rawFloat, sizeof(bits32));
            rawBits = bits32;
        }
        else
        {
            std::memcpy(&rawBits, &rawDouble, sizeof(rawBits));
        }

        bytesOut.resize(field.length);
        for (int i = 0; i < field.length; ++i)
            bytesOut[i] = static_cast<char>((rawBits >> (8 * (field.length - 1 - i))) & 0xFF);
        applyFieldByteOrder(bytesOut, field);
        return true;
    }

    quint64 rawValue = 0;
    if (!rawFromTypedValue(field, valueText, rawValue, reason, solution))
        return false;

    bytesOut.resize(field.length);
    for (int i = 0; i < field.length; ++i)
        bytesOut[i] = static_cast<char>((rawValue >> (8 * (field.length - 1 - i))) & 0xFF);
    applyFieldByteOrder(bytesOut, field);
    return true;
}

QString PayloadBuilder::fieldHexPreview(const FieldDefinition& field,
                                        const QString& valueText,
                                        QString& shortError)
{
    shortError.clear();

    QByteArray bytes;
    QString reason;
    QString solution;
    if (!encodeFieldValue(field, valueText, bytes, reason, solution))
    {
        shortError = reason;
        return QString();
    }

    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

bool PayloadBuilder::buildHexPayload(const MessageDefinition& message,
                                     QByteArray& payloadOut,
                                     QStringList& problems)
{
    const int initialProblemCount = problems.size();

    if (message.payloadLengthBytes <= 0)
    {
        problems.append(QString("Message '%1': the payload length is %2 bytes. Solution: set Payload Length to at least 1 byte.")
                            .arg(message.messageName).arg(message.payloadLengthBytes));
        payloadOut.clear();
        return false;
    }

    payloadOut = QByteArray(message.payloadLengthBytes, char(0));

    for (int i = 0; i < message.fields.size(); ++i)
    {
        const FieldDefinition& field = message.fields.at(i);
        const QString prefix = QString("Message '%1', field '%2': ").arg(message.messageName).arg(field.name);

        if (field.byteOffsetcorrect < 0 || field.length <= 0)
        {
            problems.append(prefix + QString("byte offset %1 / length %2 is invalid. Solution: Byte Offset starts at 1 and Length must be at least 1.")
                                         .arg(field.byteOffset).arg(field.length));
            continue;
        }

        if (field.byteOffsetcorrect + field.length > message.payloadLengthBytes)
        {
            problems.append(prefix + QString("the field ends at byte %1 but the message payload is only %2 byte(s). Solution: increase the message's Payload Length to at least %1, or reduce the field's Byte Offset / Length.")
                                         .arg(field.byteOffsetcorrect + field.length)
                                         .arg(message.payloadLengthBytes));
            continue;
        }

        QByteArray fieldBytes;
        QString reason;
        QString solution;
        if (!encodeFieldValue(field, field.sendValueText, fieldBytes, reason, solution))
        {
            problems.append(prefix + reason + " Solution: " + solution);
            continue;
        }

        std::memcpy(payloadOut.data() + field.byteOffsetcorrect, fieldBytes.constData(), fieldBytes.size());
    }

    return problems.size() == initialProblemCount;
}

bool PayloadBuilder::buildNmeaSentence(const MessageDefinition& message,
                                       QByteArray& sentenceOut,
                                       QStringList& problems)
{
    const int initialProblemCount = problems.size();
    sentenceOut.clear();

    const QString prefix = QString("Message '%1': ").arg(message.messageName);

    QString formatter = message.nmeaSentenceType.trimmed().toUpper();
    bool formatterOk = (formatter.size() == 3);
    for (int i = 0; formatterOk && i < formatter.size(); ++i)
        formatterOk = formatter.at(i).isLetterOrNumber();
    if (!formatterOk)
    {
        problems.append(prefix + QString("'%1' is not a valid 3-character NMEA sentence formatter. Solution: pick a sentence (e.g. GGA) in the message definition.")
                                     .arg(message.nmeaSentenceType));
    }

    QString talker = message.nmeaTalker.trimmed().toUpper();
    if (talker.isEmpty())
        talker = "GP";
    bool talkerOk = (talker.size() == 2);
    for (int i = 0; talkerOk && i < talker.size(); ++i)
        talkerOk = talker.at(i).isLetterOrNumber();
    if (!talkerOk)
    {
        problems.append(prefix + QString("'%1' is not a valid 2-character NMEA talker id. Solution: use a 2-letter talker such as GP, GN, HE or II.")
                                     .arg(message.nmeaTalker));
    }

    int slotCount = 0;
    const NmeaSentenceDef* registryDef = NmeaSentenceRegistry::lookup(formatter);
    if (registryDef)
    {
        for (int i = 0; i < registryDef->fields.size(); ++i)
            slotCount = qMax(slotCount, registryDef->fields.at(i).index);
    }

    for (int i = 0; i < message.fields.size(); ++i)
    {
        const FieldDefinition& field = message.fields.at(i);
        if (field.nmeaFieldIndex < 1)
        {
            problems.append(QString("Message '%1', field '%2': the field has no Field # (1-based comma position). Solution: set the comma position in Configure Fields.")
                                .arg(message.messageName).arg(field.name));
            continue;
        }
        slotCount = qMax(slotCount, field.nmeaFieldIndex);
    }

    QStringList tokens;
    for (int i = 0; i < slotCount; ++i)
        tokens.append(QString());

    for (int i = 0; i < message.fields.size(); ++i)
    {
        const FieldDefinition& field = message.fields.at(i);
        if (field.nmeaFieldIndex < 1)
            continue;

        const QString token = field.sendValueText.trimmed();
        if (token.contains(',') || token.contains('*') || token.contains('$') || token.contains('!'))
        {
            problems.append(QString("Message '%1', field '%2': the value '%3' contains characters that delimit NMEA sentences (, * $ !). Solution: remove those characters from the value.")
                                .arg(message.messageName).arg(field.name).arg(token));
            continue;
        }
        if (!isPrintableAscii(token))
        {
            problems.append(QString("Message '%1', field '%2': the value '%3' contains non-ASCII characters. Solution: NMEA sentences are plain ASCII — use only printable ASCII characters.")
                                .arg(message.messageName).arg(field.name).arg(token));
            continue;
        }

        tokens[field.nmeaFieldIndex - 1] = token;
    }

    if (problems.size() != initialProblemCount)
        return false;

    QString body = talker + formatter;
    if (slotCount > 0)
        body += "," + tokens.join(",");

    const QByteArray bodyBytes = body.toLatin1();
    const quint8 checksum = xorChecksum(bodyBytes);

    sentenceOut.append('$');
    sentenceOut.append(bodyBytes);
    sentenceOut.append('*');
    sentenceOut.append(QString("%1").arg(checksum, 2, 16, QChar('0')).toUpper().toLatin1());
    sentenceOut.append("\r\n");
    return true;
}

quint8 PayloadBuilder::xorChecksum(const QByteArray& body)
{
    quint8 checksum = 0;
    for (int i = 0; i < body.size(); ++i)
        checksum ^= static_cast<quint8>(body.at(i));
    return checksum;
}

bool PayloadBuilder::fieldSupportsBitEditing(const FieldDefinition& field)
{
    if (field.length < 1 || field.length > 8)
        return false;

    switch (field.dataType)
    {
    case FieldDataType::RawUnsignedBE:
    case FieldDataType::Uint8:
    case FieldDataType::Int8:
    case FieldDataType::Uint16:
    case FieldDataType::Int16:
    case FieldDataType::Uint32:
    case FieldDataType::Int32:
    case FieldDataType::Uint64:
    case FieldDataType::Int64:
    case FieldDataType::Bool:
        return true;
    case FieldDataType::Float32:
    case FieldDataType::Float64:
    case FieldDataType::String:
    default:
        return false;
    }
}
