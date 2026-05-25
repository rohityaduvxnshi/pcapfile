#include "FieldCsvCodec.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSet>
#include <QTextStream>

namespace
{
struct TypeLabel
{
    const char* label;
    FieldDataType type;
};

const TypeLabel kTypeLabels[] = {
    { "Raw Unsigned BE", FieldDataType::RawUnsignedBE },
    { "RawUnsignedBE",   FieldDataType::RawUnsignedBE },
    { "raw",             FieldDataType::RawUnsignedBE },
    { "bool",            FieldDataType::Bool },
    { "Bool",            FieldDataType::Bool },
    { "uchar",           FieldDataType::Uint8 },
    { "Uint8",           FieldDataType::Uint8 },
    { "uint8",           FieldDataType::Uint8 },
    { "char",            FieldDataType::Int8 },
    { "Int8",            FieldDataType::Int8 },
    { "int8",            FieldDataType::Int8 },
    { "ushort",          FieldDataType::Uint16 },
    { "Uint16",          FieldDataType::Uint16 },
    { "uint16",          FieldDataType::Uint16 },
    { "short",           FieldDataType::Int16 },
    { "Int16",           FieldDataType::Int16 },
    { "int16",           FieldDataType::Int16 },
    { "uint",            FieldDataType::Uint32 },
    { "Uint32",          FieldDataType::Uint32 },
    { "uint32",          FieldDataType::Uint32 },
    { "int",             FieldDataType::Int32 },
    { "Int32",           FieldDataType::Int32 },
    { "int32",           FieldDataType::Int32 },
    { "ulong",           FieldDataType::Uint64 },
    { "Uint64",          FieldDataType::Uint64 },
    { "uint64",          FieldDataType::Uint64 },
    { "long",            FieldDataType::Int64 },
    { "Int64",           FieldDataType::Int64 },
    { "int64",           FieldDataType::Int64 },
    { "float",           FieldDataType::Float32 },
    { "Float32",         FieldDataType::Float32 },
    { "float32",         FieldDataType::Float32 },
    { "double",          FieldDataType::Float64 },
    { "Float64",         FieldDataType::Float64 },
    { "float64",         FieldDataType::Float64 }
};
const int kTypeLabelCount = sizeof(kTypeLabels) / sizeof(kTypeLabels[0]);

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
}

QStringList FieldCsvCodec::supportedDataTypeLabels()
{
    QStringList out;
    out << "Raw Unsigned BE" << "bool" << "uchar" << "char"
        << "ushort" << "short" << "uint" << "int"
        << "ulong" << "long" << "float" << "double";
    return out;
}

QString FieldCsvCodec::dataTypeToLabel(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::RawUnsignedBE: return QString("Raw Unsigned BE");
    case FieldDataType::Bool:          return QString("bool");
    case FieldDataType::Uint8:         return QString("uchar");
    case FieldDataType::Int8:          return QString("char");
    case FieldDataType::Uint16:        return QString("ushort");
    case FieldDataType::Int16:         return QString("short");
    case FieldDataType::Uint32:        return QString("uint");
    case FieldDataType::Int32:         return QString("int");
    case FieldDataType::Uint64:        return QString("ulong");
    case FieldDataType::Int64:         return QString("long");
    case FieldDataType::Float32:       return QString("float");
    case FieldDataType::Float64:       return QString("double");
    }
    return QString("Raw Unsigned BE");
}

bool FieldCsvCodec::dataTypeFromLabel(const QString& label, FieldDataType& dataType)
{
    const QString trimmed = label.trimmed();
    for (int i = 0; i < kTypeLabelCount; ++i)
    {
        if (trimmed.compare(QString::fromLatin1(kTypeLabels[i].label), Qt::CaseInsensitive) == 0)
        {
            dataType = kTypeLabels[i].type;
            return true;
        }
    }
    return false;
}

