#ifndef ASTERIXTYPES_H
#define ASTERIXTYPES_H

// v15: ASTERIX (EUROCONTROL surveillance data format) core type definitions.
//
// ASTERIX records start with a variable-length FSPEC bitmap that indicates which
// data items are present. Each category (CAT021, CAT048, etc.) has its own User
// Application Profile (UAP) — the ordered list of items addressable by FSPEC bits.
// Items are one of four structural kinds (Fixed, Extended, Repetitive, Compound)
// plus the SPF/RE explicit-length form. Values are byte slices that must be
// formatted per item-specific semantics (Time-of-Day, Lat/Lon, Mode-3/A, etc.).
//
// This header is data-only — no Qt widgets, no decoder logic. AsterixDecoder
// consumes these definitions; AsterixUapRegistry provides the static UAPs.

#include <QList>
#include <QString>
#include <QtGlobal>

enum class AsterixItemKind
{
    Fixed,          // exactly fixedLength bytes
    Extended,       // 1+ extents of extendedExtentLength bytes; FX=LSB continues
    Repetitive,     // 1-byte REP, then REP * repetitiveElementLength bytes
    Compound,       // sub-FSPEC bitmap + sub-items in compoundSubItems order
    ExplicitLength, // 1-byte LEN (includes itself), then LEN-1 bytes (SPF/RE)
    Unknown         // catch-all; decoder aborts the record gracefully if hit
};

enum class AsterixValueKind
{
    HexBytes,        // default — emit hex string (uppercase, no spaces)
    UintBE,          // unsigned big-endian (1..8 bytes)
    IntBE,           // signed big-endian, two's complement (1..8 bytes)
    Float32BE,
    TimeOfDay,       // 3-byte unsigned, LSB = 1/128 s — "HH:MM:SS.sss"
    Lat3byte,        // signed 3-byte, LSB = 180/2^23 deg — degrees
    Lon3byte,        // signed 3-byte, LSB = 180/2^23 deg — degrees
    Lat4byte,        // signed 4-byte, LSB = 180/2^25 deg — degrees
    Lon4byte,        // signed 4-byte, LSB = 180/2^25 deg — degrees
    Mode3A,          // 12-bit code in lower 12 bits of 2-byte field; octal
    ModeC_FL,        // 2-byte signed, LSB = 1/4 FL
    Callsign6,       // 6 bytes, 8 chars of 6-bit ICAO encoding
    Address24bit,    // 3-byte hex ICAO 24-bit address
    MultiPart        // composite / repetitive / compound — formatter emits hex
};

struct AsterixSubItem
{
    QString          id;            // e.g. "SRP"
    QString          defaultName;   // e.g. "SSR Plot Runlength"
    int              byteLength;    // fixed for sub-items
    AsterixValueKind valueKind;

    AsterixSubItem()
        : byteLength(0),
          valueKind(AsterixValueKind::HexBytes)
    {
    }
};

struct AsterixItemDef
{
    int                    frn;                       // 1-based UAP Field Reference Number
    QString                id;                        // e.g. "I048/010"
    QString                defaultName;               // e.g. "Data Source Identifier"
    AsterixItemKind        kind;
    int                    fixedLength;               // Fixed
    int                    extendedExtentLength;      // Extended (per extent)
    int                    repetitiveElementLength;   // Repetitive (per element)
    QList<AsterixSubItem>  compoundSubItems;          // Compound (in sub-FSPEC order)
    AsterixValueKind       valueKind;

    AsterixItemDef()
        : frn(0),
          kind(AsterixItemKind::Unknown),
          fixedLength(0),
          extendedExtentLength(0),
          repetitiveElementLength(0),
          valueKind(AsterixValueKind::HexBytes)
    {
    }
};

struct AsterixCategoryDef
{
    int                     category;   // 21, 34, 48, 62
    QString                 name;
    QList<AsterixItemDef>   uap;        // FRN order, uap[frn-1] = item at FRN

    AsterixCategoryDef() : category(0) {}
};

#endif // ASTERIXTYPES_H
