#include "IcdDocxImporter.h"

#include "FieldCsvCodec.h"

// Qt ships a zip reader in its (private) GUI module. Using it keeps the whole
// feature fully offline with no extra third-party dependency and no GPL/LGPL
// addition beyond Qt itself. The .pro adds `QT += gui-private` for this header.
#include <private/qzipreader_p.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QtGlobal>

namespace
{
// --- WordprocessingML parsing helpers -------------------------------------
// We compare local element names (namespace processing is on by default, so
// QXmlStreamReader::name() returns the local name regardless of the "w:" prefix).

int attrIntVal(const QXmlStreamReader& r, const char* localName, int defVal)
{
    const QXmlStreamAttributes attrs = r.attributes();
    for (int i = 0; i < attrs.size(); ++i)
    {
        if (attrs.at(i).name() == QLatin1String(localName))
        {
            bool ok = false;
            const int v = attrs.at(i).value().toString().toInt(&ok);
            return ok ? v : defVal;
        }
    }
    return defVal;
}

QString cellAt(const QStringList& cells, int idx)
{
    if (idx >= 0 && idx < cells.size())
        return cells.at(idx).trimmed();
    return QString();
}

// Parse a possibly-messy numeric cell ("5", "0x0A", "Byte 5"). Returns the first
// integer found. Decimal by default; hex when the cell starts with 0x.
int parseLeadingInt(const QString& s, bool& ok)
{
    ok = false;
    const QString t = s.trimmed();
    if (t.isEmpty())
        return 0;

    if (t.startsWith("0x", Qt::CaseInsensitive))
    {
        const int v = t.mid(2).toInt(&ok, 16);
        if (ok) return v;
    }

    int v = t.toInt(&ok);
    if (ok) return v;

    QRegularExpression re("(-?\\d+)");
    const QRegularExpressionMatch m = re.match(t);
    if (m.hasMatch())
    {
        v = m.captured(1).toInt(&ok);
        if (ok) return v;
    }
    ok = false;
    return 0;
}

void parseCell(QXmlStreamReader& r, QString& text, int& gridSpan)
{
    gridSpan = 1;
    QStringList paras;
    while (!r.atEnd())
    {
        r.readNext();
        if (r.isEndElement() && r.name() == QLatin1String("tc"))
            break;
        if (r.isStartElement())
        {
            if (r.name() == QLatin1String("tbl"))
            {
                // Nested table inside a cell — skip its whole subtree so its inner
                // <w:tc> end tags do not confuse this loop.
                r.skipCurrentElement();
                continue;
            }
            if (r.name() == QLatin1String("gridSpan"))
                gridSpan = attrIntVal(r, "val", 1);
            else if (r.name() == QLatin1String("p"))
                paras << r.readElementText(QXmlStreamReader::IncludeChildElements);
        }
    }
    text = paras.join(" ").simplified();
    if (gridSpan < 1)
        gridSpan = 1;
}

void parseRow(QXmlStreamReader& r, QStringList& row)
{
    while (!r.atEnd())
    {
        r.readNext();
        if (r.isEndElement() && r.name() == QLatin1String("tr"))
            break;
        if (r.isStartElement())
        {
            if (r.name() == QLatin1String("tbl"))
            {
                r.skipCurrentElement();
                continue;
            }
            if (r.name() == QLatin1String("tc"))
            {
                QString text;
                int span = 1;
                parseCell(r, text, span);
                row.append(text);
                for (int s = 1; s < span; ++s)
                    row.append(QString());   // pad horizontally-merged columns
            }
        }
    }
}

void parseTable(QXmlStreamReader& r, IcdRawTable& table)
{
    while (!r.atEnd())
    {
        r.readNext();
        if (r.isEndElement() && r.name() == QLatin1String("tbl"))
            break;
        if (r.isStartElement() && r.name() == QLatin1String("tr"))
        {
            QStringList row;
            parseRow(r, row);
            table.rows.append(row);
        }
    }
    int maxCols = 0;
    for (int i = 0; i < table.rows.size(); ++i)
        maxCols = qMax(maxCols, table.rows.at(i).size());
    table.columnCount = maxCols;
}

void parseDocumentXml(const QByteArray& xml, IcdDocument& doc)
{
    QXmlStreamReader r(xml);
    QString pendingHeading;   // last non-empty paragraph seen before a table

    while (!r.atEnd())
    {
        r.readNext();
        if (!r.isStartElement())
            continue;

        if (r.name() == QLatin1String("tbl"))
        {
            IcdRawTable table;
            table.precedingHeading = pendingHeading.trimmed();
            parseTable(r, table);
            doc.tables.append(table);
            pendingHeading.clear();
        }
        else if (r.name() == QLatin1String("p"))
        {
            const QString txt = r.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
            if (!txt.isEmpty())
                pendingHeading = txt;
        }
    }
}

QString sanitizeProfileFileStem(const QString& name)
{
    QString stem = name.trimmed();
    stem.replace(QRegularExpression("[^A-Za-z0-9 _.-]"), "_");
    stem = stem.simplified();
    stem.replace(' ', '_');
    if (stem.isEmpty())
        stem = "profile";
    return stem;
}

const QString kProfileSuffix = QStringLiteral(".icdmap.json");
}

