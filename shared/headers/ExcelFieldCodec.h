#ifndef EXCELFIELDCODEC_H
#define EXCELFIELDCODEC_H

// Excel (.xlsx) import/export of a field list, shared by both apps. Columns:
// Name, ByteOffset, DataType, Length, Resolution, ResolutionExpression, Value,
// Endianness. Type labels are resolved via the shared FieldTypeLabels helpers.
//
// Bit / conditional decoders are NOT carried in the spreadsheet — use JSON for
// those. Built on the vendored QXlsx (linked into both apps).

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
