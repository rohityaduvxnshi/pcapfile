#include "BitfieldDecoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

namespace
{
QString normalizedUnknownBehavior(QString behavior)
{
    behavior = behavior.trimmed().toUpper();
    if (behavior == "BLANK" || behavior == "RAW_BINARY") return behavior;
    return "UNKNOWN";
}

QString normalizedRuleType(QString type)
{
    type = type.trimmed().toUpper();
    if (type == "RESERVED") return "RESERVED";
    if (type == "SINGLE_BIT") return "SINGLE_BIT";
    return "GROUPED_BITS";
}
}

QString BitfieldDecoder::rulesToJson(const QList<BitDecodeRule>& rules)
{
    QJsonArray ruleArray;

    for (int i = 0; i < rules.size(); ++i)
    {
        const BitDecodeRule& rule = rules.at(i);
        if (!rule.enabled) continue;

        QJsonObject ruleObject;
        ruleObject.insert("label", rule.label.trimmed());
        ruleObject.insert("type", ruleTypeText(rule));
        ruleObject.insert("reserved", rule.reserved);
        ruleObject.insert("unknownBehavior", normalizedUnknownBehavior(rule.unknownBehavior));
        ruleObject.insert("enabled", rule.enabled);

        QJsonArray bitsArray;
        for (int b = 0; b < rule.bitPositions.size(); ++b)
            bitsArray.append(rule.bitPositions.at(b));
        ruleObject.insert("bits", bitsArray);

        QJsonArray mappingsArray;
        QMap<quint64, QString>::const_iterator it;
        for (it = rule.valueMeanings.constBegin(); it != rule.valueMeanings.constEnd(); ++it)
        {
            QJsonObject mappingObject;
            mappingObject.insert("value", QString::number(it.key()));
            mappingObject.insert("binary", binaryString(it.key(), rule.bitPositions.size()));
            mappingObject.insert("meaning", it.value());
            mappingsArray.append(mappingObject);
        }
        ruleObject.insert("mappings", mappingsArray);

        ruleArray.append(ruleObject);
    }

    QJsonObject root;
    root.insert("rules", ruleArray);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool BitfieldDecoder::rulesFromJson(const QString& jsonText,
                                    int fieldLengthBytes,
                                    QList<BitDecodeRule>& rules,
                                    QString& errorMessage)
{
    rules.clear();
    errorMessage.clear();

    const QString trimmed = jsonText.trimmed();
    if (trimmed.isEmpty()) return true;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        errorMessage = "Bitfield decoder data is not valid JSON.";
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray ruleArray = root.value("rules").toArray();

    for (int i = 0; i < ruleArray.size(); ++i)
    {
        const QJsonObject ruleObject = ruleArray.at(i).toObject();
        BitDecodeRule rule;
        rule.label = ruleObject.value("label").toString().trimmed();
        rule.reserved = ruleObject.value("reserved").toBool(false);
        rule.unknownBehavior = normalizedUnknownBehavior(ruleObject.value("unknownBehavior").toString("UNKNOWN"));
        rule.enabled = ruleObject.value("enabled").toBool(true);

        const QString type = normalizedRuleType(ruleObject.value("type").toString(rule.reserved ? "RESERVED" : "GROUPED_BITS"));
        rule.reserved = (type == "RESERVED");

        const QJsonArray bitsArray = ruleObject.value("bits").toArray();
        for (int b = 0; b < bitsArray.size(); ++b)
            rule.bitPositions << bitsArray.at(b).toInt(-1);

        const QJsonArray mappingsArray = ruleObject.value("mappings").toArray();
        for (int m = 0; m < mappingsArray.size(); ++m)
        {
            const QJsonObject mappingObject = mappingsArray.at(m).toObject();
            quint64 value = 0;
            bool valueOk = false;

            const QString binary = mappingObject.value("binary").toString().trimmed();
            if (!binary.isEmpty())
            {
                valueOk = binaryToValue(binary, value);
            }
            else
            {
                value = mappingObject.value("value").toString().toULongLong(&valueOk, 10);
            }

            if (!valueOk)
            {
                errorMessage = QString("Rule %1 contains an invalid mapping value.").arg(i + 1);
                return false;
            }

            rule.valueMeanings.insert(value, mappingObject.value("meaning").toString());
        }

        rules << rule;
    }

    return validateRules(rules, fieldLengthBytes, errorMessage);
}

bool BitfieldDecoder::parseBitPositions(const QString& text,
                                        int maxBitCount,
                                        QList<int>& bitPositions,
                                        QString& errorMessage)
{
    bitPositions.clear();
    errorMessage.clear();

    QString cleaned = text.trimmed();
    cleaned.remove(' ');

    if (cleaned.isEmpty())
    {
        errorMessage = "Bit positions cannot be empty.";
        return false;
    }

    QSet<int> seen;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QStringList parts = cleaned.split(',', Qt::SkipEmptyParts);
#else
    const QStringList parts = cleaned.split(',', QString::SkipEmptyParts);
#endif
    for (int i = 0; i < parts.size(); ++i)
    {
        const QString part = parts.at(i).trimmed();
        if (part.isEmpty()) continue;

        if (part.contains('-'))
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const QStringList rangeParts = part.split('-', Qt::KeepEmptyParts);
#else
            const QStringList rangeParts = part.split('-', QString::KeepEmptyParts);
#endif
            if (rangeParts.size() != 2)
            {
                errorMessage = "Invalid bit range: " + part;
                return false;
            }

            bool startOk = false;
            bool endOk = false;
            const int start = rangeParts.at(0).toInt(&startOk, 10);
            const int end = rangeParts.at(1).toInt(&endOk, 10);
            if (!startOk || !endOk || start > end)
            {
                errorMessage = "Invalid bit range: " + part;
                return false;
            }

            for (int b = start; b <= end; ++b)
            {
                if (b < 0 || b >= maxBitCount)
                {
                    errorMessage = QString("Bit position %1 is outside valid range 0-%2.").arg(b).arg(maxBitCount - 1);
                    return false;
                }
                if (seen.contains(b))
                {
                    errorMessage = QString("Duplicate bit position %1.").arg(b);
                    return false;
                }
                seen.insert(b);
                bitPositions << b;
            }
        }
        else
        {
            bool ok = false;
            const int b = part.toInt(&ok, 10);
            if (!ok)
            {
                errorMessage = "Invalid bit position: " + part;
                return false;
            }
            if (b < 0 || b >= maxBitCount)
            {
                errorMessage = QString("Bit position %1 is outside valid range 0-%2.").arg(b).arg(maxBitCount - 1);
                return false;
            }
            if (seen.contains(b))
            {
                errorMessage = QString("Duplicate bit position %1.").arg(b);
                return false;
            }
            seen.insert(b);
            bitPositions << b;
        }
    }

    if (bitPositions.isEmpty())
    {
        errorMessage = "Bit positions cannot be empty.";
        return false;
    }

    if (bitPositions.size() > 64)
    {
        errorMessage = "A single decode rule cannot contain more than 64 bits.";
        return false;
    }

    return true;
}

