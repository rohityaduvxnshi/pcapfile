#ifndef MESSAGEDEFINITION_H
#define MESSAGEDEFINITION_H

#include "AppTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct MessageDefinition
{
    QString messageName;
    quint16 port;
    int payloadLengthBytes;
    QList<FieldDefinition> fields;

    // v12: optional disambiguator. When non-empty, packetMatchesMessage requires the
    // first N bytes of the UDP payload to match. Allows two messages of the same length
    // on the same port to be told apart by a header signature (e.g. a leading opcode byte).
    // Empty = no header check (pre-v12 behaviour).
    QByteArray optionalHeader;

    // v13: per-message compare options (header / terminator / checksum / refresh rate /
    // endianness). When hasCompareOptions is false, no extra CSV columns are emitted.
    bool hasCompareOptions;
    CompareOptionsConfig compareOptions;

    MessageDefinition()
        : port(0),
          payloadLengthBytes(0),
          fields(),
          optionalHeader(),
          hasCompareOptions(false),
          compareOptions()
    {
    }
};

#endif // MESSAGEDEFINITION_H
