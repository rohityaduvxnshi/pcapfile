#ifndef MESSAGEDEFINITION_H
#define MESSAGEDEFINITION_H

#include "AppTypes.h"

#include <QList>
#include <QString>
#include <QtGlobal>

struct MessageDefinition
{
    QString messageName;
    quint16 port;
    int payloadLengthBytes;
    QList<FieldDefinition> fields;

    MessageDefinition()
        : port(0),
          payloadLengthBytes(0),
          fields()
    {
    }
};

#endif // MESSAGEDEFINITION_H
