#include "ProjectFile.h"

#include "BitfieldDecoder.h"
#include "ConditionalBitfieldDecoder.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStandardPaths>

namespace
{
QString dataTypeToJsonString(FieldDataType t)
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

FieldDataType dataTypeFromJsonString(const QString& s)
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

QJsonObject fieldToJson(const FieldDefinition& f)
{
    QJsonObject o;
    o.insert("name", f.name);
    o.insert("byteOffset", f.byteOffset);
    o.insert("byteOffsetCorrect", f.byteOffsetcorrect);
    o.insert("length", f.length);
    o.insert("dataType", dataTypeToJsonString(f.dataType));
    o.insert("resolution", f.resolution);
    o.insert("resolutionExpression", f.resolutionExpression);
    o.insert("hasBitfieldDecoder", f.hasBitfieldDecoder);
    if (f.hasBitfieldDecoder)
        o.insert("bitfieldDecoder", BitfieldDecoder::rulesToJson(f.bitDecodeRules));
    o.insert("hasConditionalBitfieldDecoder", f.hasConditionalBitfieldDecoder);
    if (f.hasConditionalBitfieldDecoder)
        o.insert("conditionalDecoder", ConditionalBitfieldDecoder::toJson(f.conditionalDecoder));
    // NMEA: 1-based comma position (0 for Hex fields) + value kind (custom sentences).
    o.insert("nmeaFieldIndex", f.nmeaFieldIndex);
    o.insert("nmeaValueKind", f.nmeaValueKind);
    return o;
}

FieldDefinition fieldFromJson(const QJsonObject& o)
{
    FieldDefinition f;
    f.name = o.value("name").toString();
    f.byteOffset = o.value("byteOffset").toInt(0);
    f.byteOffsetcorrect = o.value("byteOffsetCorrect").toInt(f.byteOffset - 1);
    f.length = o.value("length").toInt(1);
    f.dataType = dataTypeFromJsonString(o.value("dataType").toString("RawUnsignedBE"));
    f.resolution = o.value("resolution").toDouble(1.0);
    f.resolutionExpression = o.value("resolutionExpression").toString("1");
    f.hasBitfieldDecoder = o.value("hasBitfieldDecoder").toBool(false);
    if (f.hasBitfieldDecoder)
    {
        const QString bj = o.value("bitfieldDecoder").toString();
        QString err;
        BitfieldDecoder::rulesFromJson(bj, f.length, f.bitDecodeRules, err);
        f.hasBitfieldDecoder = !f.bitDecodeRules.isEmpty();
    }
    f.hasConditionalBitfieldDecoder = o.value("hasConditionalBitfieldDecoder").toBool(false);
    if (f.hasConditionalBitfieldDecoder)
    {
        const QString cj = o.value("conditionalDecoder").toString();
        QString err;
        ConditionalBitfieldDecoder::fromJson(cj, f.conditionalDecoder, err);
        f.hasConditionalBitfieldDecoder = !f.conditionalDecoder.profiles.isEmpty();
    }
    // NMEA: 1-based comma position (0 for Hex fields) + value kind (custom sentences).
    f.nmeaFieldIndex = o.value("nmeaFieldIndex").toInt(0);
    f.nmeaValueKind = o.value("nmeaValueKind").toInt(0);
    return f;
}

QJsonArray fieldsToJson(const QList<FieldDefinition>& fields)
{
    QJsonArray arr;
    for (int i = 0; i < fields.size(); ++i)
        arr.append(fieldToJson(fields.at(i)));
    return arr;
}

QList<FieldDefinition> fieldsFromJson(const QJsonArray& arr)
{
    QList<FieldDefinition> out;
    for (int i = 0; i < arr.size(); ++i)
        out.append(fieldFromJson(arr.at(i).toObject()));
    return out;
}

QJsonObject filterToJson(const FilterConfiguration& fc)
{
    QJsonObject o;
    o.insert("mode", fc.mode);
    o.insert("commonPort", fc.commonPort);
    QJsonArray arr;
    for (int i = 0; i < fc.filters.size(); ++i)
    {
        const MessageFilter& mf = fc.filters.at(i);
        QJsonObject fo;
        fo.insert("label", mf.label);
        fo.insert("port", mf.port);
        fo.insert("headerHex", QString::fromLatin1(mf.header.toHex()));
        arr.append(fo);
    }
    o.insert("filters", arr);
    return o;
}

FilterConfiguration filterFromJson(const QJsonObject& o)
{
    FilterConfiguration fc;
    fc.mode = o.value("mode").toInt(FILTER_MODE_PORT);
    fc.commonPort = o.value("commonPort").toInt(0);
    const QJsonArray arr = o.value("filters").toArray();
    for (int i = 0; i < arr.size(); ++i)
    {
        const QJsonObject fo = arr.at(i).toObject();
        MessageFilter mf;
        mf.label = fo.value("label").toString();
        mf.port = fo.value("port").toInt(-1);
        mf.header = QByteArray::fromHex(fo.value("headerHex").toString().toLatin1());
        fc.filters.append(mf);
    }
    return fc;
}

// v13: compare options round-trip. Flat object; missing keys = defaults.
QJsonObject compareOptionsToJson(const CompareOptionsConfig& c)
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
    o.insert("expectedMessageId", static_cast<double>(c.expectedMessageId));

