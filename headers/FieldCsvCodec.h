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
};

#endif // FIELDCSVCODEC_H
