#include "ConditionalBitfieldDecoder.h"

#include "BitfieldDecoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace
{
QString normalizedUnknownBehavior(const QString& behavior)
{
    const QString upper = behavior.trimmed().toUpper();
    if (upper == "BLANK") return "BLANK";
    return "UNKNOWN_CONTROLLER";
}
}

QString ConditionalBitfieldDecoder::toJson(const ConditionalBitfieldDecoderConfig& decoder)
{
    QJsonArray profilesArray;
    for (int i = 0; i < decoder.profiles.size(); ++i)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(i);
        QJsonObject profileObject;
        profileObject.insert("profileName", profile.profileName);
        profileObject.insert("controllerValue", QString::number(profile.controllerValue));
        profileObject.insert("rules", BitfieldDecoder::rulesToJson(profile.bitDecodeRules));
        profilesArray.append(profileObject);
    }

    QJsonObject root;
    root.insert("controllerFieldName", decoder.controllerFieldName);
    root.insert("unknownBehavior", normalizedUnknownBehavior(decoder.unknownBehavior));
    root.insert("profiles", profilesArray);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool ConditionalBitfieldDecoder::fromJson(const QString& jsonText,
                                          ConditionalBitfieldDecoderConfig& decoder,
                                          QString& errorMessage)
{
    decoder = ConditionalBitfieldDecoderConfig();
    errorMessage.clear();

    const QString trimmed = jsonText.trimmed();
    if (trimmed.isEmpty()) return true;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        errorMessage = "Conditional bitfield decoder data is not valid JSON.";
        return false;
    }

    const QJsonObject root = doc.object();
    decoder.controllerFieldName = root.value("controllerFieldName").toString().trimmed();
    decoder.unknownBehavior = normalizedUnknownBehavior(root.value("unknownBehavior").toString("UNKNOWN_CONTROLLER"));

    const QJsonArray profilesArray = root.value("profiles").toArray();
    for (int i = 0; i < profilesArray.size(); ++i)
    {
        const QJsonObject profileObject = profilesArray.at(i).toObject();
        ConditionalBitDecodeProfile profile;
        profile.profileName = profileObject.value("profileName").toString().trimmed();

        bool valueOk = false;
        profile.controllerValue = profileObject.value("controllerValue").toString().toULongLong(&valueOk, 0);
        if (!valueOk)
        {
            errorMessage = QString("Profile %1 has an invalid controller value.").arg(i + 1);
            return false;
        }

        // Parse rules using lenient length (8) — validate() will re-check with the real field length.
        const QString rulesJson = profileObject.value("rules").toString();
        if (!rulesJson.trimmed().isEmpty())
        {
            QString ruleError;
            if (!BitfieldDecoder::rulesFromJson(rulesJson, 8, profile.bitDecodeRules, ruleError))
            {
                errorMessage = QString("Profile '%1' has invalid rules: %2").arg(profile.profileName).arg(ruleError);
                return false;
            }
        }

        decoder.profiles << profile;
    }

    return true;
}

