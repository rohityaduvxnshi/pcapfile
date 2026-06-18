#ifndef NMEATYPES_H
#define NMEATYPES_H

// NMEA 0183: data-only definitions for the registry-driven decoder.
//
// NMEA sentences are ASCII, comma-delimited lines of the form
//   $aaccc,d1,d2,...,dn*hh<CR><LF>
// where "aa" is the talker id, "ccc" the sentence formatter, and the data
// fields are positional. This file mirrors the (now removed) AsterixTypes.h
// in spirit: a registry maps each formatter to a fixed list of named fields,
// each addressed by its 1-based position after the address field.
//
// Unlike the Hex model there are no byte offsets — a field is selected by its
// comma index. The value kind drives how the raw token is formatted for CSV.

#include <QList>
#include <QString>

enum class NmeaValueKind
{
    Text,        // raw token, passed through
    Numeric,     // numeric token (x.x), passed through verbatim
    Latitude,    // llll.ll  -> "DD MM.mmmm" decimal-minutes form
    Longitude,   // yyyyy.yy -> "DDD MM.mmmm"
    Time,        // hhmmss.ss -> "hh:mm:ss.ss"
    Date,        // ddmmyy   -> "dd/mm/yy"
    Status,      // single char A/V etc., passed through
    Char         // single character field, passed through
};

// One positional field within a sentence.
struct NmeaFieldDef
{
    QString       name;   // default column name, e.g. "UTC"
    int           index;  // 1-based comma position after the address field
    NmeaValueKind kind;

    NmeaFieldDef() : index(0), kind(NmeaValueKind::Text) {}
    NmeaFieldDef(const QString& n, int i, NmeaValueKind k)
        : name(n), index(i), kind(k) {}
};

// One sentence formatter and its ordered field list.
struct NmeaSentenceDef
{
    QString               formatter;    // 3-char mnemonic, e.g. "GGA"
    QString               displayName;  // human label, e.g. "GGA - GPS Fix Data"
    QList<NmeaFieldDef>   fields;
};

#endif // NMEATYPES_H
