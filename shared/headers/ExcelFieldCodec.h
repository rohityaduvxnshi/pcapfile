#ifndef EXCELFIELDCODEC_H
#define EXCELFIELDCODEC_H

// Excel (.xlsx) import/export of a field list, shared by both apps. The columns
// mirror the CSV codec exactly (Name, ByteOffset, DataType, Length, Resolution,
// ResolutionExpression, Value, Endianness) and reuse FieldCsvCodec's type-label
// helpers, so a sheet exported here is the spreadsheet twin of the CSV.
//
// Bit / conditional decoders are NOT carried in the spreadsheet (same as CSV) —
// use JSON for those. Built on the vendored QXlsx (now linked into both apps).

#include "AppTypes.h"

#include <QList>
#include <QString>
#include <QStringList>

class ExcelFieldCodec
{
public:
    static bool exportFields(const QString& path,
                             const QList<FieldDefinition>& fields,
                             QString& errorMessage);

    // payloadLengthBytes > 0 enables the same bounds check the CSV importer does
    // (offset+length must fit). warnings collects non-fatal notes.
    static bool importFields(const QString& path,
                             int payloadLengthBytes,
                             QList<FieldDefinition>& out,
                             QStringList& warnings,
                             QString& errorMessage);
};

#endif // EXCELFIELDCODEC_H
