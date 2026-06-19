#include "MessageJsonCodec.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace
{
const int kSchemaVersion = 1;
const QString kKind = QStringLiteral("UniversalDataSuiteMessages");

QString dataTypeToString(FieldDataType t)
{
    switch (t)
    {
    case FieldDataType::RawUnsignedBE: return "RawUnsignedBE";
    case FieldDataType::Bool:          return "Bool";
    case FieldDataType::Uint8:         return "Uint8";
    case FieldDataType::Int8:          return "Int8";
    case FieldDataType::Uint16:        return "Uint16";
    case FieldDataType::Int16:         return "Int16";
    case FieldDataType::Uint32:        return "Uint32";
    case FieldDataType::Int32:         return "Int32";
    case FieldDataType::Uint64:        return "Uint64";
    case FieldDataType::Int64:         return "Int64";
    case FieldDataType::Float32:       return "Float32";
    case FieldDataType::Float64:       return "Float64";
    case FieldDataType::String:        return "String";
    }
    return "RawUnsignedBE";
}

FieldDataType dataTypeFromString(const QString& s)
{
    if (s == "Bool")    return FieldDataType::Bool;
    if (s == "Uint8")   return FieldDataType::Uint8;
    if (s == "Int8")    return FieldDataType::Int8;
    if (s == "Uint16")  return FieldDataType::Uint16;
    if (s == "Int16")   return FieldDataType::Int16;
    if (s == "Uint32")  return FieldDataType::Uint32;
    if (s == "Int32")   return FieldDataType::Int32;
    if (s == "Uint64")  return FieldDataType::Uint64;
    if (s == "Int64")   return FieldDataType::Int64;
    if (s == "Float32") return FieldDataType::Float32;
    if (s == "Float64") return FieldDataType::Float64;
    if (s == "String")  return FieldDataType::String;
    return FieldDataType::RawUnsignedBE;
}

QString endiannessToString(FieldEndianness e)
{
    return e == FieldEndianness::Little ? QStringLiteral("Little") : QStringLiteral("Big");
}

FieldEndianness endiannessFromString(const QString& s)
{
    return s == QLatin1String("Little") ? FieldEndianness::Little : FieldEndianness::Big;
}

// quint64 is stored as a decimal STRING so values above 2^53 survive (JSON
// numbers are doubles). Reading tolerates either a string or a number.
QJsonValue u64ToJson(quint64 v) { return QString::number(v); }
quint64 u64FromJson(const QJsonValue& v, quint64 def)
{
    if (v.isString())
    {
        bool ok = false;
        const quint64 r = v.toString().toULongLong(&ok);
        return ok ? r : def;
    }
    if (v.isDouble())
        return static_cast<quint64>(v.toDouble());
    return def;
}

QJsonArray intListToJson(const QList<int>& xs)
{
    QJsonArray a;
    for (int i = 0; i < xs.size(); ++i)
        a.append(xs.at(i));
    return a;
}

QList<int> intListFromJson(const QJsonArray& a)
{
    QList<int> xs;
    for (int i = 0; i < a.size(); ++i)
        xs.append(a.at(i).toInt());
    return xs;
}

QJsonObject bitRuleToJson(const BitDecodeRule& r)
{
    QJsonObject o;
    o.insert("label", r.label);
    o.insert("bitPositions", intListToJson(r.bitPositions));
    QJsonObject vm;
    for (QMap<quint64, QString>::const_iterator it = r.valueMeanings.constBegin();
         it != r.valueMeanings.constEnd(); ++it)
        vm.insert(QString::number(it.key()), it.value());
    o.insert("valueMeanings", vm);
    o.insert("reserved", r.reserved);
    o.insert("unknownBehavior", r.unknownBehavior);
    o.insert("enabled", r.enabled);
    return o;
}

BitDecodeRule bitRuleFromJson(const QJsonObject& o)
{
    BitDecodeRule r;
    r.label = o.value("label").toString();
    r.bitPositions = intListFromJson(o.value("bitPositions").toArray());
    const QJsonObject vm = o.value("valueMeanings").toObject();
    for (QJsonObject::const_iterator it = vm.constBegin(); it != vm.constEnd(); ++it)
    {
        bool ok = false;
        const quint64 key = it.key().toULongLong(&ok);
        if (ok)
            r.valueMeanings.insert(key, it.value().toString());
    }
    r.reserved = o.value("reserved").toBool(false);
    r.unknownBehavior = o.value("unknownBehavior").toString("UNKNOWN");
    r.enabled = o.value("enabled").toBool(true);
    return r;
}

QJsonArray bitRulesToJson(const QList<BitDecodeRule>& rules)
{
    QJsonArray a;
    for (int i = 0; i < rules.size(); ++i)
        a.append(bitRuleToJson(rules.at(i)));
    return a;
}

QList<BitDecodeRule> bitRulesFromJson(const QJsonArray& a)
{
    QList<BitDecodeRule> rules;
    for (int i = 0; i < a.size(); ++i)
        rules.append(bitRuleFromJson(a.at(i).toObject()));
    return rules;
}

QJsonObject exclusionToJson(const ConditionalBitExclusionRule& e)
{
    QJsonObject o;
    o.insert("mutuallyExclusiveBits", intListToJson(e.mutuallyExclusiveBits));
    o.insert("validationLabel", e.validationLabel);
    o.insert("invalidMessage", e.invalidMessage);
    return o;
}

ConditionalBitExclusionRule exclusionFromJson(const QJsonObject& o)
{
    ConditionalBitExclusionRule e;
    e.mutuallyExclusiveBits = intListFromJson(o.value("mutuallyExclusiveBits").toArray());
    e.validationLabel = o.value("validationLabel").toString();
    e.invalidMessage = o.value("invalidMessage").toString();
    return e;
}

QJsonObject profileToJson(const ConditionalBitDecodeProfile& p)
{
    QJsonObject o;
    o.insert("profileName", p.profileName);
    o.insert("controllerValue", u64ToJson(p.controllerValue));
    o.insert("bitDecodeRules", bitRulesToJson(p.bitDecodeRules));
    QJsonArray ex;
    for (int i = 0; i < p.exclusionRules.size(); ++i)
        ex.append(exclusionToJson(p.exclusionRules.at(i)));
    o.insert("exclusionRules", ex);
    return o;
}

ConditionalBitDecodeProfile profileFromJson(const QJsonObject& o)
{
    ConditionalBitDecodeProfile p;
    p.profileName = o.value("profileName").toString();
    p.controllerValue = u64FromJson(o.value("controllerValue"), 0);
    p.bitDecodeRules = bitRulesFromJson(o.value("bitDecodeRules").toArray());
    const QJsonArray ex = o.value("exclusionRules").toArray();
    for (int i = 0; i < ex.size(); ++i)
        p.exclusionRules.append(exclusionFromJson(ex.at(i).toObject()));
    return p;
}

QJsonObject conditionalToJson(const ConditionalBitfieldDecoderConfig& c)
{
    QJsonObject o;
    o.insert("controllerFieldName", c.controllerFieldName);
    o.insert("unknownBehavior", c.unknownBehavior);
    QJsonArray profiles;
    for (int i = 0; i < c.profiles.size(); ++i)
        profiles.append(profileToJson(c.profiles.at(i)));
    o.insert("profiles", profiles);
    return o;
}

ConditionalBitfieldDecoderConfig conditionalFromJson(const QJsonObject& o)
{
    ConditionalBitfieldDecoderConfig c;
    c.controllerFieldName = o.value("controllerFieldName").toString();
    c.unknownBehavior = o.value("unknownBehavior").toString("UNKNOWN_CONTROLLER");
    const QJsonArray profiles = o.value("profiles").toArray();
    for (int i = 0; i < profiles.size(); ++i)
        c.profiles.append(profileFromJson(profiles.at(i).toObject()));
    return c;
}

// Mirrors ProjectFile's compare-options layout (same keys) so the section reads
// the same either way; expectedMessageId is stored as a string for exactness.
QJsonObject compareToJson(const CompareOptionsConfig& c)
{
    QJsonObject o;
    o.insert("checkHeader", c.checkHeader);
    o.insert("headerByteOffset", c.headerByteOffset);
    o.insert("headerLength", c.headerLength);
    o.insert("expectedHeaderHex", QString::fromLatin1(c.expectedHeader.toHex()));
    o.insert("headerInputMode", c.headerInputMode);
    o.insert("expectedHeaderText", c.expectedHeaderText);

    o.insert("checkTerminator", c.checkTerminator);
    o.insert("terminatorByteOffset", c.terminatorByteOffset);
    o.insert("terminatorLength", c.terminatorLength);
    o.insert("expectedTerminatorHex", QString::fromLatin1(c.expectedTerminator.toHex()));
    o.insert("terminatorInputMode", c.terminatorInputMode);
    o.insert("expectedTerminatorText", c.expectedTerminatorText);

    o.insert("checkChecksum", c.checkChecksum);
    o.insert("checksumAlgorithm", c.checksumAlgorithm);
    o.insert("checksumRangeStart", c.checksumRangeStart);
    o.insert("checksumRangeEnd", c.checksumRangeEnd);
    o.insert("checksumByteOffset", c.checksumByteOffset);
    o.insert("checksumLength", c.checksumLength);

    o.insert("checkRefreshRate", c.checkRefreshRate);
    o.insert("expectedRefreshRateHz", c.expectedRefreshRateHz);
    o.insert("refreshRateToleranceHz", c.refreshRateToleranceHz);

    o.insert("checkEndianness", c.checkEndianness);
    o.insert("expectedEndianness", c.expectedEndianness);

    o.insert("checkDataLength", c.checkDataLength);
    o.insert("dataLengthByteOffset", c.dataLengthByteOffset);
    o.insert("dataLengthSize", c.dataLengthSize);
    o.insert("dataLengthAdjust", c.dataLengthAdjust);

    o.insert("checkMessageId", c.checkMessageId);
    o.insert("messageIdByteOffset", c.messageIdByteOffset);
    o.insert("messageIdSize", c.messageIdSize);
    o.insert("expectedMessageId", u64ToJson(c.expectedMessageId));
    return o;
}

CompareOptionsConfig compareFromJson(const QJsonObject& o)
{
    CompareOptionsConfig c;
    c.checkHeader = o.value("checkHeader").toBool(false);
    c.headerByteOffset = o.value("headerByteOffset").toInt(0);
    c.headerLength = o.value("headerLength").toInt(0);
    c.expectedHeader = QByteArray::fromHex(o.value("expectedHeaderHex").toString().toLatin1());
    c.headerInputMode = o.value("headerInputMode").toString("HEX");
    c.expectedHeaderText = o.value("expectedHeaderText").toString();

    c.checkTerminator = o.value("checkTerminator").toBool(false);
    c.terminatorByteOffset = o.value("terminatorByteOffset").toInt(-1);
    c.terminatorLength = o.value("terminatorLength").toInt(0);
    c.expectedTerminator = QByteArray::fromHex(o.value("expectedTerminatorHex").toString().toLatin1());
    c.terminatorInputMode = o.value("terminatorInputMode").toString("HEX");
    c.expectedTerminatorText = o.value("expectedTerminatorText").toString();

    c.checkChecksum = o.value("checkChecksum").toBool(false);
    c.checksumAlgorithm = o.value("checksumAlgorithm").toString("XOR");
    c.checksumRangeStart = o.value("checksumRangeStart").toInt(0);
    c.checksumRangeEnd = o.value("checksumRangeEnd").toInt(0);
    c.checksumByteOffset = o.value("checksumByteOffset").toInt(0);
    c.checksumLength = o.value("checksumLength").toInt(1);

    c.checkRefreshRate = o.value("checkRefreshRate").toBool(false);
    c.expectedRefreshRateHz = o.value("expectedRefreshRateHz").toDouble(0.0);
    c.refreshRateToleranceHz = o.value("refreshRateToleranceHz").toDouble(1.0);

    c.checkEndianness = o.value("checkEndianness").toBool(false);
    c.expectedEndianness = o.value("expectedEndianness").toString("BIG");

    c.checkDataLength = o.value("checkDataLength").toBool(false);
    c.dataLengthByteOffset = o.value("dataLengthByteOffset").toInt(0);
    c.dataLengthSize = o.value("dataLengthSize").toInt(2);
    c.dataLengthAdjust = o.value("dataLengthAdjust").toInt(0);

    c.checkMessageId = o.value("checkMessageId").toBool(false);
    c.messageIdByteOffset = o.value("messageIdByteOffset").toInt(0);
    c.messageIdSize = o.value("messageIdSize").toInt(2);
    c.expectedMessageId = u64FromJson(o.value("expectedMessageId"), 0);
    return c;
}

QJsonObject fieldToJson(const FieldDefinition& f)
{
    QJsonObject o;
    o.insert("name", f.name);
    o.insert("byteOffset", f.byteOffset);
    o.insert("byteOffsetCorrect", f.byteOffsetcorrect);
    o.insert("length", f.length);
    o.insert("dataType", dataTypeToString(f.dataType));
    o.insert("resolution", f.resolution);
    o.insert("resolutionExpression", f.resolutionExpression);

    // Reader decode configs (preserved even when written by the simulator).
    o.insert("hasBitfieldDecoder", f.hasBitfieldDecoder);
    if (f.hasBitfieldDecoder && !f.bitDecodeRules.isEmpty())
        o.insert("bitDecodeRules", bitRulesToJson(f.bitDecodeRules));
    o.insert("hasConditionalBitfieldDecoder", f.hasConditionalBitfieldDecoder);
    if (f.hasConditionalBitfieldDecoder && !f.conditionalDecoder.profiles.isEmpty())
        o.insert("conditionalDecoder", conditionalToJson(f.conditionalDecoder));

    // NMEA.
    o.insert("nmeaFieldIndex", f.nmeaFieldIndex);
    o.insert("nmeaValueKind", f.nmeaValueKind);

    // Simulator send configs (preserved even when written by the reader).
    o.insert("sendValueText", f.sendValueText);
    o.insert("endianness", endiannessToString(f.endianness));
    return o;
}

FieldDefinition fieldFromJson(const QJsonObject& o)
{
    FieldDefinition f;
    f.name = o.value("name").toString();
    f.byteOffset = o.value("byteOffset").toInt(0);
    f.byteOffsetcorrect = o.value("byteOffsetCorrect").toInt(f.byteOffset - 1);
    f.length = o.value("length").toInt(1);
    f.dataType = dataTypeFromString(o.value("dataType").toString("RawUnsignedBE"));
    f.resolution = o.value("resolution").toDouble(1.0);
    f.resolutionExpression = o.value("resolutionExpression").toString("1");

    f.hasBitfieldDecoder = o.value("hasBitfieldDecoder").toBool(false);
    if (f.hasBitfieldDecoder)
    {
        f.bitDecodeRules = bitRulesFromJson(o.value("bitDecodeRules").toArray());
        f.hasBitfieldDecoder = !f.bitDecodeRules.isEmpty();
    }
    f.hasConditionalBitfieldDecoder = o.value("hasConditionalBitfieldDecoder").toBool(false);
    if (f.hasConditionalBitfieldDecoder)
    {
        f.conditionalDecoder = conditionalFromJson(o.value("conditionalDecoder").toObject());
        f.hasConditionalBitfieldDecoder = !f.conditionalDecoder.profiles.isEmpty();
    }

    f.nmeaFieldIndex = o.value("nmeaFieldIndex").toInt(0);
    f.nmeaValueKind = o.value("nmeaValueKind").toInt(0);

    f.sendValueText = o.value("sendValueText").toString();
    f.endianness = endiannessFromString(o.value("endianness").toString("Big"));
    return f;
}

QJsonObject messageToJsonObj(const MessageDefinition& m)
{
    QJsonObject o;
    o.insert("messageName", m.messageName);
    o.insert("port", static_cast<int>(m.port));
    o.insert("payloadLengthBytes", m.payloadLengthBytes);
    o.insert("optionalHeaderHex", QString::fromLatin1(m.optionalHeader.toHex()));

    QJsonArray fields;
    for (int i = 0; i < m.fields.size(); ++i)
        fields.append(fieldToJson(m.fields.at(i)));
    o.insert("fields", fields);

    // Reader compare options.
    o.insert("hasCompareOptions", m.hasCompareOptions);
    o.insert("compareOptions", compareToJson(m.compareOptions));

    // NMEA / format.
    o.insert("dataFormat", m.dataFormat);
    o.insert("nmeaSentenceType", m.nmeaSentenceType);

    // Simulator send settings.
    o.insert("sendFrequencyHz", m.sendFrequencyHz);
    o.insert("sendEnabled", m.sendEnabled);
    o.insert("nmeaTalker", m.nmeaTalker);
    return o;
}

MessageDefinition messageFromJsonObj(const QJsonObject& o)
{
    MessageDefinition m;
    m.messageName = o.value("messageName").toString();
    m.port = static_cast<quint16>(o.value("port").toInt(0));
    m.payloadLengthBytes = o.value("payloadLengthBytes").toInt(0);
    m.optionalHeader = QByteArray::fromHex(o.value("optionalHeaderHex").toString().toLatin1());

    const QJsonArray fields = o.value("fields").toArray();
    for (int i = 0; i < fields.size(); ++i)
        m.fields.append(fieldFromJson(fields.at(i).toObject()));

    m.hasCompareOptions = o.value("hasCompareOptions").toBool(false);
    m.compareOptions = compareFromJson(o.value("compareOptions").toObject());

    m.dataFormat = o.value("dataFormat").toString("HEX");
    m.nmeaSentenceType = o.value("nmeaSentenceType").toString();

    m.sendFrequencyHz = o.value("sendFrequencyHz").toDouble(1.0);
    m.sendEnabled = o.value("sendEnabled").toBool(true);
    m.nmeaTalker = o.value("nmeaTalker").toString("GP");
    return m;
}

QJsonObject makeRoot(const QString& contentKey, const QJsonValue& content)
{
    QJsonObject root;
    root.insert("version", kSchemaVersion);
    root.insert("kind", kKind);
    root.insert("exportedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(contentKey, content);
    return root;
}

// Parse a top-level object, tolerant of either a "messages" array or a single
// message object. Pulls every message out. Returns false with a reason on a
// structural problem.
bool parseMessages(const QString& jsonText, QList<MessageDefinition>& out, QString& errorMessage)
{
    out.clear();
    errorMessage.clear();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        errorMessage = QString("Invalid JSON at offset %1: %2. "
                               "Solution: open a file exported by this suite (File → Export … to JSON).")
                           .arg(pe.offset).arg(pe.errorString());
        return false;
    }

    QJsonArray msgs;
    if (doc.isObject())
    {
        const QJsonObject root = doc.object();
        if (root.value("messages").isArray())
            msgs = root.value("messages").toArray();
        else if (root.contains("fields") || root.contains("messageName"))
            msgs.append(root); // a single message object at the top level
        else
        {
            errorMessage = "JSON has no 'messages' array or message object. "
                           "Solution: choose a file exported by this suite.";
            return false;
        }
    }
    else if (doc.isArray())
    {
        msgs = doc.array(); // a bare array of message objects
    }
    else
    {
        errorMessage = "Top-level JSON must be an object or an array. "
                       "Solution: choose a file exported by this suite.";
        return false;
    }

    for (int i = 0; i < msgs.size(); ++i)
    {
        if (!msgs.at(i).isObject())
        {
            errorMessage = QString("Message %1 is not a JSON object.").arg(i + 1);
            return false;
        }
        out.append(messageFromJsonObj(msgs.at(i).toObject()));
    }
    return true;
}
} // namespace

