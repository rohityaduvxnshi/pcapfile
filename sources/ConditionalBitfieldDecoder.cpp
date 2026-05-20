#include "ConditionalBitfieldDecoder.h"
#include "BitfieldDecoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

QString ConditionalBitfieldDecoder::toJson(const ConditionalBitfieldDecoderConfig& decoder)
{
    QJsonObject root;
    root["controllerFieldName"] = decoder.controllerFieldName;
    root["unknownBehavior"] = decoder.unknownBehavior;

    QJsonArray profilesArray;
    for (int p = 0; p < decoder.profiles.size(); ++p)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(p);
        QJsonObject profileObj;
        profileObj["profileName"] = profile.profileName;
        profileObj["controllerValue"] = QString::number(profile.controllerValue);
        profileObj["rules"] = BitfieldDecoder::rulesToJson(profile.bitDecodeRules);

        QJsonArray exclusionArray;
        for (int e = 0; e < profile.exclusionRules.size(); ++e)
        {
            const ConditionalBitExclusionRule& exRule = profile.exclusionRules.at(e);
            QJsonObject exObj;
            QJsonArray bitsArray;
            for (int b = 0; b < exRule.mutuallyExclusiveBits.size(); ++b)
                bitsArray.append(exRule.mutuallyExclusiveBits.at(b));
            exObj["bits"] = bitsArray;
            exObj["validationLabel"] = exRule.validationLabel;
            exObj["invalidMessage"] = exRule.invalidMessage;
            exclusionArray.append(exObj);
        }
        profileObj["exclusionRules"] = exclusionArray;

        profilesArray.append(profileObj);
    }
    root["profiles"] = profilesArray;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool ConditionalBitfieldDecoder::fromJson(const QString& jsonText,
                                           ConditionalBitfieldDecoderConfig& decoder,
                                           QString& errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (doc.isNull())
    {
        errorMessage = "JSON parse error: " + parseError.errorString();
        return false;
    }
    if (!doc.isObject())
    {
        errorMessage = "Expected JSON object at root.";
        return false;
    }

    const QJsonObject root = doc.object();
    decoder.controllerFieldName = root.value("controllerFieldName").toString().trimmed();

    const QString ub = root.value("unknownBehavior").toString().trimmed().toUpper();
    decoder.unknownBehavior = (ub == "BLANK") ? "BLANK" : "UNKNOWN_CONTROLLER";

    decoder.profiles.clear();
    const QJsonArray profilesArray = root.value("profiles").toArray();
    for (int p = 0; p < profilesArray.size(); ++p)
    {
        const QJsonObject profileObj = profilesArray.at(p).toObject();
        ConditionalBitDecodeProfile profile;
        profile.profileName = profileObj.value("profileName").toString().trimmed();

        bool ok = false;
        const QString valStr = profileObj.value("controllerValue").toString().trimmed();
        profile.controllerValue = valStr.toULongLong(&ok, 0);
        if (!ok)
        {
            errorMessage = QString("Profile '%1': invalid controllerValue '%2'.").arg(profile.profileName).arg(valStr);
            return false;
        }

        const QString rulesJson = profileObj.value("rules").toString();
        if (!rulesJson.trimmed().isEmpty())
        {
            QString ruleError;
            // Deserialize with permissive max length (8 bytes = 64 bits); validate() enforces actual length.
            if (!BitfieldDecoder::rulesFromJson(rulesJson, 8, profile.bitDecodeRules, ruleError))
            {
                errorMessage = QString("Profile '%1' rules: %2").arg(profile.profileName).arg(ruleError);
                return false;
            }
        }

        const QJsonArray exclusionArray = profileObj.value("exclusionRules").toArray();
        for (int e = 0; e < exclusionArray.size(); ++e)
        {
            const QJsonObject exObj = exclusionArray.at(e).toObject();
            ConditionalBitExclusionRule exRule;
            const QJsonArray bitsArray = exObj.value("bits").toArray();
            for (int b = 0; b < bitsArray.size(); ++b)
                exRule.mutuallyExclusiveBits.append(bitsArray.at(b).toInt());
            exRule.validationLabel = exObj.value("validationLabel").toString().trimmed();
            exRule.invalidMessage = exObj.value("invalidMessage").toString();
            profile.exclusionRules.append(exRule);
        }

        decoder.profiles.append(profile);
    }

    return true;
}