    return o;
}

CompareOptionsConfig compareOptionsFromJson(const QJsonObject& o)
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
    c.expectedMessageId = static_cast<quint64>(o.value("expectedMessageId").toDouble(0.0));

    return c;
}

QJsonObject messageToJson(const MessageDefinition& m)
{
    QJsonObject o;
    o.insert("messageName", m.messageName);
    o.insert("port", static_cast<int>(m.port));
    o.insert("payloadLengthBytes", m.payloadLengthBytes);
    // v12: optional header bytes for disambiguating same-length msgs on same port.
    o.insert("optionalHeaderHex", QString::fromLatin1(m.optionalHeader.toHex()));
    o.insert("fields", fieldsToJson(m.fields));
    // v13: per-message compare options
    o.insert("hasCompareOptions", m.hasCompareOptions);
    o.insert("compareOptions", compareOptionsToJson(m.compareOptions));
    // NMEA: data format + sentence formatter.
    o.insert("dataFormat", m.dataFormat);
    o.insert("nmeaSentenceType", m.nmeaSentenceType);
    return o;
}

MessageDefinition messageFromJson(const QJsonObject& o)
{
    MessageDefinition m;
    m.messageName = o.value("messageName").toString();
    m.port = static_cast<quint16>(o.value("port").toInt(0));
    m.payloadLengthBytes = o.value("payloadLengthBytes").toInt(0);
    m.optionalHeader = QByteArray::fromHex(o.value("optionalHeaderHex").toString().toLatin1());
    m.fields = fieldsFromJson(o.value("fields").toArray());
    // v13
    m.hasCompareOptions = o.value("hasCompareOptions").toBool(false);
    m.compareOptions = compareOptionsFromJson(o.value("compareOptions").toObject());
    // NMEA: data format + sentence formatter (defaults preserve pre-NMEA files).
    m.dataFormat = o.value("dataFormat").toString("HEX");
    m.nmeaSentenceType = o.value("nmeaSentenceType").toString();
    return m;
}
}