QString MessageJsonCodec::messagesToJson(const QList<MessageDefinition>& messages)
{
    QJsonArray arr;
    for (int i = 0; i < messages.size(); ++i)
        arr.append(messageToJsonObj(messages.at(i)));
    return QString::fromUtf8(QJsonDocument(makeRoot("messages", arr)).toJson(QJsonDocument::Indented));
}

bool MessageJsonCodec::messagesFromJson(const QString& jsonText,
                                        QList<MessageDefinition>& out,
                                        QString& errorMessage)
{
    return parseMessages(jsonText, out, errorMessage);
}

QString MessageJsonCodec::messageToJson(const MessageDefinition& message)
{
    QList<MessageDefinition> one;
    one.append(message);
    return messagesToJson(one);
}

bool MessageJsonCodec::messageFromJson(const QString& jsonText,
                                       MessageDefinition& out,
                                       QString& errorMessage)
{
    QList<MessageDefinition> list;
    if (!parseMessages(jsonText, list, errorMessage))
        return false;
    if (list.isEmpty())
    {
        errorMessage = "The file contained no message. "
                       "Solution: export a message to JSON first, then import it.";
        return false;
    }
    out = list.first();
    return true;
}

QString MessageJsonCodec::fieldsToJson(const QList<FieldDefinition>& fields)
{
    QJsonArray arr;
    for (int i = 0; i < fields.size(); ++i)
        arr.append(fieldToJson(fields.at(i)));
    return QString::fromUtf8(QJsonDocument(makeRoot("fields", arr)).toJson(QJsonDocument::Indented));
}