bool BitfieldDecoder::validateRules(const QList<BitDecodeRule>& rules,
                                    int fieldLengthBytes,
                                    QString& errorMessage)
{
    errorMessage.clear();

    if (fieldLengthBytes <= 0)
    {
        errorMessage = "Field length must be greater than zero.";
        return false;
    }

    const int maxBitCount = fieldLengthBytes * 8;
    QSet<int> usedBits;
    QSet<QString> labels;

    for (int i = 0; i < rules.size(); ++i)
    {
        const BitDecodeRule& rule = rules.at(i);
        if (!rule.enabled) continue;

        if (rule.label.trimmed().isEmpty())
        {
            errorMessage = QString("Decode rule %1 has an empty label.").arg(i + 1);
            return false;
        }

        const QString normalizedLabel = sanitizeColumnLabel(rule.label).toLower();
        if (labels.contains(normalizedLabel))
        {
            errorMessage = QString("Duplicate decode rule label: %1").arg(rule.label);
            return false;
        }
        labels.insert(normalizedLabel);

        if (rule.bitPositions.isEmpty())
        {
            errorMessage = QString("Decode rule %1 has no bit positions.").arg(rule.label);
            return false;
        }

        if (rule.bitPositions.size() > 64)
        {
            errorMessage = QString("Decode rule %1 uses more than 64 bits.").arg(rule.label);
            return false;
        }

        QSet<int> localBits;
        for (int b = 0; b < rule.bitPositions.size(); ++b)
        {
            const int bitPosition = rule.bitPositions.at(b);
            if (bitPosition < 0 || bitPosition >= maxBitCount)
            {
                errorMessage = QString("Decode rule %1 uses invalid bit position %2. Valid range is 0-%3.")
                                   .arg(rule.label)
                                   .arg(bitPosition)
                                   .arg(maxBitCount - 1);
                return false;
            }
            if (localBits.contains(bitPosition))
            {
                errorMessage = QString("Decode rule %1 has duplicate bit position %2.").arg(rule.label).arg(bitPosition);
                return false;
            }
            if (usedBits.contains(bitPosition))
            {
                errorMessage = QString("Bit position %1 is used by more than one decode rule.").arg(bitPosition);
                return false;
            }
            localBits.insert(bitPosition);
            usedBits.insert(bitPosition);
        }

        if (rule.valueMeanings.isEmpty())
        {
            errorMessage = QString("Decode rule %1 has no value mappings.").arg(rule.label);
            return false;
        }

        const int width = rule.bitPositions.size();
        if (width < 64)
        {
            const quint64 maxValue = (static_cast<quint64>(1) << width) - 1;
            QMap<quint64, QString>::const_iterator it;
            for (it = rule.valueMeanings.constBegin(); it != rule.valueMeanings.constEnd(); ++it)
            {
                if (it.key() > maxValue)
                {
                    errorMessage = QString("Decode rule %1 has mapping value %2 outside %3-bit range.")
                                       .arg(rule.label)
                                       .arg(QString::number(it.key()))
                                       .arg(width);
                    return false;
                }
            }
        }
    }

    return true;
}

