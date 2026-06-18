#include "SimSetupFile.h"

#include "FieldCsvCodec.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace
{
// Enum spellings, matching the parser's project files so field lists move
// freely between the two apps ("Uint16", "Float32", ...).
QString dataTypeToJsonString(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::RawUnsignedBE: return "RawUnsignedBE";
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
    case FieldDataType::Bool:          return "Bool";
    case FieldDataType::String:        return "String";
    }
    return "RawUnsignedBE";
}

FieldDataType dataTypeFromJsonString(const QString& text)
{
    FieldDataType dataType = FieldDataType::RawUnsignedBE;
    if (FieldCsvCodec::dataTypeFromLabel(text, dataType))
        return dataType;
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
    o.insert("nmeaFieldIndex", f.nmeaFieldIndex);
    o.insert("nmeaValueKind", f.nmeaValueKind);
    o.insert("sendValue", f.sendValueText);
    o.insert("endianness", f.endianness == FieldEndianness::Little ? QString("LITTLE") : QString("BIG"));
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
    f.nmeaFieldIndex = o.value("nmeaFieldIndex").toInt(0);
    f.nmeaValueKind = o.value("nmeaValueKind").toInt(0);
    f.sendValueText = o.value("sendValue").toString();
    // Endianness is optional; absent (older setups / parser field lists) = Big.
    f.endianness = (o.value("endianness").toString("BIG").compare("LITTLE", Qt::CaseInsensitive) == 0)
                       ? FieldEndianness::Little
                       : FieldEndianness::Big;
    // Parser field lists carry decoder configs — deliberately ignored here;
    // the simulator transmits values, it never decodes.
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

QJsonObject messageToJson(const MessageDefinition& m)
{
    QJsonObject o;
    o.insert("messageName", m.messageName);
    o.insert("payloadLengthBytes", m.payloadLengthBytes);
    o.insert("dataFormat", m.dataFormat);
    o.insert("nmeaSentenceType", m.nmeaSentenceType);
    o.insert("nmeaTalker", m.nmeaTalker);
    o.insert("sendEnabled", m.sendEnabled);
    o.insert("sendFrequencyHz", m.sendFrequencyHz);
    o.insert("fields", fieldsToJson(m.fields));
    return o;
}

MessageDefinition messageFromJson(const QJsonObject& o)
{
    MessageDefinition m;
    m.messageName = o.value("messageName").toString();
    m.payloadLengthBytes = o.value("payloadLengthBytes").toInt(0);
    m.dataFormat = o.value("dataFormat").toString("HEX");
    m.nmeaSentenceType = o.value("nmeaSentenceType").toString();
    m.nmeaTalker = o.value("nmeaTalker").toString("GP");
    m.sendEnabled = o.value("sendEnabled").toBool(true);
    m.sendFrequencyHz = o.value("sendFrequencyHz").toDouble(1.0);
    m.fields = fieldsFromJson(o.value("fields").toArray());
    return m;
}

bool writeJsonAtomically(const QJsonObject& root, const QString& path, QString& errorMessage)
{
    const QString tmpPath = path + ".tmp";

    QFile tmp(tmpPath);
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        errorMessage = QString("Cannot write %1: %2").arg(tmpPath).arg(tmp.errorString());
        return false;
    }
    tmp.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    tmp.close();

    if (QFile::exists(path))
    {
        const QString bakPath = path + ".bak";
        QFile::remove(bakPath);
        QFile::copy(path, bakPath);
        if (!QFile::remove(path))
        {
            errorMessage = QString("Cannot replace %1 (file in use?).").arg(path);
            QFile::remove(tmpPath);
            return false;
        }
    }

    if (!QFile::rename(tmpPath, path))
    {
        errorMessage = QString("Cannot rename %1 to %2.").arg(tmpPath).arg(path);
        return false;
    }

    return true;
}
} // namespace

