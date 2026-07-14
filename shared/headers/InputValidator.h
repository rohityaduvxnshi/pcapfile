#ifndef INPUTVALIDATOR_H
#define INPUTVALIDATOR_H

#include "AppTypes.h"
#include "FilterTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class InputValidator
{
public:
    static bool validateFilePath(const QString& filePath, QString& errorMessage);
    static bool validatePortText(const QString& portText, int& port, QString& errorMessage);
    static bool validatePortValue(int port, QString& errorMessage);
    static bool solveResolutionExpression(const QString& expression, double& value, QString& errorMessage);
    // maxNumericLength caps the byte length of non-String fields. The default 8
    // preserves the historical quint64 limit (used by the simulator, whose
    // PayloadBuilder encodes via quint64). The reader passes kNoNumericLengthCap
    // because its ExtractionEngine decodes integers of any width.
    static const int kNoNumericLengthCap = 0;
    static bool validateField(const QString& name,
                              const QString& byteText,
                              const QString& lengthText,
                              const QString& resolutionText,
                              QString& errorMessage,
                              int maxNumericLength = 8);
    static bool validateFields(const QList<FieldDefinition>& fields, QString& errorMessage,
                               int maxNumericLength = 8);

    static int minMessageFilterCount();
    static int maxMessageFilterCount();
    static bool validateMessageFilterCount(int count, QString& errorMessage);
    static bool validateHeaderHexText(const QString& text,
                                      QByteArray& bytesOut,
                                      QString& labelOut,
                                      QString& errorMessage);
    static bool validatePortFilters(const QList<int>& ports,
                                    QList<MessageFilter>& outFilters,
                                    QString& errorMessage);
    static bool validateHeaderFilters(int commonPort,
                                      const QStringList& hexTexts,
                                      QList<MessageFilter>& outFilters,
                                      QString& errorMessage);
    static bool validateFilterConfiguration(const FilterConfiguration& config,
                                            QString& errorMessage);

private:
    static bool hexStringToBytes(const QString& normalizedHex,
                                 QByteArray& bytesOut,
                                 QString& errorMessage);
};

#endif // INPUTVALIDATOR_H