bool FieldCsvCodec::importFromCsv(const QString& path,
                                  int payloadLengthBytes,
                                  QList<FieldDefinition>& out,
                                  QStringList& warnings,
                                  QString& errorMessage)
{
    out.clear();
    warnings.clear();
    errorMessage.clear();

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
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith('#')) continue;
        headerLineIndex = i;
        break;
    }
    if (headerLineIndex < 0)
    {
        errorMessage = "CSV file has no header row.";
        return false;
    }

    const QStringList headers = parseCsvLine(lines.at(headerLineIndex));
    int colName = -1, colByte = -1, colType = -1, colLen = -1, colRes = -1, colExpr = -1;
    for (int i = 0; i < headers.size(); ++i)
    {
        const QString h = headers.at(i).trimmed().toLower();
        if      (h == "name" || h == "fieldname" || h == "field name")              colName = i;
        else if (h == "byteoffset" || h == "byte offset" || h == "offset")          colByte = i;
        else if (h == "datatype" || h == "data type" || h == "type")                colType = i;
        else if (h == "length" || h == "len" || h == "bytes")                       colLen  = i;
        else if (h == "resolution" || h == "scale")                                 colRes  = i;
        else if (h == "resolutionexpression" || h == "resolution expression"
                 || h == "expression")                                              colExpr = i;
    }
    if (colName < 0 || colByte < 0 || colType < 0)
    {
        errorMessage = "CSV header must include Name, ByteOffset, and DataType columns.";
        return false;
    }

    QStringList errorList;
    QSet<QString> seenNames;

    for (int lineIdx = headerLineIndex + 1; lineIdx < lines.size(); ++lineIdx)
    {
        const QString raw = lines.at(lineIdx);
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith('#')) continue;

        const QStringList cells = parseCsvLine(raw);
        const int rowNumber = lineIdx + 1;

        const QString cellName = (colName < cells.size()) ? cells.at(colName).trimmed() : QString();
        const QString cellByte = (colByte < cells.size()) ? cells.at(colByte).trimmed() : QString();
        const QString cellType = (colType < cells.size()) ? cells.at(colType).trimmed() : QString();
        const QString cellLen  = (colLen  >= 0 && colLen  < cells.size()) ? cells.at(colLen).trimmed()  : QString();
        const QString cellRes  = (colRes  >= 0 && colRes  < cells.size()) ? cells.at(colRes).trimmed()  : QString();
        const QString cellExpr = (colExpr >= 0 && colExpr < cells.size()) ? cells.at(colExpr).trimmed() : QString();

        if (cellName.isEmpty() && cellByte.isEmpty() && cellType.isEmpty()) continue;

        if (cellName.isEmpty())
        {
            errorList << QString("Line %1: Name is empty.").arg(rowNumber);
            continue;
        }
        if (seenNames.contains(cellName))
        {
            errorList << QString("Line %1: Duplicate field name '%2'.").arg(rowNumber).arg(cellName);
            continue;
        }
        seenNames.insert(cellName);

        bool byteOk = false;
        const int byteOffset = cellByte.toInt(&byteOk);
        if (!byteOk || byteOffset < 0)
        {
            errorList << QString("Line %1: ByteOffset must be a non-negative integer (got '%2').")
                          .arg(rowNumber).arg(cellByte);
            continue;
        }

        FieldDataType dataType = FieldDataType::RawUnsignedBE;
        if (!dataTypeFromLabel(cellType, dataType))
        {
            errorList << QString("Line %1: Unknown DataType '%2'. Accepted: %3.")
                          .arg(rowNumber).arg(cellType).arg(supportedDataTypeLabels().join(", "));
            continue;
        }

        int length;
        if (cellLen.isEmpty())
        {
            const int natural = fieldDataTypeNaturalLength(dataType);
            if (natural <= 0)
            {
                errorList << QString("Line %1: Length is required for DataType '%2'.")
                              .arg(rowNumber).arg(cellType);
                continue;
            }
            length = natural;
        }
        else
        {
            bool lenOk = false;
            length = cellLen.toInt(&lenOk);
            if (!lenOk || length < 1)
            {
                errorList << QString("Line %1: Length must be a positive integer (got '%2').")
                              .arg(rowNumber).arg(cellLen);
                continue;
            }
        }

        double resolution = 1.0;
        if (!cellRes.isEmpty())
        {
            bool resOk = false;
            resolution = cellRes.toDouble(&resOk);
            if (!resOk)
            {
                errorList << QString("Line %1: Resolution must be a number (got '%2').")
                              .arg(rowNumber).arg(cellRes);
                continue;
            }
        }

        if (payloadLengthBytes > 0)
        {
            const int correctedOffset = byteOffset - 1;
            if (correctedOffset < 0 || correctedOffset + length > payloadLengthBytes)
            {
                errorList << QString("Line %1: Field '%2' (ByteOffset %3, Length %4) exceeds payload length %5.")
                              .arg(rowNumber).arg(cellName).arg(byteOffset).arg(length).arg(payloadLengthBytes);
                continue;
            }
        }

        FieldDefinition field;
        field.name = cellName;
        field.byteOffset = byteOffset;
        field.byteOffsetcorrect = byteOffset - 1;
        field.length = length;
        field.dataType = dataType;
        field.resolution = resolution;
        field.resolutionExpression = cellExpr.isEmpty() ? QString("1") : cellExpr;
        out.append(field);
    }

    if (!errorList.isEmpty())
    {
        errorMessage = errorList.join("\n");
        out.clear();
        return false;
    }

    return true;
}

bool FieldCsvCodec::exportToCsv(const QString& path,
                                const QList<FieldDefinition>& fields,
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
    out << "Name,ByteOffset,DataType,Length,Resolution,ResolutionExpression\n";
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& f = fields.at(i);
        out << escapeCsvCell(f.name) << ','
            << f.byteOffset << ','
            << escapeCsvCell(dataTypeToLabel(f.dataType)) << ','
            << f.length << ','
            << QString::number(f.resolution, 'g', 15) << ','
            << escapeCsvCell(f.resolutionExpression)
            << '\n';
    }
    out.flush();
    file.close();
    return true;
}

bool FieldCsvCodec::writeTemplate(const QString& path, QString& errorMessage)
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
    out << "# PCAP UDP Extractor field-definition template\n";
    out << "# Lines starting with '#' are ignored.\n";
    out << "# Accepted DataType values: " << supportedDataTypeLabels().join(", ") << "\n";
    out << "# ByteOffset is 1-based (matches the dialog). Length is optional for fixed-size types.\n";
    out << "# Bitfield and Conditional decoders are NEVER imported from CSV - add manually in the dialog.\n";
    out << "Name,ByteOffset,DataType,Length,Resolution,ResolutionExpression\n";
    out << "# ExampleA,1,ushort,2,0.01,raw*0.01\n";
    out << "# ExampleB,3,float,4,1,1\n";
    out.flush();
    file.close();
    return true;
}
