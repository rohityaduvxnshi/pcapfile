#ifndef PAYLOADBUILDER_H
#define PAYLOADBUILDER_H

// Pure payload ENCODER for the Universal Data Simulator — the exact inverse
// of the parser app's ExtractionEngine decode:
//
//   parser decode:    engineering value = raw (big-endian) * resolution
//   simulator encode: raw = round(engineering value / resolution), big-endian
//
// Signed types are stored as two's complement masked to length*8 bits (the
// inverse of the parser's signExtendRawValue). Float32 requires length 4 and
// Float64 length 8 (the parser shows "N/A" otherwise). String fields are
// UTF-8, NUL-padded to length (the parser trims trailing NULs on decode).
//
// Every failure reports a REASON ("what is wrong") and a SOLUTION ("what the
// user can do about it") so dialogs can show actionable errors.

#include "AppTypes.h"
#include "MessageDefinition.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

class PayloadBuilder
{
public:
    // Typed value text -> raw wire integer (integer-family types + Bool only:
    // RawUnsignedBE, Uint8..Int64, Bool). The result is already masked to
    // length*8 bits (two's complement for negatives).
    static bool rawFromTypedValue(const FieldDefinition& field,
                                  const QString& valueText,
                                  quint64& rawOut,
                                  QString& reason,
                                  QString& solution);

    // Raw wire integer -> typed value text (the value the parser would show
    // for these bytes). Used by the bit editor for the live readout and to
    // write the composed value back into the Value cell.
    static QString typedValueFromRaw(const FieldDefinition& field, quint64 rawValue);

    // Full single-field encode (every FieldDataType incl. Float32/64, Bool,
    // String). On success bytesOut.size() == field.length.
    static bool encodeFieldValue(const FieldDefinition& field,
                                 const QString& valueText,
                                 QByteArray& bytesOut,
                                 QString& reason,
                                 QString& solution);

    // For the live Hex cell next to the Value input: uppercase hex pairs
    // separated by spaces, exactly 2*length hex digits ("0A 1F" for 2 bytes).
    // Empty + shortError on failure.
    static QString fieldHexPreview(const FieldDefinition& field,
                                   const QString& valueText,
                                   QString& shortError);

    // HEX message: zero-filled payloadLengthBytes buffer with every field
    // encoded at its byteOffsetcorrect. All problems are collected (one line
    // per failing field, reason + solution) — nothing throws, nothing stops
    // at the first error.
    static bool buildHexPayload(const MessageDefinition& message,
                                QByteArray& payloadOut,
                                QStringList& problems);

    // NMEA message: "$" + talker + formatter + "," + tokens + "*HH" + CRLF.
    // Token slots are sized to the highest configured nmeaFieldIndex (and the
    // registry field count for predefined sentences); unconfigured slots stay
    // empty (null fields, ",,").
    static bool buildNmeaSentence(const MessageDefinition& message,
                                  QByteArray& sentenceOut,
                                  QStringList& problems);

    // 8-bit XOR of every byte strictly between '$' and '*'.
    static quint8 xorChecksum(const QByteArray& body);

    // True for the types the bit editor supports (integer family + Bool,
    // length 1..8). Float and String have no meaningful per-bit editing.
    static bool fieldSupportsBitEditing(const FieldDefinition& field);
};

#endif // PAYLOADBUILDER_H
