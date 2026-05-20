#ifndef EXTRACTIONENGINE_H
#define EXTRACTIONENGINE_H

#include "AppTypes.h"

#include <QStringList>

class ExtractionEngine
{
public:
    static QString valueFromPayload(const QByteArray& payload, const FieldDefinition& field);
    static QStringList valuesFromPayload(const QByteArray& payload, const QList<FieldDefinition>& fields);

    // Returns the CSV column header list in the same order that valuesFromPayload() produces values.
    static QStringList columnHeaders(const QList<FieldDefinition>& fields);
};

#endif