bool MessageJsonCodec::fieldsFromJson(const QString& jsonText,
                                      QList<FieldDefinition>& out,
                                      QString& errorMessage)
{
    out.clear();
    errorMessage.clear();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        errorMessage = QString("Invalid JSON at offset %1: %2. "
                               "Solution: open a fields/message file exported by this suite.")
                           .arg(pe.offset).arg(pe.errorString());
        return false;
    }

    // Accept: { fields:[...] }, a whole message object, { messages:[...] }, or a
    // bare array of field objects.
    QJsonArray fieldArray;
    if (doc.isObject())
    {
        const QJsonObject root = doc.object();
        if (root.value("fields").isArray())
            fieldArray = root.value("fields").toArray();
        else if (root.value("messages").isArray())
        {
            const QJsonArray msgs = root.value("messages").toArray();
            if (!msgs.isEmpty() && msgs.first().isObject())
                fieldArray = msgs.first().toObject().value("fields").toArray();
        }
    }
    else if (doc.isArray())
    {
        fieldArray = doc.array();
    }

    if (fieldArray.isEmpty())
    {
        errorMessage = "No 'fields' array found in the file. "
                       "Solution: choose a fields or message file exported by this suite.";
        return false;
    }

    for (int i = 0; i < fieldArray.size(); ++i)
    {
        if (!fieldArray.at(i).isObject())
        {
            errorMessage = QString("Item %1 in 'fields' is not an object.").arg(i + 1);
            return false;
        }
        const FieldDefinition f = fieldFromJson(fieldArray.at(i).toObject());
        if (f.name.trimmed().isEmpty())
        {
            errorMessage = QString("Field %1 has an empty 'name'.").arg(i + 1);
            return false;
        }
        out.append(f);
    }
    return true;
}