bool ProjectFile::save(const ProjectState& state, const QString& path, QString& errorMessage)
{
    errorMessage.clear();
    if (path.isEmpty())
    {
        errorMessage = "Project save path is empty.";
        return false;
    }

    QJsonObject root;
    root.insert("version", 1);
    root.insert("appVersion", "v8");
    root.insert("savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("pcapPath", state.pcapPath);
    root.insert("inputMode", state.inputMode);
    root.insert("filterMode", state.filterMode);
    root.insert("filterCount", state.filterCount);
    root.insert("filterConfig", filterToJson(state.filterConfig));

    QJsonArray portsArray;
    for (int r = 0; r < state.portMessagesByRow.size(); ++r)
    {
        QJsonObject row;
        row.insert("filterRow", r);
        QJsonArray msgs;
        const QList<MessageDefinition>& list = state.portMessagesByRow.at(r);
        for (int i = 0; i < list.size(); ++i)
            msgs.append(messageToJson(list.at(i)));
        row.insert("messages", msgs);
        portsArray.append(row);
    }
    root.insert("portMessages", portsArray);

    root.insert("headerFields", fieldsToJson(state.headerFields));

    // v12: per-header-row length filters (matches portMessages shape).
    QJsonArray headerMsgsArray;
    for (int r = 0; r < state.headerMessagesByRow.size(); ++r)
    {
        QJsonObject row;
        row.insert("filterRow", r);
        QJsonArray msgs;
        const QList<MessageDefinition>& list = state.headerMessagesByRow.at(r);
        for (int i = 0; i < list.size(); ++i)
            msgs.append(messageToJson(list.at(i)));
        row.insert("messages", msgs);
        headerMsgsArray.append(row);
    }
    root.insert("headerMessages", headerMsgsArray);

    QJsonObject live;
    live.insert("fields", fieldsToJson(state.liveFields));
    live.insert("filterConfig", filterToJson(state.liveFilterConfig));
    // v12: live-mode length filters
    QJsonArray liveMsgs;
    for (int i = 0; i < state.liveMessages.size(); ++i)
        liveMsgs.append(messageToJson(state.liveMessages.at(i)));
    live.insert("messages", liveMsgs);
    root.insert("live", live);

    const QJsonDocument doc(root);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);

    const QString tmpPath = path + ".tmp";
    QFile tmp(tmpPath);
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        errorMessage = QString("Cannot open '%1' for writing: %2").arg(tmpPath).arg(tmp.errorString());
        return false;
    }
    const qint64 written = tmp.write(bytes);
    tmp.close();
    if (written != bytes.size())
    {
        errorMessage = QString("Write incomplete for '%1' (%2/%3 bytes).")
                           .arg(tmpPath).arg(written).arg(bytes.size());
        QFile::remove(tmpPath);
        return false;
    }

    if (QFile::exists(path))
    {
        const QString bakPath = path + ".bak";
        QFile::remove(bakPath);
        QFile::rename(path, bakPath);
    }
    if (!QFile::rename(tmpPath, path))
    {
        errorMessage = QString("Cannot rename temp file to '%1'.").arg(path);
        QFile::remove(tmpPath);
        return false;
    }
    return true;
}