// --- Stage 1: extract ------------------------------------------------------

bool IcdDocxImporter::extract(const QString& docxPath, IcdDocument& doc, QString& errorMessage)
{
    doc.tables.clear();
    errorMessage.clear();

    QZipReader zip(docxPath);
    if (zip.status() != QZipReader::NoError)
    {
        errorMessage = "Could not open the file as a Word .docx (it is not a valid "
                       "Office Open XML / ZIP package).";
        return false;
    }

    const QByteArray xml = zip.fileData(QStringLiteral("word/document.xml"));
    if (xml.isEmpty())
    {
        errorMessage = "word/document.xml was not found or is empty. Is this really a "
                       "Word .docx file? (Legacy .doc and image-only documents are not supported.)";
        return false;
    }

    parseDocumentXml(xml, doc);

    if (doc.tables.isEmpty())
    {
        errorMessage = "No tables were found in the document. ICD message/field "
                       "definitions must be laid out in Word tables.";
        return false;
    }
    return true;
}

// --- Stage 2: buildDrafts --------------------------------------------------

void IcdDocxImporter::buildDrafts(const IcdDocument& doc,
                                  const QList<int>& selectedTableIndices,
                                  const IcdMappingProfile& profile,
                                  QList<IcdMessageDraft>& drafts,
                                  QStringList& globalWarnings)
{
    drafts.clear();
    globalWarnings.clear();

    if (profile.colName < 0 || profile.colByteOffset < 0 || profile.colDataType < 0)
        globalWarnings << "Name, ByteOffset and DataType columns must all be mapped before building.";

    int customCounter = 0;
    QSet<QString> usedMessageNames;

    for (int sel = 0; sel < selectedTableIndices.size(); ++sel)
    {
        const int tIdx = selectedTableIndices.at(sel);
        if (tIdx < 0 || tIdx >= doc.tables.size())
            continue;

        const IcdRawTable& table = doc.tables.at(tIdx);
        IcdMessageDraft draft;
        draft.sourceTableIndex = tIdx;

        // Message name.
        QString name;
        if (profile.nameSource == int(IcdNameSource::CustomPrefix))
        {
            ++customCounter;
            name = QString("%1_%2").arg(profile.customNamePrefix).arg(customCounter);
        }
        else
        {
            name = table.precedingHeading.trimmed();
            if (name.isEmpty())
            {
                name = QString("Message_%1").arg(tIdx + 1);
                draft.warnings << QString("Table %1: no heading found above the table; named '%2'.")
                                  .arg(tIdx + 1).arg(name);
            }
        }
        name = name.simplified();

        QString uniqueName = name;
        int suffix = 2;
        while (usedMessageNames.contains(uniqueName))
            uniqueName = QString("%1_%2").arg(name).arg(suffix++);
        if (uniqueName != name)
            draft.warnings << QString("Duplicate message name '%1' renamed to '%2'.").arg(name).arg(uniqueName);
        usedMessageNames.insert(uniqueName);

        MessageDefinition& msg = draft.message;
        msg.messageName = uniqueName;
        msg.port = (profile.defaultPort > 0 && profile.defaultPort <= 65535)
                       ? static_cast<quint16>(profile.defaultPort)
                       : static_cast<quint16>(0);
        msg.dataFormat = "HEX";

        const int hr = profile.headerRowIndex;
        int maxExtent = 0;
        QSet<QString> fieldNames;

        for (int rowIdx = hr + 1; rowIdx < table.rows.size(); ++rowIdx)
        {
            const QStringList& cells = table.rows.at(rowIdx);

            bool anyText = false;
            for (int c = 0; c < cells.size(); ++c)
                if (!cells.at(c).trimmed().isEmpty()) { anyText = true; break; }
            if (!anyText)
                continue;

            const int rowNo = rowIdx + 1;

            const QString nm = cellAt(cells, profile.colName);
            if (nm.isEmpty())
            {
                draft.warnings << QString("Table %1 row %2: no field name; row skipped.").arg(tIdx + 1).arg(rowNo);
                continue;
            }

            const QString offStr = cellAt(cells, profile.colByteOffset);
            bool offOk = false;
            const int rawOff = parseLeadingInt(offStr, offOk);
            if (!offOk)
            {
                draft.warnings << QString("Table %1 row %2 ('%3'): ByteOffset '%4' is not a number; row skipped.")
                                  .arg(tIdx + 1).arg(rowNo).arg(nm).arg(offStr);
                continue;
            }

            const QString typeStr = cellAt(cells, profile.colDataType);
            FieldDataType dt = FieldDataType::RawUnsignedBE;
            if (!FieldCsvCodec::dataTypeFromLabel(typeStr, dt))
            {
                draft.warnings << QString("Table %1 row %2 ('%3'): unknown DataType '%4'; row skipped. Accepted: %5")
                                  .arg(tIdx + 1).arg(rowNo).arg(nm).arg(typeStr)
                                  .arg(FieldCsvCodec::supportedDataTypeLabels().join(", "));
                continue;
            }

            int length = 0;
            const QString lenStr = cellAt(cells, profile.colLength);
            if (profile.colLength < 0 || lenStr.isEmpty())
            {
                const int natural = fieldDataTypeNaturalLength(dt);
                if (natural <= 0)
                {
                    draft.warnings << QString("Table %1 row %2 ('%3'): Length is required for type '%4'; row skipped.")
                                      .arg(tIdx + 1).arg(rowNo).arg(nm).arg(typeStr);
                    continue;
                }
                length = natural;
            }
            else
            {
                bool lenOk = false;
                length = parseLeadingInt(lenStr, lenOk);
                if (!lenOk || length < 1)
                {
                    draft.warnings << QString("Table %1 row %2 ('%3'): Length '%4' is invalid; row skipped.")
                                      .arg(tIdx + 1).arg(rowNo).arg(nm).arg(lenStr);
                    continue;
                }
            }

            if (dt != FieldDataType::String && length > 8)
                draft.warnings << QString("Table %1 row %2 ('%3'): length %4 exceeds 8 bytes for a non-String type; "
                                          "change the type to String or untick this field, or it will fail validation.")
                                  .arg(tIdx + 1).arg(rowNo).arg(nm).arg(length);

            double resolution = 1.0;
            const QString resStr = cellAt(cells, profile.colResolution);
            if (profile.colResolution >= 0 && !resStr.isEmpty())
            {
                bool resOk = false;
                const double rv = resStr.toDouble(&resOk);
                if (!resOk)
                    draft.warnings << QString("Table %1 row %2 ('%3'): Resolution '%4' is not numeric; defaulted to 1.")
                                      .arg(tIdx + 1).arg(rowNo).arg(nm).arg(resStr);
                else if (rv <= 0.0)
                    draft.warnings << QString("Table %1 row %2 ('%3'): Resolution '%4' must be positive; defaulted to 1.")
                                      .arg(tIdx + 1).arg(rowNo).arg(nm).arg(resStr);
                else
                    resolution = rv;
            }

            QString expr = "1";
            const QString exprStr = cellAt(cells, profile.colResolutionExpr);
            if (profile.colResolutionExpr >= 0 && !exprStr.isEmpty())
                expr = exprStr;

            int byteOffset1Based = 0;
            int byteOffsetCorrect = 0;
            if (profile.offsetBase == 1)
            {
                byteOffset1Based = rawOff;
                byteOffsetCorrect = rawOff - 1;
            }
            else
            {
                byteOffsetCorrect = rawOff;
                byteOffset1Based = rawOff + 1;
            }
            if (byteOffsetCorrect < 0)
            {
                draft.warnings << QString("Table %1 row %2 ('%3'): offset %4 (%5-based) is out of range; row skipped.")
                                  .arg(tIdx + 1).arg(rowNo).arg(nm).arg(rawOff)
                                  .arg(profile.offsetBase == 1 ? "1" : "0");
                continue;
            }

            QString fieldName = nm.simplified();
            QString uniqueField = fieldName;
            int fs = 2;
            while (fieldNames.contains(uniqueField))
                uniqueField = QString("%1_%2").arg(fieldName).arg(fs++);
            if (uniqueField != fieldName)
                draft.warnings << QString("Table %1: duplicate field name '%2' renamed to '%3'.")
                                  .arg(tIdx + 1).arg(fieldName).arg(uniqueField);
            fieldNames.insert(uniqueField);

            FieldDefinition f;
            f.name = uniqueField;
            f.byteOffset = byteOffset1Based;
            f.byteOffsetcorrect = byteOffsetCorrect;
            f.length = length;
            f.dataType = dt;
            f.resolution = resolution;
            f.resolutionExpression = expr;
            msg.fields.append(f);

            maxExtent = qMax(maxExtent, byteOffsetCorrect + length);
        }

        if (profile.autoPayloadLength)
            msg.payloadLengthBytes = maxExtent;

        if (msg.fields.isEmpty())
            draft.warnings << QString("Table %1: no valid field rows were produced.").arg(tIdx + 1);

        drafts.append(draft);
    }
}

