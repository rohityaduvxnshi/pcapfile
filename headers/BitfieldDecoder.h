#ifndef BITFIELDDECODER_H
#define BITFIELDDECODER_H

#include "AppTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class BitfieldDecoder
{
public:
    static QString rulesToJson(const QList<BitDecodeRule>& rules);
    static bool rulesFromJson(const QString& jsonText,
                              int fieldLengthBytes,
                              QList<BitDecodeRule>& rules,
                              QString& errorMessage);

    static bool parseBitPositions(const QString& text,
                                  int maxBitCount,
                                  QList<int>& bitPositions,
                                  QString& errorMessage);

    static bool validateRules(const QList<BitDecodeRule>& rules,
                              int fieldLengthBytes,
                              QString& errorMessage);

    static QString decodeRule(const QByteArray& fieldBytes, const BitDecodeRule& rule);
    static QString binaryString(quint64 value, int width);
    static bool binaryToValue(const QString& binary, quint64& value);
    static QString sanitizeColumnLabel(QString label);
    static QString bitsText(const QList<int>& bitPositions);
    static QString mappingSummary(const BitDecodeRule& rule, int maxCharacters = 90);
    static QString ruleTypeText(const BitDecodeRule& rule);
};

#endif // BITFIELDDECODER_H
