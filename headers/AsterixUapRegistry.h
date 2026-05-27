#ifndef ASTERIXUAPREGISTRY_H
#define ASTERIXUAPREGISTRY_H

// v15: static UAP definitions for the ASTERIX categories the app decodes.
// Implemented categories: CAT021 (ADS-B), CAT034 (radar service), CAT048
// (monoradar target), CAT062 (system track). Definitions are transcribed
// from EUROCONTROL specifications and stored in singletons built lazily.

#include "AsterixTypes.h"

#include <QList>

class AsterixUapRegistry
{
public:
    // Returns the UAP for the given category number, or nullptr if unsupported.
    // Pointer is valid for the lifetime of the program (static storage).
    static const AsterixCategoryDef* lookup(int category);

    // List of supported category numbers in ascending order.
    static QList<int> supportedCategories();

    // Human-readable name for the supported category, or empty if unsupported.
    static QString categoryDisplayName(int category);
};

#endif // ASTERIXUAPREGISTRY_H
