#ifndef NMEASENTENCEREGISTRY_H
#define NMEASENTENCEREGISTRY_H

// Built-in catalogue of supported NMEA 0183 sentence formatters and their
// positional fields. Mirrors the role the AsterixUapRegistry played for
// ASTERIX categories. The definitions are transcribed from the NMEA 0183
// v3.01 standard and stored in a lazily-built singleton list.
//
// Extending the catalogue is purely additive: append a NmeaSentenceDef in
// NmeaSentenceRegistry.cpp.

#include "NmeaTypes.h"

#include <QList>
#include <QString>

class NmeaSentenceRegistry
{
public:
    // Returns the definition for a 3-char formatter (case-insensitive), or
    // nullptr if unsupported. Pointer is valid for the program lifetime.
    static const NmeaSentenceDef* lookup(const QString& formatter);

    // Supported formatter mnemonics in catalogue order.
    static QList<QString> supportedFormatters();

    // Human label for the formatter, or empty if unsupported.
    static QString displayName(const QString& formatter);
};

#endif // NMEASENTENCEREGISTRY_H
