#include "BitRuleCsvCodec.h"

#include "BitfieldDecoder.h"

#include <QFile>
#include <QIODevice>
#include <QMap>
#include <QSet>
#include <QTextStream>

namespace
{
QStringList parseCsvLine(const QString& line)
{
    QStringList out;
    QString cell;
    bool inQuotes = false;
    int i = 0;
    while (i < line.size())
    {
        const QChar ch = line.at(i);
        if (inQuotes)
        {
            if (ch == QChar('"'))
            {
                if (i + 1 < line.size() && line.at(i + 1) == QChar('"'))
                {
                    cell.append(QChar('"'));
                    i += 2;
                    continue;
                }
                inQuotes = false;
                ++i;
                continue;
            }
            cell.append(ch);
            ++i;
            continue;
        }
        if (ch == QChar(','))
        {
            out.append(cell);
            cell.clear();
            ++i;
            continue;
        }
        if (ch == QChar('"') && cell.isEmpty())
        {
            inQuotes = true;
            ++i;
            continue;
        }
        cell.append(ch);
        ++i;
    }
    out.append(cell);
    return out;
}

QString escapeCsvCell(QString cell)
{
    const bool needsQuote = cell.contains(',') || cell.contains('"')
                            || cell.contains('\n') || cell.contains('\r');
    cell.replace(QChar('"'), QString("\"\""));
    if (needsQuote)
    {
        cell.prepend(QChar('"'));
        cell.append(QChar('"'));
    }
    return cell;
}

void readAllLogicalLines(QFile& file, QStringList& lines)
{
    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#endif
    QString buffer;
    bool inQuotes = false;
    while (!in.atEnd())
    {
        const QString line = in.readLine();
        for (int k = 0; k < line.size(); ++k)
            if (line.at(k) == QChar('"'))
                inQuotes = !inQuotes;
        if (!buffer.isEmpty())
            buffer.append('\n');
        buffer.append(line);
        if (!inQuotes)
        {
            lines.append(buffer);
            buffer.clear();
        }
    }
    if (!buffer.isEmpty())
        lines.append(buffer);
}

QString normalizeUnknownBehavior(QString s)
{
    s = s.trimmed().toUpper();
    if (s.isEmpty()) return QString("UNKNOWN");
    return s;
}

bool isUnknownBehaviorValid(const QString& s)
{
    return s == "UNKNOWN" || s == "BLANK" || s == "RAW_BINARY";
}

bool parseBoolCell(const QString& cell, bool defaultValue, bool& ok)
{
    const QString s = cell.trimmed().toLower();
    if (s.isEmpty()) { ok = true; return defaultValue; }
    if (s == "true" || s == "1" || s == "yes" || s == "y") { ok = true; return true; }
    if (s == "false" || s == "0" || s == "no" || s == "n") { ok = true; return false; }
    ok = false;
    return defaultValue;
}

QString normalizeBitsCell(const QString& cell)
{
    QString s = cell.trimmed();
    s.replace(QChar(';'), QChar(','));
    return s;
}

QString bitsToCsvCell(const QList<int>& bits)
{
    QStringList parts;
    for (int i = 0; i < bits.size(); ++i)
        parts << QString::number(bits.at(i));
    return parts.join(";");
}

QString cellAt(const QStringList& cells, int col)
{
    if (col < 0 || col >= cells.size()) return QString();
    return cells.at(col).trimmed();
}
}