// --- Profile persistence ---------------------------------------------------

QString IcdDocxImporter::profilesDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    return base + "/icd_mapping_profiles";
}

QStringList IcdDocxImporter::availableProfiles()
{
    QStringList names;
    QDir dir(profilesDirectory());
    if (!dir.exists())
        return names;
    const QStringList files = dir.entryList(QStringList() << QString("*%1").arg(kProfileSuffix),
                                            QDir::Files, QDir::Name);
    for (int i = 0; i < files.size(); ++i)
    {
        QString stem = files.at(i);
        stem.chop(kProfileSuffix.size());
        if (!stem.isEmpty())
            names << stem;
    }
    return names;
}

bool IcdDocxImporter::saveProfile(const IcdMappingProfile& profile, QString& errorMessage)
{
    errorMessage.clear();
    const QString dirPath = profilesDirectory();
    QDir dir;
    if (!dir.mkpath(dirPath))
    {
        errorMessage = QString("Could not create the profiles directory:\n%1").arg(dirPath);
        return false;
    }
    const QString stem = sanitizeProfileFileStem(profile.profileName);
    const QString path = QString("%1/%2%3").arg(dirPath).arg(stem).arg(kProfileSuffix);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        errorMessage = QString("Could not write profile file:\n%1").arg(file.errorString());
        return false;
    }
    const QByteArray data = profileToJson(profile).toUtf8();
    file.write(data);
    file.close();
    return true;
}

