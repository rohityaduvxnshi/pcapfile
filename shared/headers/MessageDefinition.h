#ifndef MESSAGEDEFINITION_H
#define MESSAGEDEFINITION_H

#include "AppTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
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

    // NMEA: per-message data format selector. dataFormat == "HEX" (default)
    // preserves all pre-existing byte-offset behaviour. dataFormat == "NMEA"
    // routes extraction through NmeaDecoder and uses nmeaSentenceType (the
    // 3-char formatter, e.g. "GGA") to pick the sentence definition. Empty
    // nmeaSentenceType = unset.
    QString dataFormat;
    QString nmeaSentenceType;

    // Simulator: how often this message is re-sent while streaming (one rate
    // per message; every field travels together in one payload).
    double sendFrequencyHz;

    // Simulator: the "Send?" tick — only ticked messages are streamed.
    bool sendEnabled;

    // Simulator, NMEA only: 2-char talker id prepended before the formatter
    // when building the sentence ($GPGGA -> talker "GP").
    QString nmeaTalker;

    // Field byte offsets may be entered/shown in BYTES (default) or WORDS
    // (1 word = 2 bytes). This is purely a display/entry unit for the field
    // table: field.byteOffset is ALWAYS stored in bytes and field.length is
    // ALWAYS in bytes, so the decode/encode engines never change. Auto-detected
    // on ICD import. Value: "BYTES" or "WORDS".
    QString offsetUnit;

    // Multi-connection binding. Empty = unbound (legacy / "any" connection). When
    // non-empty it holds the id of a ConnectionDefinition (see ConnectionTypes.h):
    //   Parser    - the live-capture connection whose datagrams this message decodes,
    //               so traffic from different adapters/ports is never mixed.
    //   Simulator - the send connection this message is transmitted on.
    // Loaded leniently: an empty/unknown id falls back to the default connection.
    QString connectionId;

    // Simulator multi-connection send: when non-empty, the message is transmitted
    // to EVERY connection id in this list (one frame per destination), so a single
    // message can fan out to several locations at once. Empty = fall back to the
    // single connectionId above, and if that is also empty, to the default (first)
    // connection. The parser ignores this field (a received message decodes one
    // source); it is preserved through save/load so it round-trips between apps.
    QStringList connectionIds;

    MessageDefinition()
        : port(0),
          payloadLengthBytes(0),
          fields(),
          optionalHeader(),
          hasCompareOptions(false),
          compareOptions(),
          dataFormat("HEX"),
          nmeaSentenceType(),
          sendFrequencyHz(1.0),
          sendEnabled(true),
          nmeaTalker("GP"),
          offsetUnit("BYTES"),
          connectionId(),
          connectionIds()
    {
    }
};

// The explicit list of connection ids a simulator message is bound to: the
// multi-select list when present, else the single legacy binding, else empty
// (the caller substitutes the default/first connection). Centralises the
// back-compat fallback so every send/UI site agrees.
inline QStringList messageConnectionIds(const MessageDefinition& m)
{
    if (!m.connectionIds.isEmpty())
        return m.connectionIds;
    if (!m.connectionId.isEmpty())
        return QStringList() << m.connectionId;
    return QStringList();
}

#endif // MESSAGEDEFINITION_H