bool ConditionalBitfieldDecoder::validate(const ConditionalBitfieldDecoderConfig& decoder,
                                           const QList<FieldDefinition>& allFields,
                                           const QString& dependentFieldName,
                                           int dependentFieldLengthBytes,
                                           QString& errorMessage)
{
    if (decoder.controllerFieldName.trimmed().isEmpty())
    {
        errorMessage = "Controller field name cannot be empty.";
        return false;
    }

    if (decoder.controllerFieldName == dependentFieldName)
    {
        errorMessage = QString("Controller field cannot be the same as the dependent field '%1'.").arg(dependentFieldName);
        return false;
    }

    int controllerLength = -1;
    bool controllerFound = false;
    for (int i = 0; i < allFields.size(); ++i)
    {
        if (allFields.at(i).name == decoder.controllerFieldName)
        {
            controllerLength = allFields.at(i).length;
            controllerFound = true;
            break;
        }
    }

    if (!controllerFound)
    {
        errorMessage = QString("Controller field '%1' not found in the field list.").arg(decoder.controllerFieldName);
        return false;
    }

    if (controllerLength < 1 || controllerLength > 8)
    {
        errorMessage = QString("Controller field '%1' length must be between 1 and 8 bytes.").arg(decoder.controllerFieldName);
        return false;
    }

    if (decoder.profiles.isEmpty())
    {
        errorMessage = "At least one profile must be configured.";
        return false;
    }

    QList<quint64> seenValues;
    QList<QString> seenNames;
    QList<QString> seenSanitizedNames;

    for (int p = 0; p < decoder.profiles.size(); ++p)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(p);

        if (profile.profileName.trimmed().isEmpty())
        {
            errorMessage = QString("Profile %1: profile name cannot be empty.").arg(p + 1);
            return false;
        }

        if (seenNames.contains(profile.profileName))
        {
            errorMessage = QString("Duplicate profile name '%1'.").arg(profile.profileName);
            return false;
        }
        seenNames.append(profile.profileName);

        const QString sanitized = BitfieldDecoder::sanitizeColumnLabel(profile.profileName);
        if (seenSanitizedNames.contains(sanitized))
        {
            errorMessage = QString("Profile name '%1' conflicts with another profile after sanitization.").arg(profile.profileName);
            return false;
        }
        seenSanitizedNames.append(sanitized);

        if (seenValues.contains(profile.controllerValue))
        {
            errorMessage = QString("Duplicate controller value 0x%1 in profile '%2'.")
                .arg(profile.controllerValue, 0, 16).arg(profile.profileName);
            return false;
        }
        seenValues.append(profile.controllerValue);

        QString ruleError;
        if (!BitfieldDecoder::validateRules(profile.bitDecodeRules, dependentFieldLengthBytes, ruleError))
        {
            errorMessage = QString("Profile '%1' rules: %2").arg(profile.profileName).arg(ruleError);
            return false;
        }

        const int maxBits = dependentFieldLengthBytes * 8;
        for (int e = 0; e < profile.exclusionRules.size(); ++e)
        {
            const ConditionalBitExclusionRule& exRule = profile.exclusionRules.at(e);
            if (exRule.mutuallyExclusiveBits.size() < 2)
            {
                errorMessage = QString("Profile '%1' exclusion rule %2: must specify at least 2 bit positions.")
                    .arg(profile.profileName).arg(e + 1);
                return false;
            }
            for (int b = 0; b < exRule.mutuallyExclusiveBits.size(); ++b)
            {
                const int bitPos = exRule.mutuallyExclusiveBits.at(b);
                if (bitPos < 0 || bitPos >= maxBits)
                {
                    errorMessage = QString("Profile '%1' exclusion rule %2: bit %3 out of range (0-%4).")
                        .arg(profile.profileName).arg(e + 1).arg(bitPos).arg(maxBits - 1);
                    return false;
                }
            }
            if (exRule.validationLabel.trimmed().isEmpty())
            {
                errorMessage = QString("Profile '%1' exclusion rule %2: validation label cannot be empty.")
                    .arg(profile.profileName).arg(e + 1);
                return false;
            }
        }
    }

    return true;
}

