#ifndef NMEADECODER_H
#define NMEADECODER_H

// Pure decoder for NMEA 0183 UDP payloads. Mirrors the role AsterixDecoder
// played for ASTERIX.
//
// A UDP datagram may carry one or more parametric sentences:
//   $aaccc,d1,d2,...,dn*hh<CR><LF>
// The decoder splits the payload into sentences, validates the XOR checksum,
// parses the talker + formatter from the address field, splits the data
// fields on ',', and emits one NmeaDecodedRecord per sentence whose formatter
// matches `expectedFormatter`. Non-matching sentences are skipped with a
// warning. The decoder is read-only and never mutates its inputs.

#include "NmeaTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

struct NmeaDecodedField
{
    int     index;           // 1-based comma position after the address field
    QString rawValue;        // token exactly as transmitted (may be empty = null field)
    QString formattedValue;  // formatted per the field's NmeaValueKind

    NmeaDecodedField() : index(0) {}
};

struct NmeaDecodedRecord
{
    QString                   talker;       // 2-char talker id, e.g. "GP"
    QString                   formatter;    // 3-char formatter, e.g. "GGA"
    bool                      checksumOk;   // false if the *hh did not match (or was absent)
    QList<NmeaDecodedField>   fields;       // every comma field, 1..n

    NmeaDecodedRecord() : checksumOk(false) {}

    // Returns the formatted field at the given 1-based index, or an empty
    // string if that field was not present in the sentence.
    QString valueAt(int oneBasedIndex) const;

    // Returns the raw (unformatted) token at the given 1-based index, or an
    // empty string if that field was not present. Used by custom sentences,
    // which re-format the raw token using a user-chosen value kind.
    QString rawValueAt(int oneBasedIndex) const;
};

class NmeaDecoder
{
public:
    struct Result
    {
        QList<NmeaDecodedRecord> records;
        QStringList              warnings;

        Result() {}
    };

    // Decode an entire UDP payload, keeping only sentences whose formatter
    // equals `expectedFormatter` (case-insensitive, talker ignored).
    static Result decodePacket(const QString& expectedFormatter, const QByteArray& payload);

    // Format a single raw token per the given value kind. Exposed so the
    // field configurator can preview formatting.
    static QString formatValue(NmeaValueKind kind, const QString& raw);
};

#endif // NMEADECODER_H