bool BitRuleCsvCodec::importFromCsv(const QString& path,
                                    int fieldLengthBytes,
                                    QList<BitDecodeRule>& out,
                                    QStringList& warnings,
                                    QString& errorMessage)
{
    out.clear();
    warnings.clear();
    errorMessage.clear();

    if (fieldLengthBytes <= 0)
    {
        errorMessage = "Field length must be greater than zero.";
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorMessage = QString("Cannot open CSV file: %1").arg(file.errorString());
        return false;
    }

    QStringList lines;
    readAllLogicalLines(file, lines);
    file.close();

    int headerLineIndex = -1;
    for (int i = 0; i < lines.size(); ++i)
    {
        const QString t = lines.at(i).trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith('#')) continue;
        headerLineIndex = i;
        break;
    }
    if (headerLineIndex < 0)
    {
        errorMessage = "CSV file has no header row.";
        return false;
    }

    const QStringList headers = parseCsvLine(lines.at(headerLineIndex));
    int colLabel = -1, colBits = -1, colReserved = -1, colUnknown = -1, colEnabled = -1;
    int colValue = -1, colBinary = -1, colMeaning = -1;
    for (int i = 0; i < headers.size(); ++i)
    {
        const QString h = headers.at(i).trimmed().toLower();
        if      (h == "label" || h == "name" || h == "rule" || h == "rulename")            colLabel = i;
        else if (h == "bits" || h == "bitpositions" || h == "bit positions")               colBits = i;
        else if (h == "reserved")                                                          colReserved = i;
        else if (h == "unknownbehavior" || h == "unknown behavior" || h == "unknown")      colUnknown = i;
        else if (h == "enabled")                                                           colEnabled = i;
        else if (h == "value" || h == "decimal" || h == "raw")                             colValue = i;
        else if (h == "binary" || h == "binarystring" || h == "binary string")             colBinary = i;
        else if (h == "meaning" || h == "description")                                     colMeaning = i;
    }
    if (colLabel < 0 || colBits < 0)
    {
        errorMessage = "CSV header must include Label and Bits columns.";
        return false;
    }
    if (colValue < 0 && colBinary < 0)
    {
        errorMessage = "CSV header must include Value or Binary column.";
        return false;
    }
    if (colMeaning < 0)
    {
        errorMessage = "CSV header must include Meaning column.";
        return false;
    }

    QStringList errorList;
    QStringList labelOrder;
    QMap<QString, BitDecodeRule> rulesByLabel;
    const int maxBitCount = fieldLengthBytes * 8;

    for (int lineIdx = headerLineIndex + 1; lineIdx < lines.size(); ++lineIdx)
    {
        const QString raw = lines.at(lineIdx);
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith('#')) continue;

        const QStringList cells = parseCsvLine(raw);
        const int rowNumber = lineIdx + 1;

        const QString label = cellAt(cells, colLabel);
        if (label.isEmpty()) continue;

        const QString bitsCell = cellAt(cells, colBits);
        if (bitsCell.isEmpty())
        {
            errorList << QString("Line %1: Bits cell is empty for Label '%2'.").arg(rowNumber).arg(label);
            continue;
        }

        const QString reservedCell = cellAt(cells, colReserved);
        bool reservedOk = false;
        const bool reserved = parseBoolCell(reservedCell, false, reservedOk);
        if (!reservedOk)
        {
            errorList << QString("Line %1: Reserved must be true/false (got '%2').").arg(rowNumber).arg(reservedCell);
            continue;
        }

        const QString unknownRaw = cellAt(cells, colUnknown);
        const QString unknownBehavior = normalizeUnknownBehavior(unknownRaw);
        if (!unknownRaw.trimmed().isEmpty() && !isUnknownBehaviorValid(unknownBehavior))
        {
            errorList << QString("Line %1: Unknown UnknownBehavior '%2'. Accepted: UNKNOWN, BLANK, RAW_BINARY.")
                           .arg(rowNumber).arg(unknownRaw);
            continue;
        }

        const QString enabledCell = cellAt(cells, colEnabled);
        bool enabledOk = false;
        const bool enabled = parseBoolCell(enabledCell, true, enabledOk);
        if (!enabledOk)
        {
            errorList << QString("Line %1: Enabled must be true/false (got '%2').").arg(rowNumber).arg(enabledCell);
            continue;
        }

        QList<int> bitPositions;
        QString bitsErr;
        if (!BitfieldDecoder::parseBitPositions(normalizeBitsCell(bitsCell), maxBitCount, bitPositions, bitsErr))
        {
            errorList << QString("Line %1: Invalid Bits '%2': %3").arg(rowNumber).arg(bitsCell).arg(bitsErr);
            continue;
        }

        const QString binaryCell = cellAt(cells, colBinary);
        const QString valueCell  = cellAt(cells, colValue);
        const QString meaningCell = cellAt(cells, colMeaning);
        quint64 mappingValue = 0;
        bool haveMapping = false;

        if (!binaryCell.isEmpty())
        {
            const int expectedWidth = bitPositions.size();
            if (binaryCell.length() != expectedWidth)
            {
                errorList << QString("Line %1: Binary '%2' length %3 does not match bit count %4.")
                               .arg(rowNumber).arg(binaryCell).arg(binaryCell.length()).arg(expectedWidth);
                continue;
            }
            quint64 v = 0;
            if (!BitfieldDecoder::binaryToValue(binaryCell, v))
            {
                errorList << QString("Line %1: Binary '%2' is not a valid bit string.").arg(rowNumber).arg(binaryCell);
                continue;
            }
            mappingValue = v;
            haveMapping = true;
        }
        else if (!valueCell.isEmpty())
        {
            bool ok = false;
            mappingValue = valueCell.toULongLong(&ok, 10);
            if (!ok)
            {
                errorList << QString("Line %1: Value '%2' is not a valid decimal integer.").arg(rowNumber).arg(valueCell);
                continue;
            }
            const int width = bitPositions.size();
            if (width < 64)
            {
                const quint64 maxValue = (static_cast<quint64>(1) << width) - 1;
                if (mappingValue > maxValue)
                {
                    errorList << QString("Line %1: Value %2 exceeds %3-bit range (max %4).")
                                   .arg(rowNumber).arg(mappingValue).arg(width).arg(maxValue);
                    continue;
                }
            }
            haveMapping = true;
        }

        if (!reserved && !haveMapping)
        {
            errorList << QString("Line %1: Value or Binary is required for non-reserved rule '%2'.")
                           .arg(rowNumber).arg(label);
            continue;
        }
        if (!reserved && meaningCell.isEmpty())
        {
            errorList << QString("Line %1: Meaning is required for non-reserved rule '%2'.")
                           .arg(rowNumber).arg(label);
            continue;
        }

        if (!rulesByLabel.contains(label))
        {
            BitDecodeRule rule;
            rule.label = label;
            rule.bitPositions = bitPositions;
            rule.reserved = reserved;
            rule.unknownBehavior = unknownBehavior;
            rule.enabled = enabled;
            rulesByLabel.insert(label, rule);
            labelOrder.append(label);
        }
        else
        {
            const BitDecodeRule& existing = rulesByLabel.value(label);
            if (existing.bitPositions != bitPositions)
            {
                errorList << QString("Line %1: Bits '%2' for Label '%3' differs from earlier definition.")
                               .arg(rowNumber).arg(bitsCell).arg(label);
                continue;
            }
            if (existing.reserved != reserved)
            {
                errorList << QString("Line %1: Reserved flag for Label '%2' is inconsistent across rows.")
                               .arg(rowNumber).arg(label);
                continue;
            }
            if (existing.unknownBehavior != unknownBehavior)
            {
                errorList << QString("Line %1: UnknownBehavior for Label '%2' is inconsistent across rows.")
                               .arg(rowNumber).arg(label);
                continue;
            }
            if (existing.enabled != enabled)
            {
                errorList << QString("Line %1: Enabled for Label '%2' is inconsistent across rows.")
                               .arg(rowNumber).arg(label);
                continue;
            }
        }

        if (haveMapping)
        {
            BitDecodeRule& rule = rulesByLabel[label];
            if (rule.valueMeanings.contains(mappingValue))
            {
                errorList << QString("Line %1: Duplicate Value %2 for Label '%3'.")
                               .arg(rowNumber).arg(mappingValue).arg(label);
                continue;
            }
            rule.valueMeanings.insert(mappingValue, meaningCell);
        }
    }

    if (!errorList.isEmpty())
    {
        errorMessage = errorList.join("\n");
        return false;
    }

    QList<BitDecodeRule> rules;
    for (int i = 0; i < labelOrder.size(); ++i)
        rules.append(rulesByLabel.value(labelOrder.at(i)));

    QString validateErr;
    if (!BitfieldDecoder::validateRules(rules, fieldLengthBytes, validateErr))
    {
        errorMessage = validateErr;
        return false;
    }

    out = rules;
    return true;
}