QStringList ConditionalBitfieldDecoder::columnHeaders(const QString& dependentFieldName,
                                                        const ConditionalBitfieldDecoderConfig& decoder)
{
    QStringList headers;
    headers << dependentFieldName + "_Profile";

    for (int p = 0; p < decoder.profiles.size(); ++p)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(p);
        const QString profSanitized = BitfieldDecoder::sanitizeColumnLabel(profile.profileName);

        for (int r = 0; r < profile.bitDecodeRules.size(); ++r)
        {
            const BitDecodeRule& rule = profile.bitDecodeRules.at(r);
            headers << dependentFieldName + "_" + profSanitized + "_" + BitfieldDecoder::sanitizeColumnLabel(rule.label);
        }

        for (int e = 0; e < profile.exclusionRules.size(); ++e)
        {
            const ConditionalBitExclusionRule& exRule = profile.exclusionRules.at(e);
            headers << dependentFieldName + "_" + profSanitized + "_Validation_" + BitfieldDecoder::sanitizeColumnLabel(exRule.validationLabel);
        }
    }

    return headers;
}

QStringList ConditionalBitfieldDecoder::decode(const QByteArray& depFieldBytes,
                                                quint64 controllerRawValue,
                                                bool controllerFound,
                                                const ConditionalBitfieldDecoderConfig& decoder)
{
    // Count total columns to guarantee output always matches columnHeaders()
    int totalCols = 1;
    for (int p = 0; p < decoder.profiles.size(); ++p)
    {
        totalCols += decoder.profiles.at(p).bitDecodeRules.size();
        totalCols += decoder.profiles.at(p).exclusionRules.size();
    }

    QStringList result;
    for (int i = 0; i < totalCols; ++i)
        result << QString();

    const bool useUnknownLabel = decoder.unknownBehavior.toUpper() != "BLANK";

    if (!controllerFound)
    {
        if (useUnknownLabel)
            result[0] = QString("UNKNOWN_CONTROLLER(0x%1)").arg(controllerRawValue, 0, 16).toUpper();
        return result;
    }

    int colIndex = 1;
    for (int p = 0; p < decoder.profiles.size(); ++p)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(p);
        const int profileCols = profile.bitDecodeRules.size() + profile.exclusionRules.size();

        if (profile.controllerValue == controllerRawValue)
        {
            result[0] = profile.profileName;

            for (int r = 0; r < profile.bitDecodeRules.size(); ++r)
            {
                if (!depFieldBytes.isEmpty())
                    result[colIndex + r] = BitfieldDecoder::decodeRule(depFieldBytes, profile.bitDecodeRules.at(r));
            }

            for (int e = 0; e < profile.exclusionRules.size(); ++e)
            {
                const ConditionalBitExclusionRule& exRule = profile.exclusionRules.at(e);
                int setBitsCount = 0;
                for (int b = 0; b < exRule.mutuallyExclusiveBits.size(); ++b)
                {
                    const int bitPos = exRule.mutuallyExclusiveBits.at(b);
                    const int byteIdx = bitPos / 8;
                    const int bitIdx = bitPos % 8;
                    if (byteIdx < depFieldBytes.size())
                    {
                        const quint8 byteVal = static_cast<quint8>(depFieldBytes.at(byteIdx));
                        if ((byteVal >> bitIdx) & 0x01)
                            ++setBitsCount;
                    }
                }
                result[colIndex + profile.bitDecodeRules.size() + e] =
                    (setBitsCount > 1) ? exRule.invalidMessage : "OK";
            }

            return result;
        }

        colIndex += profileCols;
    }

    // No profile matched
    if (useUnknownLabel)
        result[0] = QString("UNKNOWN_CONTROLLER(0x%1)").arg(controllerRawValue, 0, 16).toUpper();

    return result;
}