bool IcdDocxImporter::loadProfile(const QString& profileName, IcdMappingProfile& profile, QString& errorMessage)
{
    errorMessage.clear();
    const QString path = QString("%1/%2%3").arg(profilesDirectory()).arg(profileName).arg(kProfileSuffix);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage = QString("Could not open profile file:\n%1").arg(file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return profileFromJson(QString::fromUtf8(data), profile, errorMessage);
}

QString IcdDocxImporter::profileToJson(const IcdMappingProfile& profile)
{
    QJsonObject o;
    o["kind"] = "IcdMappingProfile";
    o["version"] = 1;
    o["profileName"] = profile.profileName;
    o["headerRowIndex"] = profile.headerRowIndex;
    o["offsetBase"] = profile.offsetBase;
    o["colName"] = profile.colName;
    o["colByteOffset"] = profile.colByteOffset;
    o["colDataType"] = profile.colDataType;
    o["colLength"] = profile.colLength;
    o["colResolution"] = profile.colResolution;
    o["colResolutionExpr"] = profile.colResolutionExpr;
    o["nameSource"] = profile.nameSource;
    o["customNamePrefix"] = profile.customNamePrefix;
    o["defaultPort"] = profile.defaultPort;
    o["autoPayloadLength"] = profile.autoPayloadLength;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

bool IcdDocxImporter::profileFromJson(const QString& jsonText, IcdMappingProfile& profile, QString& errorMessage)
{
    errorMessage.clear();
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
    {
        errorMessage = QString("Profile JSON is invalid: %1").arg(perr.errorString());
        return false;
    }
    const QJsonObject o = doc.object();
    IcdMappingProfile p;   // defaults
    p.profileName = o.value("profileName").toString(p.profileName);
    p.headerRowIndex = o.value("headerRowIndex").toInt(p.headerRowIndex);
    p.offsetBase = o.value("offsetBase").toInt(p.offsetBase);
    p.colName = o.value("colName").toInt(p.colName);
    p.colByteOffset = o.value("colByteOffset").toInt(p.colByteOffset);
    p.colDataType = o.value("colDataType").toInt(p.colDataType);
    p.colLength = o.value("colLength").toInt(p.colLength);
    p.colResolution = o.value("colResolution").toInt(p.colResolution);
    p.colResolutionExpr = o.value("colResolutionExpr").toInt(p.colResolutionExpr);
    p.nameSource = o.value("nameSource").toInt(p.nameSource);
    p.customNamePrefix = o.value("customNamePrefix").toString(p.customNamePrefix);
    p.defaultPort = o.value("defaultPort").toInt(p.defaultPort);
    p.autoPayloadLength = o.value("autoPayloadLength").toBool(p.autoPayloadLength);
    profile = p;
    return true;
}