bool BitRuleCsvCodec::exportToCsv(const QString& path,
                                  const QList<BitDecodeRule>& rules,
                                  QString& errorMessage)
{
    errorMessage.clear();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        errorMessage = QString("Cannot open CSV file for writing: %1").arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "Label,Bits,Reserved,UnknownBehavior,Enabled,Value,Binary,Meaning\n";

    for (int r = 0; r < rules.size(); ++r)
    {
        const BitDecodeRule& rule = rules.at(r);
        const QString bitsCell = bitsToCsvCell(rule.bitPositions);
        const QString reservedCell = rule.reserved ? "true" : "false";
        const QString unknownCell = rule.unknownBehavior.isEmpty() ? QString("UNKNOWN") : rule.unknownBehavior;
        const QString enabledCell = rule.enabled ? "true" : "false";

        if (rule.valueMeanings.isEmpty())
        {
            out << escapeCsvCell(rule.label) << ','
                << escapeCsvCell(bitsCell) << ','
                << reservedCell << ','
                << escapeCsvCell(unknownCell) << ','
                << enabledCell << ",,,\n";
        }
        else
        {
            QMap<quint64, QString>::const_iterator it;
            for (it = rule.valueMeanings.constBegin(); it != rule.valueMeanings.constEnd(); ++it)
            {
                const QString binary = BitfieldDecoder::binaryString(it.key(), rule.bitPositions.size());
                out << escapeCsvCell(rule.label) << ','
                    << escapeCsvCell(bitsCell) << ','
                    << reservedCell << ','
                    << escapeCsvCell(unknownCell) << ','
                    << enabledCell << ','
                    << it.key() << ','
                    << binary << ','
                    << escapeCsvCell(it.value())
                    << '\n';
            }
        }
    }

    out.flush();
    file.close();
    return true;
}

