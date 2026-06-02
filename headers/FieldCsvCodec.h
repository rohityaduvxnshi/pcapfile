#ifndef FIELDCSVCODEC_H
#define FIELDCSVCODEC_H

#include "AppTypes.h"

#include <QList>
#include <QString>
#include <QStringList>

class FieldCsvCodec
{
public:
    static bool importFromCsv(const QString& path,
                              int payloadLengthBytes,
                              QList<FieldDefinition>& out,
                              QStringList& warnings,
                              QString& errorMessage);

    static bool exportToCsv(const QString& path,
                            const QList<FieldDefinition>& fields,
                            QString& errorMessage);

    static bool writeTemplate(const QString& path, QString& errorMessage);

    static QStringList supportedDataTypeLabels();
    static QString dataTypeToLabel(FieldDataType dataType);
    static bool dataTypeFromLabel(const QString& label, FieldDataType& dataType);

    // Generic, ICD-friendly resolver: maps a free-form type word ("Unsigned Integer",
    // "Uchar", "Float", "Signed Short", ...) plus a byte size to a FieldDataType.
    // Tries dataTypeFromLabel() first, then keyword + size logic (the Size column
    // disambiguates integer width). sizeBytes <= 0 means "size unknown" (width is
    // then inferred from the keyword). Returns false only for unrecognised words.
    static bool dataTypeFromLabelAndSize(const QString& label, int sizeBytes, FieldDataType& dataType);
};

#endif // FIELDCSVCODEC_H