bool ProjectFile::load(const QString& path, ProjectState& state, QString& errorMessage)
{
    errorMessage.clear();
    state = ProjectState();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage = QString("Cannot open '%1' for reading: %2").arg(path).arg(file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        errorMessage = QString("Invalid project file: %1").arg(pe.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    const int version = root.value("version").toInt(0);
    if (version != 1)
    {
        errorMessage = QString("Unsupported project file version %1.").arg(version);
        return false;
    }

    state.appSchemaVersion = version;
    state.savedAtIso = root.value("savedAt").toString();
    state.pcapPath = root.value("pcapPath").toString();
    state.inputMode = root.value("inputMode").toString("file");
    state.filterMode = root.value("filterMode").toString("port");
    state.filterCount = root.value("filterCount").toInt(1);
    state.filterConfig = filterFromJson(root.value("filterConfig").toObject());

    const QJsonArray portsArray = root.value("portMessages").toArray();
    for (int r = 0; r < portsArray.size(); ++r)
    {
        const QJsonObject row = portsArray.at(r).toObject();
        const QJsonArray msgs = row.value("messages").toArray();
        QList<MessageDefinition> list;
        for (int i = 0; i < msgs.size(); ++i)
            list.append(messageFromJson(msgs.at(i).toObject()));
        state.portMessagesByRow.append(list);
    }

    state.headerFields = fieldsFromJson(root.value("headerFields").toArray());

    const QJsonObject live = root.value("live").toObject();
    state.liveFields = fieldsFromJson(live.value("fields").toArray());
    state.liveFilterConfig = filterFromJson(live.value("filterConfig").toObject());

    // v12: optional fields — absent in pre-v12 project files (load returns empties).
    const QJsonArray headerMsgsArray = root.value("headerMessages").toArray();
    for (int r = 0; r < headerMsgsArray.size(); ++r)
    {
        const QJsonObject row = headerMsgsArray.at(r).toObject();
        const QJsonArray msgs = row.value("messages").toArray();
        QList<MessageDefinition> list;
        for (int i = 0; i < msgs.size(); ++i)
            list.append(messageFromJson(msgs.at(i).toObject()));
        state.headerMessagesByRow.append(list);
    }

    const QJsonArray liveMsgsArray = live.value("messages").toArray();
    for (int i = 0; i < liveMsgsArray.size(); ++i)
        state.liveMessages.append(messageFromJson(liveMsgsArray.at(i).toObject()));

    return true;
}

QString ProjectFile::sidecarPathFor(const QString& pcapPath)
{
    if (pcapPath.trimmed().isEmpty()) return QString();

    QFileInfo info(pcapPath);
    const QString preferred = info.absoluteDir().absoluteFilePath(info.completeBaseName() + ".pcproj.json");

    QDir dir = info.absoluteDir();
    QFile tmp(dir.absoluteFilePath(".pcproj_write_test.tmp"));
    const bool writable = tmp.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (writable)
    {
        tmp.close();
        tmp.remove();
        return preferred;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    const QByteArray hash = QCryptographicHash::hash(info.absoluteFilePath().toUtf8(),
                                                     QCryptographicHash::Md5).toHex();
    return QDir(appData).absoluteFilePath(
        QString("%1_%2.pcproj.json").arg(info.completeBaseName(), QString::fromLatin1(hash.left(8))));
}

bool ProjectFile::exists(const QString& path)
{
    return !path.isEmpty() && QFile::exists(path);
}

namespace
{
// Convert a JSON string produced by BitfieldDecoder::rulesToJson / ConditionalBitfieldDecoder::toJson
// into a nested QJsonValue so it appears as a structured object in the exported file
// (instead of an escaped string). Falls back to QJsonValue::Null on parse failure.
QJsonValue jsonStringToValue(const QString& jsonText)
{
    if (jsonText.trimmed().isEmpty()) return QJsonValue();
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError) return QJsonValue();
    if (doc.isObject()) return doc.object();
    if (doc.isArray())  return doc.array();
    return QJsonValue();
}

// Inverse of jsonStringToValue — flatten a nested object back into a compact JSON string
// so it can be fed to BitfieldDecoder::rulesFromJson / ConditionalBitfieldDecoder::fromJson.
// Returns empty string if the value is null / not an object.
QString jsonValueToString(const QJsonValue& v)
{
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isString())
        return v.toString();
    return QString();
}
}

QString ProjectFile::fieldListToJson(const QList<FieldDefinition>& fields)
{
    QJsonObject root;
    root.insert("version", 1);
    root.insert("kind", "PcapUdpExtractorFieldList");
    root.insert("exportedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonArray arr;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& f = fields.at(i);
        QJsonObject fo;
        fo.insert("name", f.name);
        fo.insert("byteOffset", f.byteOffset);
        fo.insert("byteOffsetCorrect", f.byteOffsetcorrect);
        fo.insert("length", f.length);
        fo.insert("dataType", dataTypeToJsonString(f.dataType));
        fo.insert("resolution", f.resolution);
        fo.insert("resolutionExpression", f.resolutionExpression);

        if (f.hasBitfieldDecoder && !f.bitDecodeRules.isEmpty())
            fo.insert("bitfieldDecoder",
                      jsonStringToValue(BitfieldDecoder::rulesToJson(f.bitDecodeRules)));
        else
            fo.insert("bitfieldDecoder", QJsonValue());

        if (f.hasConditionalBitfieldDecoder && !f.conditionalDecoder.profiles.isEmpty())
            fo.insert("conditionalDecoder",
                      jsonStringToValue(ConditionalBitfieldDecoder::toJson(f.conditionalDecoder)));
        else
            fo.insert("conditionalDecoder", QJsonValue());

        arr.append(fo);
    }
    root.insert("fields", arr);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool ProjectFile::fieldListFromJson(const QString& jsonText,
                                    QList<FieldDefinition>& fields,
                                    QString& errorMessage)
{
    fields.clear();
    errorMessage.clear();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        errorMessage = QString("Invalid JSON at offset %1: %2").arg(pe.offset).arg(pe.errorString());
        return false;
    }
    if (!doc.isObject())
    {
        errorMessage = "Top-level JSON value must be an object with a 'fields' array.";
        return false;
    }

    const QJsonObject root = doc.object();
    if (!root.contains("fields") || !root.value("fields").isArray())
    {
        errorMessage = "Missing 'fields' array at the top level.";
        return false;
    }

    const QJsonArray arr = root.value("fields").toArray();
    QStringList decoderWarnings;

    for (int i = 0; i < arr.size(); ++i)
    {
        if (!arr.at(i).isObject())
        {
            errorMessage = QString("Item %1 in 'fields' is not an object.").arg(i + 1);
            return false;
        }
        const QJsonObject fo = arr.at(i).toObject();

        FieldDefinition f;
        f.name = fo.value("name").toString();
        if (f.name.trimmed().isEmpty())
        {
            errorMessage = QString("Field %1 has an empty 'name'.").arg(i + 1);
            return false;
        }
        f.byteOffset = fo.value("byteOffset").toInt(0);
        f.byteOffsetcorrect = fo.value("byteOffsetCorrect").toInt(f.byteOffset - 1);
        f.length = fo.value("length").toInt(1);
        f.dataType = dataTypeFromJsonString(fo.value("dataType").toString("RawUnsignedBE"));
        f.resolution = fo.value("resolution").toDouble(1.0);
        f.resolutionExpression = fo.value("resolutionExpression").toString("1");
        const QJsonValue bf = fo.value("bitfieldDecoder");
        if (!bf.isNull() && !bf.isUndefined())
        {
            const QString bfStr = jsonValueToString(bf);
            if (!bfStr.isEmpty())
            {
                QString decErr;
                if (BitfieldDecoder::rulesFromJson(bfStr, f.length, f.bitDecodeRules, decErr))
                    f.hasBitfieldDecoder = !f.bitDecodeRules.isEmpty();
                else
                    decoderWarnings << QString("Field '%1': bitfieldDecoder failed validation (%2). Field imported without bit rules.")
                                          .arg(f.name).arg(decErr);
            }
        }

        const QJsonValue cd = fo.value("conditionalDecoder");
        if (!cd.isNull() && !cd.isUndefined())
        {
            const QString cdStr = jsonValueToString(cd);
            if (!cdStr.isEmpty())
            {
                QString decErr;
                if (ConditionalBitfieldDecoder::fromJson(cdStr, f.conditionalDecoder, decErr))
                    f.hasConditionalBitfieldDecoder = !f.conditionalDecoder.profiles.isEmpty();
                else
                    decoderWarnings << QString("Field '%1': conditionalDecoder failed validation (%2). Field imported without conditional decoder.")
                                          .arg(f.name).arg(decErr);
            }
        }

        fields.append(f);
    }

    if (!decoderWarnings.isEmpty())
        errorMessage = decoderWarnings.join("\n");  // non-fatal — return true with warnings

    return true;
}