bool BitRuleCsvCodec::writeTemplate(const QString& path, QString& errorMessage)
{
    errorMessage.clear();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        errorMessage = QString("Cannot open CSV file for writing: %1").arg(file.errorString());
        return false;
    }
    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "# Bitfield Decoder CSV template - PcapUdpExtractor\n";
    out << "# Lines starting with '#' are ignored.\n";
    out << "# Bits column accepts: single bit ('3'), range ('0-2'), or list using ';' ('0;1;2').\n";
    out << "# Provide Value (decimal) OR Binary (bit string of length = bit count). If both, Binary wins.\n";
    out << "# Rows with the same Label are merged into one rule with multiple value mappings.\n";
    out << "# Reserved=true rows can have empty Value / Binary / Meaning.\n";
    out << "Label,Bits,Reserved,UnknownBehavior,Enabled,Value,Binary,Meaning\n";
    out << "# Status,0-2,false,UNKNOWN,true,0,,Idle\n";
    out << "# Status,0-2,false,UNKNOWN,true,1,,Active\n";
    out << "# Status,0-2,false,UNKNOWN,true,2,,Error\n";
    out << "# Flag,3,false,UNKNOWN,true,0,,Off\n";
    out << "# Flag,3,false,UNKNOWN,true,1,,On\n";
    out << "# ReservedBlock,4-5,true,UNKNOWN,false,,,\n";
    out.flush();
    file.close();
    return true;
}
