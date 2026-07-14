#ifndef FIELDTYPELABELS_H
#define FIELDTYPELABELS_H

#include "AppTypes.h"

#include <QString>
#include <QStringList>

// Shared mapping between FieldDataType and the human / ICD type spellings used
// across the suite (field dialogs, Excel field codec, ICD import). This was
// previously bolted onto FieldCsvCodec; it now lives on its own so the CSV codec
// can be removed while the Excel + ICD paths keep their type resolution.
class FieldTypeLabels
{
public:
    // Canonical short labels offered in the UI (one per FieldDataType).
    static QStringList supportedDataTypeLabels();

    // FieldDataType -> canonical short label (e.g. Uint16 -> "ushort").
    static QString dataTypeToLabel(FieldDataType dataType);

    // Exact (case-insensitive) label/alias -> FieldDataType. Returns false for
    // an unrecognised word.
    static bool dataTypeFromLabel(const QString& label, FieldDataType& dataType);

    // Generic, ICD-friendly resolver: maps a free-form type word ("Unsigned
    // Integer", "Uchar", "Float", "Signed Short", ...) plus a byte size to a
    // FieldDataType. Tries dataTypeFromLabel() first, then keyword + size logic
    // (the Size column disambiguates integer width). sizeBytes <= 0 means "size
    // unknown" (width is inferred from the keyword). Returns false only for
    // unrecognised words.
    static bool dataTypeFromLabelAndSize(const QString& label, int sizeBytes, FieldDataType& dataType);
};

#endif // FIELDTYPELABELS_H
