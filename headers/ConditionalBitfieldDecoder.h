#ifndef CONDITIONALBITFIELDDECODER_H
#define CONDITIONALBITFIELDDECODER_H

#include "AppTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class ConditionalBitfieldDecoder
{
public:
    static QString toJson(const ConditionalBitfieldDecoderConfig& decoder);
    static bool fromJson(const QString& jsonText,
                         ConditionalBitfieldDecoderConfig& decoder,
                         QString& errorMessage);

    static bool validate(const ConditionalBitfieldDecoderConfig& decoder,
                         const QList<FieldDefinition>& allFields,
                         const QString& dependentFieldName,
                         int dependentFieldLengthBytes,
                         QString& errorMessage);

    // Stable ordered column headers for all profiles combined.
    // Format: DepField_Profile, DepField_ProfA_Rule1, ..., DepField_ProfB_Rule1, ...
    static QStringList columnHeaders(const QString& dependentFieldName,
                                     const ConditionalBitfieldDecoderConfig& decoder);

    // Returns values in the exact same order as columnHeaders().
    // Only the matching profile's rule columns are filled; all others are blank.
    static QStringList decode(const QByteArray& dependentFieldBytes,
                              quint64 controllerRawValue,
                              bool controllerFound,
                              const ConditionalBitfieldDecoderConfig& decoder);
};

#endif // CONDITIONALBITFIELDDECODER_H