bool ConditionalBitfieldDecoder::validate(const ConditionalBitfieldDecoderConfig& decoder,
                                          const QList<FieldDefinition>& allFields,
                                          const QString& dependentFieldName,
                                          int dependentFieldLengthBytes,
                                          QString& errorMessage)
{
    errorMessage.clear();

    if (decoder.controllerFieldName.trimmed().isEmpty())
    {
        errorMessage = "Controller field name cannot be empty.";
        return false;
    }

    if (decoder.controllerFieldName.trimmed() == dependentFieldName.trimmed())
    {
        errorMessage = "Controller field cannot be the same as the dependent field.";
        return false;
    }

    bool controllerFound = false;
    int controllerLength = 0;
    for (int i = 0; i < allFields.size(); ++i)
    {
        if (allFields.at(i).name.trimmed() == decoder.controllerFieldName.trimmed())
        {
            controllerFound = true;
            controllerLength = allFields.at(i).length;
            break;
        }
    }

    if (!controllerFound)
    {
        errorMessage = QString("Controller field '%1' does not exist in this message.").arg(decoder.controllerFieldName);
        return false;
    }

    if (controllerLength < 1 || controllerLength > 8)
    {
        errorMessage = QString("Controller field '%1' length must be between 1 and 8 bytes.").arg(decoder.controllerFieldName);
        return false;
    }

    if (decoder.profiles.isEmpty())
    {
        errorMessage = "Conditional bitfield decoder must have at least one profile.";
        return false;
    }

    QSet<quint64> controllerValues;
    QSet<QString> profileNamesLower;
    QSet<QString> sanitizedNames;

    for (int i = 0; i < decoder.profiles.size(); ++i)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(i);

        if (profile.profileName.trimmed().isEmpty())
        {
            errorMessage = QString("Profile %1 has an empty name.").arg(i + 1);
            return false;
        }

        const QString lowerName = profile.profileName.trimmed().toLower();
        if (profileNamesLower.contains(lowerName))
        {
            errorMessage = QString("Duplicate profile name: '%1'.").arg(profile.profileName);
            return false;
        }
        profileNamesLower.insert(lowerName);

        const QString sanitized = BitfieldDecoder::sanitizeColumnLabel(profile.profileName).toLower();
        if (sanitizedNames.contains(sanitized))
        {
            errorMessage = QString("Profile name '%1' conflicts with another profile name after sanitization.").arg(profile.profileName);
            return false;
        }
        sanitizedNames.insert(sanitized);

        if (controllerValues.contains(profile.controllerValue))
        {
            errorMessage = QString("Duplicate controller value 0x%1 in profile '%2'.")
                               .arg(QString::number(profile.controllerValue, 16).toUpper())
                               .arg(profile.profileName);
            return false;
        }
        controllerValues.insert(profile.controllerValue);

        QString ruleError;
        if (!BitfieldDecoder::validateRules(profile.bitDecodeRules, dependentFieldLengthBytes, ruleError))
        {
            errorMessage = QString("Profile '%1': %2").arg(profile.profileName).arg(ruleError);
            return false;
        }
    }

    return true;
}

QStringList ConditionalBitfieldDecoder::columnHeaders(const QString& dependentFieldName,
                                                      const ConditionalBitfieldDecoderConfig& decoder)
{
    QStringList headers;
    const QString sanitizedDepName = BitfieldDecoder::sanitizeColumnLabel(dependentFieldName);

    headers << sanitizedDepName + "_Profile";

    for (int i = 0; i < decoder.profiles.size(); ++i)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(i);
        const QString sanitizedProfile = BitfieldDecoder::sanitizeColumnLabel(profile.profileName);

        for (int r = 0; r < profile.bitDecodeRules.size(); ++r)
        {
            const BitDecodeRule& rule = profile.bitDecodeRules.at(r);
            if (!rule.enabled) continue;
            headers << sanitizedDepName + "_" + sanitizedProfile + "_" + BitfieldDecoder::sanitizeColumnLabel(rule.label);
        }
    }

    return headers;
}

QStringList ConditionalBitfieldDecoder::decode(const QByteArray& dependentFieldBytes,
                                               quint64 controllerRawValue,
                                               bool controllerFound,
                                               const ConditionalBitfieldDecoderConfig& decoder)
{
    QStringList result;

    int matchedProfileIndex = -1;
    if (controllerFound)
    {
        for (int i = 0; i < decoder.profiles.size(); ++i)
        {
            if (decoder.profiles.at(i).controllerValue == controllerRawValue)
            {
                matchedProfileIndex = i;
                break;
            }
        }
    }

    // Profile name column
    if (matchedProfileIndex < 0)
    {
        if (normalizedUnknownBehavior(decoder.unknownBehavior) == "BLANK")
            result << QString();
        else
            result << QString("UNKNOWN_CONTROLLER(0x%1)").arg(QString::number(controllerRawValue, 16).toUpper());
    }
    else
    {
        result << decoder.profiles.at(matchedProfileIndex).profileName;
    }

    // Rule columns — same iteration order as columnHeaders()
    for (int i = 0; i < decoder.profiles.size(); ++i)
    {
        const ConditionalBitDecodeProfile& profile = decoder.profiles.at(i);
        for (int r = 0; r < profile.bitDecodeRules.size(); ++r)
        {
            const BitDecodeRule& rule = profile.bitDecodeRules.at(r);
            if (!rule.enabled) continue;

            if (i == matchedProfileIndex && !dependentFieldBytes.isEmpty())
                result << BitfieldDecoder::decodeRule(dependentFieldBytes, rule);
            else
                result << QString();
        }
    }

    return result;
}
