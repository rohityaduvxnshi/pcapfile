#ifndef INPUTVALIDATOR_H
#define INPUTVALIDATOR_H

#include "AppTypes.h"

#include <QString>
#include <QList>

class InputValidator
{
public:
    static bool validateFilePath(const QString& filePath, QString& errorMessage);
    static bool validatePortText(const QString& portText, int& port, QString& errorMessage);
    static bool validatePortValue(int port, QString& errorMessage);
    static bool solveResolutionExpression(const QString& expression, double& value, QString& errorMessage);
    static bool validateField(const QString& name,
                              const QString& byteText,
                              const QString& lengthText,
                              const QString& resolutionText,
                              QString& errorMessage);
    static bool validateFields(const QList<FieldDefinition>& fields, QString& errorMessage);
};

#endif // INPUTVALIDATOR_H