bool SimSetupFile::save(const SimSetup& setup, const QString& path, QString& errorMessage)
{
    errorMessage.clear();

    QJsonObject destination;
    destination.insert("type", setup.destinationType);

    QJsonObject udp;
    udp.insert("ip", setup.udpIp);
    udp.insert("port", setup.udpPort);
    destination.insert("udp", udp);

    QJsonObject serial;
    serial.insert("portName", setup.serialPortName);
    serial.insert("baudRate", setup.serialBaud);
    serial.insert("dataBits", setup.serialDataBits);
    serial.insert("parity", setup.serialParity);
    serial.insert("stopBits", setup.serialStopBits);
    destination.insert("serial", serial);

    QJsonArray messages;
    for (int i = 0; i < setup.messages.size(); ++i)
        messages.append(messageToJson(setup.messages.at(i)));

    QJsonObject root;
    root.insert("version", setup.version);
    root.insert("kind", "UniversalDataSimulatorSetup");
    root.insert("appVersion", "1.0");
    root.insert("savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("destination", destination);
    root.insert("messages", messages);

    return writeJsonAtomically(root, path, errorMessage);
}

bool SimSetupFile::load(const QString& path, SimSetup& setup, QString& errorMessage)
{
    errorMessage.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage = QString("Cannot open %1: %2").arg(path).arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (doc.isNull() || !doc.isObject())
    {
        errorMessage = QString("%1 is not valid JSON: %2").arg(path).arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const QString kind = root.value("kind").toString();
    if (!kind.isEmpty() && kind != "UniversalDataSimulatorSetup")
    {
        errorMessage = QString("%1 is a '%2' file, not a Universal Data Simulator setup.")
                           .arg(path).arg(kind);
        return false;
    }

    SimSetup loaded;
    loaded.version = root.value("version").toInt(1);

    const QJsonObject destination = root.value("destination").toObject();
    const QString type = destination.value("type").toString("UDP").toUpper();
    loaded.destinationType = (type == "SERIAL") ? "SERIAL" : "UDP";

    const QJsonObject udp = destination.value("udp").toObject();
    loaded.udpIp = udp.value("ip").toString();
    loaded.udpPort = udp.value("port").toInt(5000);

    const QJsonObject serial = destination.value("serial").toObject();
    loaded.serialPortName = serial.value("portName").toString();
    loaded.serialBaud = serial.value("baudRate").toInt(115200);
    loaded.serialDataBits = serial.value("dataBits").toInt(8);
    loaded.serialParity = serial.value("parity").toString("None");
    loaded.serialStopBits = serial.value("stopBits").toString("1");

    const QJsonArray messages = root.value("messages").toArray();
    for (int i = 0; i < messages.size(); ++i)
        loaded.messages.append(messageFromJson(messages.at(i).toObject()));

    setup = loaded;
    return true;
}

QString SimSetupFile::autoSavePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/last_setup.json";
}

QString SimSetupFile::fieldListToJson(const QList<FieldDefinition>& fields)
{
    QJsonObject root;
    root.insert("version", 1);
    root.insert("kind", "UniversalDataSimulatorFieldList");
    root.insert("exportedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert("fields", fieldsToJson(fields));
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool SimSetupFile::fieldListFromJson(const QString& jsonText,
                                     QList<FieldDefinition>& fields,
                                     QString& errorMessage)
{
    errorMessage.clear();
    fields.clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (doc.isNull() || !doc.isObject())
    {
        errorMessage = QString("Not valid JSON: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const QString kind = root.value("kind").toString();
    if (!kind.isEmpty()
        && kind != "UniversalDataSimulatorFieldList"
        && kind != "PcapUdpExtractorFieldList")
    {
        errorMessage = QString("'%1' is not a field-list file (kind: %2).")
                           .arg(kind).arg(kind);
        return false;
    }

    const QJsonArray arr = root.value("fields").toArray();
    if (arr.isEmpty())
    {
        errorMessage = "The JSON contains no fields.";
        return false;
    }

    fields = fieldsFromJson(arr);
    return true;
}