QString BitfieldDecoder::decodeRule(const QByteArray& fieldBytes, const BitDecodeRule& rule)
{
    if (!rule.enabled || rule.bitPositions.isEmpty()) return QString();

    quint64 value = 0;
    for (int i = 0; i < rule.bitPositions.size(); ++i)
    {
        const int bitPosition = rule.bitPositions.at(i);
        const int byteIndex = bitPosition / 8;
        const int bitIndexInsideByte = bitPosition % 8;

        if (byteIndex < 0 || byteIndex >= fieldBytes.size()) return QString();

        const quint8 byteValue = static_cast<quint8>(fieldBytes.at(byteIndex));
        const bool bitSet = ((byteValue >> bitIndexInsideByte) & 0x01) != 0;
        if (bitSet) value |= (static_cast<quint64>(1) << i);
    }

    if (rule.valueMeanings.contains(value))
        return rule.valueMeanings.value(value);

    const QString binary = binaryString(value, rule.bitPositions.size());
    const QString behavior = normalizedUnknownBehavior(rule.unknownBehavior);

    if (behavior == "BLANK") return QString();
    if (behavior == "RAW_BINARY") return binary;

    return QString("UNKNOWN(%1)").arg(binary);
}

QString BitfieldDecoder::binaryString(quint64 value, int width)
{
    QString result;
    if (width <= 0) return result;

    for (int i = width - 1; i >= 0; --i)
        result += ((value >> i) & 1) ? QChar('1') : QChar('0');

    return result;
}

bool BitfieldDecoder::binaryToValue(const QString& binary, quint64& value)
{
    value = 0;
    const QString trimmed = binary.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 64) return false;

    for (int i = 0; i < trimmed.size(); ++i)
    {
        const QChar ch = trimmed.at(i);
        if (ch != QChar('0') && ch != QChar('1')) return false;
        value <<= 1;
        if (ch == QChar('1')) value |= 1;
    }

    return true;
}

QString BitfieldDecoder::sanitizeColumnLabel(QString label)
{
    label = label.trimmed();
    label.replace(QRegularExpression("\\s+"), "_");
    label.replace(QRegularExpression("[^A-Za-z0-9_]"), "_");
    label.replace(QRegularExpression("_+"), "_");
    if (label.startsWith('_')) label.remove(0, 1);
    if (label.endsWith('_')) label.chop(1);
    if (label.isEmpty()) label = "BITFIELD";
    return label;
}

QString BitfieldDecoder::bitsText(const QList<int>& bitPositions)
{
    QStringList parts;
    for (int i = 0; i < bitPositions.size(); ++i)
        parts << QString::number(bitPositions.at(i));
    return parts.join(",");
}

QString BitfieldDecoder::mappingSummary(const BitDecodeRule& rule, int maxCharacters)
{
    QStringList parts;
    QMap<quint64, QString>::const_iterator it;
    for (it = rule.valueMeanings.constBegin(); it != rule.valueMeanings.constEnd(); ++it)
    {
        parts << QString("%1=%2").arg(binaryString(it.key(), rule.bitPositions.size())).arg(it.value());
        if (parts.join("; ").size() > maxCharacters) break;
    }

    QString text = parts.join("; ");
    if (text.size() > maxCharacters)
        text = text.left(maxCharacters) + "...";
    return text;
}

QString BitfieldDecoder::ruleTypeText(const BitDecodeRule& rule)
{
    if (rule.reserved) return "RESERVED";
    if (rule.bitPositions.size() == 1) return "SINGLE_BIT";
    return "GROUPED_BITS";
}
