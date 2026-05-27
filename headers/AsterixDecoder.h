#ifndef ASTERIXDECODER_H
#define ASTERIXDECODER_H

// v15: pure decoder for ASTERIX UDP payloads.
//
// Each ASTERIX datagram begins with a 3-byte header (CAT, 2-byte LEN). One
// datagram may carry multiple CAT-blocks back-to-back; each block in turn may
// carry multiple records. A record begins with a variable-length FSPEC bitmap
// whose bits index into the category's UAP. The decoder walks FSPEC, reads
// the bytes for each set item according to its AsterixItemKind, formats the
// value per AsterixValueKind, and emits one AsterixDecodedRecord per record.
//
// The decoder is read-only and does not mutate inputs. It collects warnings
// for partial decodes (unknown FRN, truncation) and sets fatalError if it
// cannot make further progress in the datagram.

#include "AsterixTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

struct AsterixDecodedItem
{
    int        frn;             // 1-based UAP position
    QString    id;              // "I048/010"
    QString    defaultName;
    QByteArray rawBytes;        // bytes consumed for this item
    QString    formattedValue;  // pre-formatted per item's AsterixValueKind

    AsterixDecodedItem() : frn(0) {}
};

struct AsterixDecodedRecord
{
    int                          category;
    int                          recordLengthBytes;
    QList<AsterixDecodedItem>    items;     // in FRN order (only items present)

    AsterixDecodedRecord() : category(0), recordLengthBytes(0) {}
};

class AsterixDecoder
{
public:
    struct Result
    {
        QList<AsterixDecodedRecord> records;
        QStringList                 warnings;
        bool                        fatalError;

        Result() : fatalError(false) {}
    };

    // Decode an entire UDP payload as ASTERIX, expecting `expectedCategory` at
    // the start of each CAT-block. Mismatched categories produce a warning and
    // the block is skipped.
    static Result decodePacket(int expectedCategory, const QByteArray& payload);

    // Format a single byte slice per the given value kind (used both by the
    // decoder itself and by callers that want to re-format a specific subset
    // of bytes, e.g. for the AsterixFieldConfigurationDialog preview).
    static QString formatValue(AsterixValueKind kind, const QByteArray& bytes);
};

#endif // ASTERIXDECODER_H
