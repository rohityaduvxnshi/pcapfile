#ifndef EXTRACTIONENGINE_H
#define EXTRACTIONENGINE_H

#include "AppTypes.h"

class ExtractionEngine
{
public:
    static QString valueFromPayload(const QByteArray& payload, const FieldDefinition& field);
    static QStringList valuesFromPayload(const QByteArray& payload, const QList<FieldDefinition>& fields);
};

#endif
