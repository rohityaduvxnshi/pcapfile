#include "InputValidator.h"
#include "MathExpressionEvaluator.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace
{
const qint64 MAX_CAPTURE_FILE_SIZE = 500LL * 1024LL * 1024LL;

QString fieldDataTypeValidationName(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::Bool:
        return "bool";
    case FieldDataType::Uint8:
        return "uchar";
    case FieldDataType::Int8:
        return "char";
    case FieldDataType::Uint16:
        return "ushort";
    case FieldDataType::Int16:
        return "short";
    case FieldDataType::Uint32:
        return "uint";
    case FieldDataType::Int32:
        return "int";
    case FieldDataType::Uint64:
        return "ulong";
    case FieldDataType::Int64:
        return "long";
    case FieldDataType::Float32:
        return "float";
    case FieldDataType::Float64:
        return "double";
    case FieldDataType::RawUnsignedBE:
    default:
        return "Raw Unsigned BE";
    }
}
}

bool InputValidator::validateFilePath(const QString& filePath, QString& errorMessage)
{
    const QString trimmedPath = filePath.trimmed();

    if (trimmedPath.isEmpty())
    {
        errorMessage = "Please select a PCAP or PCAPNG file.";
        return false;
    }

    QFileInfo info(trimmedPath);

    if (!info.exists() || !info.isFile())
    {
        errorMessage = "Selected file does not exist.";
        return false;
    }

    if (info.size() <= 0)
    {
        errorMessage = "Selected file is empty.";
        return false;
    }

    if (info.size() > MAX_CAPTURE_FILE_SIZE)
    {
        errorMessage = "Selected file is too large for this lightweight version. Maximum allowed size is 500 MB.";
        return false;
    }

    const QString suffix = info.suffix().toLower();
    if (suffix != "pcap" && suffix != "pcapng")
    {
        errorMessage = "Only .pcap and .pcapng files are supported.";
        return false;
    }

    QFile file(trimmedPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage = "Selected file cannot be opened for reading.";
        return false;
    }

    file.close();
    return true;
}

bool InputValidator::validatePortText(const QString& portText, int& port, QString& errorMessage)
{
    bool ok = false;
    const int parsedPort = portText.trimmed().toInt(&ok, 10);

    if (!ok)
    {
        errorMessage = "UDP port must be an integer.";
        return false;
    }

    if (!validatePortValue(parsedPort, errorMessage))
    {
        return false;
    }

    port = parsedPort;
    return true;
}

bool InputValidator::validatePortValue(int port, QString& errorMessage)
{
    if (port < 0 || port > 65535)
    {
        errorMessage = "UDP port must be between 0 and 65535.";
        return false;
    }

    return true;
}

bool InputValidator::solveResolutionExpression(const QString& expression, double& value, QString& errorMessage)
{
    if (!MathExpressionEvaluator::evaluate(expression.trimmed(), value, errorMessage))
    {
        errorMessage = "Invalid resolution expression: " + errorMessage;
        return false;
    }

    if (value <= 0.0)
    {
        errorMessage = "Resolution must produce a value greater than 0.";
        return false;
    }

    return true;
}

bool InputValidator::validateField(const QString& name,
                                   const QString& byteText,
                                   const QString& lengthText,
                                   const QString& resolutionText,
                                   QString& errorMessage)
{
    if (name.trimmed().isEmpty())
    {
        errorMessage = "Field name cannot be empty.";
        return false;
    }

    bool byteOk = false;
    const int byteOffset = byteText.trimmed().toInt(&byteOk, 10);
    if (!byteOk || byteOffset < 1)
    {
        errorMessage = "Byte offset must be an integer greater than or equal to 1 (the first payload byte is byte 1).";
        return false;
    }

    bool lengthOk = false;
    const int length = lengthText.trimmed().toInt(&lengthOk, 10);
    if (!lengthOk || length <= 0)
    {
        errorMessage = "Length must be an integer greater than 0.";
        return false;
    }

    if (length > 8)
    {
        errorMessage = "Length greater than 8 bytes is not supported for integer field extraction.";
        return false;
    }

    const QString trimmedResolution = resolutionText.trimmed();
    if (!trimmedResolution.isEmpty())
    {
        double resolution = 0.0;
        if (!solveResolutionExpression(trimmedResolution, resolution, errorMessage))
        {
            return false;
        }
    }

    if (byteOffset > 1000000)
    {
        errorMessage = "Byte offset is too large.";
        return false;
    }

    return true;
}

bool InputValidator::validateFields(const QList<FieldDefinition>& fields, QString& errorMessage)
{
    QSet<QString> names;

    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);
        const QString normalizedName = field.name.trimmed().toLower();

        if (normalizedName.isEmpty())
        {
            errorMessage = QString("Field row %1 has an empty name.").arg(i + 1);
            return false;
        }

        if (names.contains(normalizedName))
        {
            errorMessage = QString("Duplicate field name found: %1").arg(field.name);
            return false;
        }

        if (field.byteOffset < 1)
        {
            errorMessage = QString("Field %1 has invalid byte offset (must be 1 or greater).").arg(field.name);
            return false;
        }

        if (field.length <= 0 || field.length > 8)
        {
            errorMessage = QString("Field %1 has invalid length. Supported length is 1 to 8 bytes.").arg(field.name);
            return false;
        }

        const int naturalLength = fieldDataTypeNaturalLength(field.dataType);
        if (naturalLength > 0 && field.length != naturalLength)
        {
            errorMessage = QString("Field %1 has invalid length. %2 requires length %3.")
                .arg(field.name)
                .arg(fieldDataTypeValidationName(field.dataType))
                .arg(naturalLength);
            return false;
        }

        if (field.resolution <= 0.0)
        {
            errorMessage = QString("Field %1 has invalid resolution.").arg(field.name);
            return false;
        }

        names.insert(normalizedName);
    }

    return true;
}
