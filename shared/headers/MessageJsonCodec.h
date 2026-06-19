#ifndef MESSAGEJSONCODEC_H
#define MESSAGEJSONCODEC_H

// One JSON interchange format shared by BOTH apps (the reader and the
// simulator). Because both apps use the same shared FieldDefinition /
// MessageDefinition structs, this codec serialises the FULL union of every
// member — the reader's decode configs (bit rules + conditional bit rules +
// NMEA) AND the simulator's send configs (send value, endianness, refresh rate,
// talker). Whichever app loads the file keeps the fields it uses and preserves
// the rest untouched, so a round-trip through either app is lossless.
//
// It deliberately serialises the bit-rule structs directly (not via the
// parser-only BitfieldDecoder classes) so the codec lives entirely in shared
// code and compiles into both apps.

#include "MessageDefinition.h"

#include <QList>
#include <QString>

class MessageJsonCodec
{
public:
    // Whole messages: fields + name / port / length / optional header /
    // compare options / data format / NMEA / send rate / send-enabled / talker.
    static QString messagesToJson(const QList<MessageDefinition>& messages);
    static bool messagesFromJson(const QString& jsonText,
                                 QList<MessageDefinition>& out,
                                 QString& errorMessage);

    // Single-message convenience wrappers (one message per file).
    static QString messageToJson(const MessageDefinition& message);
    static bool messageFromJson(const QString& jsonText,
                                MessageDefinition& out,
                                QString& errorMessage);

    // Field list only (no message metadata) — for the field-configuration
    // dialogs' Import/Export. Tolerant: also accepts a whole-message or
    // messages-array document and pulls the fields out.
    static QString fieldsToJson(const QList<FieldDefinition>& fields);
    static bool fieldsFromJson(const QString& jsonText,
                               QList<FieldDefinition>& out,
                               QString& errorMessage);
};

#endif // MESSAGEJSONCODEC_H
