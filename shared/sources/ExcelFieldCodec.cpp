#include "ExcelFieldCodec.h"

#include "FieldCsvCodec.h"

#include "xlsxcellrange.h"
#include "xlsxdocument.h"

#include <QMap>
#include <QSet>

namespace
{
// Column order written on export (1-based columns start at 1).
const char* const kHeaders[] = {
    "Name", "ByteOffset", "DataType", "Length",
    "Resolution", "ResolutionExpression", "Value", "Endianness"
};
const int kHeaderCount = 8;
} // namespace

bool ExcelFieldCodec::exportFields(const QString& path,
                                   const QList<FieldDefinition>& fields,
                                   QString& errorMessage)
{
    errorMessage.clear();

    QXlsx::Document doc;
    for (int c = 0; c < kHeaderCount; ++c)
        doc.write(1, c + 1, QString::fromLatin1(kHeaders[c]));

    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& f = fields.at(i);
        const int row = i + 2; // row 1 is the header
        doc.write(row, 1, f.name);
        doc.write(row, 2, f.byteOffset);
        doc.write(row, 3, FieldCsvCodec::dataTypeToLabel(f.dataType));
        doc.write(row, 4, f.length);
        doc.write(row, 5, f.resolution);
        doc.write(row, 6, f.resolutionExpression);
        doc.write(row, 7, f.sendValueText);
        doc.write(row, 8, f.endianness == FieldEndianness::Little ? QStringLiteral("LITTLE")
                                                                  : QStringLiteral("BIG"));
    }

    if (!doc.saveAs(path))
    {
        errorMessage = QString("Could not write the Excel file '%1'. "
                               "Solution: close it if it is open in Excel and choose a writable location.")
                           .arg(path);
        return false;
    }
    return true;
}

bool ExcelFieldCodec::importFields(const QString& path,
                                   int payloadLengthBytes,
                                   QList<FieldDefinition>& out,
                                   QStringList& warnings,
                                   QString& errorMessage)
{
    out.clear();
    warnings.clear();
    errorMessage.clear();

    QXlsx::Document doc(path);
    if (!doc.load())
    {
        errorMessage = QString("Could not open the Excel file '%1'. "
                               "Solution: make sure it is a valid .xlsx workbook (not .xls or a CSV).")
                           .arg(path);
        return false;
    }

    const QXlsx::CellRange dim = doc.dimension();
    if (dim.lastRow() < 2 || dim.lastColumn() < 1)
    {
        errorMessage = "The Excel sheet has no field rows. "
                       "Solution: use a sheet with a header row plus one row per field.";
        return false;
    }

    // Map the header row to columns (case-insensitive, same tokens as the CSV codec).
    int colName = -1, colByte = -1, colType = -1, colLen = -1, colRes = -1, colExpr = -1, colVal = -1, colEnd = -1;
    for (int c = dim.firstColumn(); c <= dim.lastColumn(); ++c)
    {
        const QString h = doc.read(1, c).toString().trimmed().toLower();
        if      (h == "name" || h == "fieldname" || h == "field name")              colName = c;
        else if (h == "byteoffset" || h == "byte offset" || h == "offset")          colByte = c;
        else if (h == "datatype" || h == "data type" || h == "type")                colType = c;
        else if (h == "length" || h == "len" || h == "bytes")                       colLen  = c;
        else if (h == "resolution" || h == "scale")                                 colRes  = c;
        else if (h == "resolutionexpression" || h == "resolution expression"
                 || h == "expression")                                              colExpr = c;
        else if (h == "value" || h == "sendvalue" || h == "send value")             colVal  = c;
        else if (h == "endianness" || h == "endian" || h == "byteorder"
                 || h == "byte order")                                              colEnd  = c;
    }
    if (colName < 0 || colByte < 0 || colType < 0)
    {
        errorMessage = "The header row must include Name, ByteOffset and DataType columns. "
                       "Solution: export an Excel file from this app first and use it as a template.";
        return false;
    }

    QStringList errorList;
    QSet<QString> seenNames;

    for (int r = 2; r <= dim.lastRow(); ++r)
    {
        const QString cellName = colName > 0 ? doc.read(r, colName).toString().trimmed() : QString();
        const QString cellByte = colByte > 0 ? doc.read(r, colByte).toString().trimmed() : QString();
        const QString cellType = colType > 0 ? doc.read(r, colType).toString().trimmed() : QString();
        const QString cellLen  = colLen  > 0 ? doc.read(r, colLen).toString().trimmed()  : QString();
        const QString cellRes  = colRes  > 0 ? doc.read(r, colRes).toString().trimmed()  : QString();
        const QString cellExpr = colExpr > 0 ? doc.read(r, colExpr).toString().trimmed() : QString();
        const QString cellVal  = colVal  > 0 ? doc.read(r, colVal).toString().trimmed()  : QString();
        const QString cellEnd  = colEnd  > 0 ? doc.read(r, colEnd).toString().trimmed()  : QString();

        if (cellName.isEmpty() && cellByte.isEmpty() && cellType.isEmpty())
            continue; // blank row

        if (cellName.isEmpty())
        {
            errorList << QString("Row %1: Name is empty.").arg(r);
            continue;
        }
        if (seenNames.contains(cellName))
        {
            errorList << QString("Row %1: Duplicate field name '%2'.").arg(r).arg(cellName);
            continue;
        }
        seenNames.insert(cellName);

        bool byteOk = false;
        const int byteOffset = cellByte.toInt(&byteOk);
        if (!byteOk || byteOffset < 0)
        {
            errorList << QString("Row %1: ByteOffset must be a non-negative integer (got '%2').")
                          .arg(r).arg(cellByte);
            continue;
        }

        FieldDataType dataType = FieldDataType::RawUnsignedBE;
        if (!FieldCsvCodec::dataTypeFromLabel(cellType, dataType))
        {
            errorList << QString("Row %1: Unknown DataType '%2'. Accepted: %3.")
                          .arg(r).arg(cellType).arg(FieldCsvCodec::supportedDataTypeLabels().join(", "));
            continue;
        }

        int length;
        if (cellLen.isEmpty())
        {
            const int natural = fieldDataTypeNaturalLength(dataType);
            if (natural <= 0)
            {
                errorList << QString("Row %1: Length is required for DataType '%2'.").arg(r).arg(cellType);
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
                errorList << QString("Row %1: Length must be a positive integer (got '%2').").arg(r).arg(cellLen);
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
                errorList << QString("Row %1: Resolution must be a number (got '%2').").arg(r).arg(cellRes);
                continue;
            }
        }

        if (payloadLengthBytes > 0)
        {
            const int correctedOffset = byteOffset - 1;
            if (correctedOffset < 0 || correctedOffset + length > payloadLengthBytes)
            {
                errorList << QString("Row %1: Field '%2' (ByteOffset %3, Length %4) exceeds payload length %5.")
                              .arg(r).arg(cellName).arg(byteOffset).arg(length).arg(payloadLengthBytes);
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
        field.sendValueText = cellVal;
        const QString endLower = cellEnd.toLower();
        if (endLower == "little" || endLower == "le" || endLower == "lsb" || endLower == "little-endian")
            field.endianness = FieldEndianness::Little;
        else
            field.endianness = FieldEndianness::Big;
        out.append(field);
    }

    if (!errorList.isEmpty())
    {
        errorMessage = errorList.join("\n");
        out.clear();
        return false;
    }
    if (out.isEmpty())
    {
        errorMessage = "No field rows were found in the sheet. "
                       "Solution: add one row per field beneath the header row.";
        return false;
    }
    return true;
}
