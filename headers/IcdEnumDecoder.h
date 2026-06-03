#ifndef ICDENUMDECODER_H
#define ICDENUMDECODER_H

#include "AppTypes.h"

#include <QList>
#include <QString>

// Heuristic enum -> bit-decoder extraction for ICD import. Parses a free-text
// description/range cell (e.g. "0x01 - MANUALLY  0x02 - AUTOMATICALLY",
// "1 - Enabled / 0 - Disabled") into a single BitDecodeRule whose low bits cover
// the largest value found. Pure/deterministic, offline, no AI. A *starting point*
// the user reviews/edits in BitfieldDecoderDialog -- never applied silently.
//
// Genericity: works for any ICD that lists value->meaning pairs in prose. Prefers
// "0xNN" tokens when present; falls back to small decimals; requires >=2 mapped
// values whose meanings start with a letter (so numeric ranges like "0-359.9 deg"
// are ignored). Returns an empty list when no plausible enum is found.
class IcdEnumDecoder
{
public:
    static QList<BitDecodeRule> rulesFromDescription(const QString& description, const QString& label);
};

#endif // ICDENUMDECODER_H
