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
#include <QVector>
#include <QXmlStreamReader>
#include <QtGlobal>

#include <algorithm>

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

// --- Stage 2 (grouped): merge several tables into one message --------------

namespace
{
// Lowest 0-based corrected byte offset across a table's data rows (from startRow),
// using `profile`'s offset column + base. `found` is set false when no usable row.
int minCorrectedOffset(const IcdRawTable& table, const IcdMappingProfile& profile,
                       int startRow, bool& found)
{
    found = false;
    int minv = 0;
    for (int rowIdx = qMax(0, startRow); rowIdx < table.rows.size(); ++rowIdx)
    {
        const QStringList& cells = table.rows.at(rowIdx);
        if (cellAt(cells, profile.colName).isEmpty())
            continue;
        bool offOk = false;
        const int rawOff = parseLeadingInt(cellAt(cells, profile.colByteOffset), offOk);
        if (!offOk)
            continue;
        const int corrected = (profile.offsetBase == 1) ? rawOff - 1 : rawOff;
        if (corrected < 0)
            continue;
        if (!found || corrected < minv)
        {
            minv = corrected;
            found = true;
        }
    }
    return minv;
}

// Append one table's field rows to outFields, applying `profile` and adding
// baseOffset to every 0-based byte offset. Header/blank rows (no name, or a
// non-numeric offset) are skipped silently so a child table that repeats the
// header column titles does not produce junk fields. Updates fieldNames (dedup)
// and runningExtent (running max byte extent). tIdx is 0-based for messages.
void appendFieldsFromTable(const IcdRawTable& table, const IcdMappingProfile& profile,
                           int startRow, int baseOffset,
                           QList<FieldDefinition>& outFields,
                           QSet<QString>& fieldNames, int& runningExtent,
                           QStringList& warnings, int tIdx)
{
    for (int rowIdx = qMax(0, startRow); rowIdx < table.rows.size(); ++rowIdx)
    {
        const QStringList& cells = table.rows.at(rowIdx);
        const int rowNo = rowIdx + 1;

        const QString nm = cellAt(cells, profile.colName);
        if (nm.isEmpty())
            continue;   // blank / header row

        bool offOk = false;
        const int rawOff = parseLeadingInt(cellAt(cells, profile.colByteOffset), offOk);
        if (!offOk)
            continue;   // header row ("Offset") or non-data row

        const QString typeStr = cellAt(cells, profile.colDataType);
        FieldDataType dt = FieldDataType::RawUnsignedBE;
        if (!FieldCsvCodec::dataTypeFromLabel(typeStr, dt))
        {
            warnings << QString("Table %1 row %2 ('%3'): unknown DataType '%4'; row skipped. Accepted: %5")
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
                warnings << QString("Table %1 row %2 ('%3'): Length is required for type '%4'; row skipped.")
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
                warnings << QString("Table %1 row %2 ('%3'): Length '%4' is invalid; row skipped.")
                                .arg(tIdx + 1).arg(rowNo).arg(nm).arg(lenStr);
                continue;
            }
        }

        if (dt != FieldDataType::String && length > 8)
            warnings << QString("Table %1 row %2 ('%3'): length %4 exceeds 8 bytes for a non-String type; "
                                "change the type to String or untick this field, or it will fail validation.")
                            .arg(tIdx + 1).arg(rowNo).arg(nm).arg(length);

        double resolution = 1.0;
        const QString resStr = cellAt(cells, profile.colResolution);
        if (profile.colResolution >= 0 && !resStr.isEmpty())
        {
            bool resOk = false;
            const double rv = resStr.toDouble(&resOk);
            if (!resOk)
                warnings << QString("Table %1 row %2 ('%3'): Resolution '%4' is not numeric; defaulted to 1.")
                                .arg(tIdx + 1).arg(rowNo).arg(nm).arg(resStr);
            else if (rv <= 0.0)
                warnings << QString("Table %1 row %2 ('%3'): Resolution '%4' must be positive; defaulted to 1.")
                                .arg(tIdx + 1).arg(rowNo).arg(nm).arg(resStr);
            else
                resolution = rv;
        }

        QString expr = "1";
        const QString exprStr = cellAt(cells, profile.colResolutionExpr);
        if (profile.colResolutionExpr >= 0 && !exprStr.isEmpty())
            expr = exprStr;

        const int rawCorrected = (profile.offsetBase == 1) ? rawOff - 1 : rawOff;
        if (rawCorrected < 0)
        {
            warnings << QString("Table %1 row %2 ('%3'): offset %4 (%5-based) is out of range; row skipped.")
                            .arg(tIdx + 1).arg(rowNo).arg(nm).arg(rawOff)
                            .arg(profile.offsetBase == 1 ? "1" : "0");
            continue;
        }
        const int finalCorrected = rawCorrected + baseOffset;

        QString fieldName = nm.simplified();
        QString uniqueField = fieldName;
        int fs = 2;
        while (fieldNames.contains(uniqueField))
            uniqueField = QString("%1_%2").arg(fieldName).arg(fs++);
        if (uniqueField != fieldName)
            warnings << QString("Table %1: duplicate field name '%2' renamed to '%3'.")
                            .arg(tIdx + 1).arg(fieldName).arg(uniqueField);
        fieldNames.insert(uniqueField);

        FieldDefinition f;
        f.name = uniqueField;
        f.byteOffset = finalCorrected + 1;          // keep the 1-based / 0-based invariant
        f.byteOffsetcorrect = finalCorrected;
        f.length = length;
        f.dataType = dt;
        f.resolution = resolution;
        f.resolutionExpression = expr;
        outFields.append(f);

        runningExtent = qMax(runningExtent, finalCorrected + length);
    }
}
}

void IcdDocxImporter::buildGroupedDrafts(const IcdDocument& doc,
                                         const QList<IcdTableGroup>& groups,
                                         QList<IcdMessageDraft>& drafts,
                                         QStringList& globalWarnings)
{
    drafts.clear();
    globalWarnings.clear();

    QSet<QString> usedMessageNames;

    for (int g = 0; g < groups.size(); ++g)
    {
        const IcdTableGroup& group = groups.at(g);
        const IcdMappingProfile& profile = group.mapping;
        if (group.tableIndices.isEmpty())
            continue;

        const int parentIdx = group.tableIndices.at(0);
        if (parentIdx < 0 || parentIdx >= doc.tables.size())
            continue;

        if (profile.colName < 0 || profile.colByteOffset < 0 || profile.colDataType < 0)
            globalWarnings << QString("Table %1: Name, ByteOffset and DataType columns must all be "
                                      "mapped (open its Settings) before building.").arg(parentIdx + 1);

        IcdMessageDraft draft;
        draft.sourceTableIndex = parentIdx;

        // Message name (from the parent table's heading, or a custom name).
        QString name;
        if (profile.nameSource == int(IcdNameSource::CustomPrefix))
        {
            name = profile.customNamePrefix.trimmed();
            if (name.isEmpty())
                name = QString("Message_%1").arg(parentIdx + 1);
        }
        else
        {
            name = doc.tables.at(parentIdx).precedingHeading.trimmed();
            if (name.isEmpty())
            {
                name = QString("Message_%1").arg(parentIdx + 1);
                draft.warnings << QString("Table %1: no heading found above the table; named '%2'.")
                                  .arg(parentIdx + 1).arg(name);
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

        QSet<QString> fieldNames;
        int runningExtent = 0;

        for (int gi = 0; gi < group.tableIndices.size(); ++gi)
        {
            const int tIdx = group.tableIndices.at(gi);
            if (tIdx < 0 || tIdx >= doc.tables.size())
                continue;
            const IcdRawTable& table = doc.tables.at(tIdx);

            // Parent: data begins after the mapped header row. Child: scan from the
            // top and let the skip-on-bad-offset rule drop any repeated header.
            const int startRow = (gi == 0) ? (profile.headerRowIndex + 1) : 0;

            int baseOffset = 0;
            if (gi > 0)
            {
                bool found = false;
                const int childMin = minCorrectedOffset(table, profile, startRow, found);
                if (found && childMin < runningExtent)
                {
                    baseOffset = runningExtent;     // offsets restart -> append
                    draft.warnings << QString("Table %1 merged by appending after byte %2 "
                                              "(its offsets restart at %3).")
                                      .arg(tIdx + 1).arg(runningExtent).arg(childMin);
                }
                else
                {
                    draft.warnings << QString("Table %1 merged using its own (absolute) offsets.")
                                      .arg(tIdx + 1);
                }
            }

            appendFieldsFromTable(table, profile, startRow, baseOffset,
                                  msg.fields, fieldNames, runningExtent, draft.warnings, tIdx);
        }

        if (profile.autoPayloadLength)
            msg.payloadLengthBytes = runningExtent;

        if (msg.fields.isEmpty())
            draft.warnings << QString("Group starting at Table %1 produced no valid field rows.")
                              .arg(parentIdx + 1);

        drafts.append(draft);
    }
}

void IcdDocxImporter::suggestContinuationGroups(const IcdDocument& doc,
                                                const QList<int>& selectedTableIndices,
                                                QHash<int, int>& parentOf)
{
    parentOf.clear();

    QList<int> sel = selectedTableIndices;
    std::sort(sel.begin(), sel.end());

    int lastParent = -1;    // current group's parent table index
    int lastIdx = -1;       // previous selected table index
    int parentCols = -1;    // column count of the current group's parent

    for (int k = 0; k < sel.size(); ++k)
    {
        const int t = sel.at(k);
        if (t < 0 || t >= doc.tables.size())
            continue;
        const IcdRawTable& tbl = doc.tables.at(t);

        bool isContinuation = false;
        if (lastParent >= 0 && lastIdx >= 0)
        {
            const bool adjacent = (t == lastIdx + 1);
            const bool sameCols = (parentCols > 0 && tbl.columnCount == parentCols);
            const QString h = tbl.precedingHeading.trimmed().toLower();
            const bool headingOk = h.isEmpty() || h.contains("cont");
            isContinuation = adjacent && sameCols && headingOk;
        }

        if (isContinuation)
        {
            parentOf.insert(t, lastParent);
            lastIdx = t;                     // parent + parentCols unchanged
        }
        else
        {
            parentOf.insert(t, t);           // its own parent (standalone for now)
            lastParent = t;
            lastIdx = t;
            parentCols = tbl.columnCount;
        }
    }
}

// --- Stage 2b: suggestMapping (heuristic auto-detection) -------------------

namespace
{
double fracOf(int part, int whole)
{
    return whole > 0 ? double(part) / double(whole) : 0.0;
}

bool headerHasAny(const QString& headerLower, const QStringList& keys)
{
    for (int i = 0; i < keys.size(); ++i)
        if (!keys.at(i).isEmpty() && headerLower.contains(keys.at(i)))
            return true;
    return false;
}

// Per-column profile gathered over a table's data rows.
struct ColumnStats
{
    int     nonEmpty;       // data cells that are not blank
    int     numeric;        // cells that parse as a leading int
    int     typeLike;       // cells that resolve to a FieldDataType label
    int     distinct;       // distinct cell texts
    int     minInt;
    int     maxInt;
    bool    sawInt;
    bool    nonDecreasing;  // int values never decrease down the rows
    bool    hasPrev;
    int     prevVal;
    qint64  textLenSum;
    QString headerLower;

    ColumnStats()
        : nonEmpty(0), numeric(0), typeLike(0), distinct(0), minInt(0), maxInt(0),
          sawInt(false), nonDecreasing(true), hasPrev(false), prevVal(0), textLenSum(0)
    {
    }
};
}

int IcdDocxImporter::suggestRepeatCount(const IcdRawTable& table)
{
    QString hay = table.precedingHeading;
    hay += QLatin1Char('\n');
    for (int r = 0; r < table.rows.size(); ++r)
        hay += table.rows.at(r).join(QLatin1Char(' ')) + QLatin1Char('\n');

    // Largest index in phrases like "Target 8" / "Item 4" / "Channel 16".
    QRegularExpression idxRe(
        "\\b(?:target|item|channel|block|track|element|record|set|sensor|contact)\\s*0*([0-9]{1,3})\\b",
        QRegularExpression::CaseInsensitiveOption);
    int maxIdx = 1;
    QRegularExpressionMatchIterator it = idxRe.globalMatch(hay);
    while (it.hasNext())
    {
        const int v = it.next().captured(1).toInt();
        if (v > maxIdx && v <= 256)
            maxIdx = v;
    }
    return maxIdx;
}

void IcdDocxImporter::suggestMapping(const IcdRawTable& table, IcdMappingProfile& profile)
{
    const int rowCount = table.rows.size();
    const int colCount = table.columnCount;
    if (rowCount < 1 || colCount < 1)
        return;

    const QStringList kNameHdr = QStringList()
        << "name" << "field" << "parameter" << "signal" << "mnemonic" << "label";
    const QStringList kOffHdr  = QStringList()
        << "offset" << "position" << "byte" << "address" << "loc";
    const QStringList kTypeHdr = QStringList()
        << "type" << "format" << "encoding";
    const QStringList kLenHdr  = QStringList()
        << "length" << "len" << "size" << "width" << "bytes" << "octet";
    const QStringList kResHdr  = QStringList()
        << "resolution" << "scale" << "lsb" << "factor";
    const QStringList kExprHdr = QStringList()
        << "expression" << "formula" << "conversion" << "equation" << "scaling";

    // --- 1. Header row: among the first few rows, the one with the most role
    // keywords in its cells. Field tables almost always have it at row 0, but some
    // carry a title/caption row above it.
    QStringList allKeywords;
    allKeywords << kNameHdr << kOffHdr << kTypeHdr << kLenHdr << kResHdr << kExprHdr;

    int headerRow = 0;
    int bestScore = -1;
    const int scanLimit = qMin(rowCount, 6);
    for (int rr = 0; rr < scanLimit; ++rr)
    {
        const QStringList& cells = table.rows.at(rr);
        int score = 0;
        for (int c = 0; c < cells.size(); ++c)
        {
            const QString low = cells.at(c).trimmed().toLower();
            if (!low.isEmpty() && headerHasAny(low, allKeywords))
                ++score;
        }
        if (score > bestScore)
        {
            bestScore = score;
            headerRow = rr;
        }
    }
    profile.headerRowIndex = headerRow;

    const QStringList header = (headerRow < rowCount) ? table.rows.at(headerRow) : QStringList();

    // --- 2. Per-column stats over the data rows.
    QVector<ColumnStats> stats(colCount);
    QVector<QSet<QString> > seen(colCount);
    for (int c = 0; c < colCount; ++c)
        stats[c].headerLower = (c < header.size()) ? header.at(c).trimmed().toLower() : QString();

    for (int rr = headerRow + 1; rr < rowCount; ++rr)
    {
        const QStringList& cells = table.rows.at(rr);
        bool anyText = false;
        for (int c = 0; c < cells.size(); ++c)
            if (!cells.at(c).trimmed().isEmpty()) { anyText = true; break; }
        if (!anyText)
            continue;

        for (int c = 0; c < colCount; ++c)
        {
            const QString cell = (c < cells.size()) ? cells.at(c).trimmed() : QString();
            if (cell.isEmpty())
                continue;
            ColumnStats& s = stats[c];
            ++s.nonEmpty;
            s.textLenSum += cell.size();
            if (!seen[c].contains(cell)) { seen[c].insert(cell); ++s.distinct; }

            bool ok = false;
            const int v = parseLeadingInt(cell, ok);
            if (ok)
            {
                ++s.numeric;
                if (!s.sawInt) { s.minInt = v; s.maxInt = v; s.sawInt = true; }
                else { s.minInt = qMin(s.minInt, v); s.maxInt = qMax(s.maxInt, v); }
                if (s.hasPrev && v < s.prevVal)
                    s.nonDecreasing = false;
                s.prevVal = v;
                s.hasPrev = true;
            }

            FieldDataType dt = FieldDataType::RawUnsignedBE;
            if (FieldCsvCodec::dataTypeFromLabel(cell, dt))
                ++s.typeLike;
        }
    }

    // --- 3. DataType column: most type-token-like cells (header confirms).
    int colType = -1;
    double bestType = 0.0;
    for (int c = 0; c < colCount; ++c)
    {
        const ColumnStats& s = stats[c];
        if (s.nonEmpty < 1)
            continue;
        const double tf = fracOf(s.typeLike, s.nonEmpty);
        const bool hdr = headerHasAny(s.headerLower, kTypeHdr);
        if (tf < 0.5 && !hdr)
            continue;
        const double sc = tf + (hdr ? 0.5 : 0.0);
        if (sc > bestType) { bestType = sc; colType = c; }
    }

    // --- 4. Offset & Length among the mostly-numeric columns. Offset grows down
    // the table (wide, increasing, distinct); Length is small and repeats.
    int colOffset = -1, colLength = -1;
    double bestOff = -1.0, bestLen = -1.0;
    for (int c = 0; c < colCount; ++c)
    {
        if (c == colType)
            continue;
        const ColumnStats& s = stats[c];
        if (s.nonEmpty < 1)
            continue;
        if (fracOf(s.numeric, s.nonEmpty) < 0.6)
            continue;

        double offSc = headerHasAny(s.headerLower, kOffHdr) ? 100.0 : 0.0;
        double lenSc = headerHasAny(s.headerLower, kLenHdr) ? 100.0 : 0.0;
        if (s.sawInt)
        {
            offSc += double(s.maxInt - s.minInt);
            if (s.nonDecreasing) offSc += 5.0;
            offSc += fracOf(s.distinct, s.nonEmpty);
            if (s.maxInt <= 64) lenSc += 5.0;
            lenSc += (1.0 - fracOf(s.distinct, s.nonEmpty));
        }
        if (offSc > bestOff) { bestOff = offSc; colOffset = c; }
        if (lenSc > bestLen) { bestLen = lenSc; colLength = c; }
    }
    if (colLength == colOffset)
        colLength = -1;   // only one numeric column -> treat it as the offset

    // --- 5. Name column: textual, high-distinctness, header confirms. Falls back
    // to the first column not already claimed.
    int colName = -1;
    double bestName = -1e9;
    for (int c = 0; c < colCount; ++c)
    {
        if (c == colType || c == colOffset || c == colLength)
            continue;
        const ColumnStats& s = stats[c];
        if (s.nonEmpty < 1)
            continue;
        const double nf = fracOf(s.numeric, s.nonEmpty);
        const double avgLen = fracOf(int(s.textLenSum), s.nonEmpty);
        double sc = headerHasAny(s.headerLower, kNameHdr) ? 100.0 : 0.0;
        sc -= nf * 20.0;                            // numeric content is un-name-like
        sc += fracOf(s.distinct, s.nonEmpty) * 10.0;
        sc += qMin(avgLen, 40.0) * 0.1;
        sc -= c * 0.01;                             // names tend to sit first
        if (sc > bestName) { bestName = sc; colName = c; }
    }
    if (colName < 0)
    {
        for (int c = 0; c < colCount; ++c)
        {
            if (c == colType || c == colOffset || c == colLength)
                continue;
            if (stats[c].nonEmpty >= 1) { colName = c; break; }
        }
    }

    // --- 6. Resolution / Expression: header keywords only (content too ambiguous).
    int colRes = -1, colExpr = -1;
    for (int c = 0; c < colCount; ++c)
    {
        if (c == colName || c == colOffset || c == colType || c == colLength)
            continue;
        if (colRes < 0 && headerHasAny(stats[c].headerLower, kResHdr))
            colRes = c;
    }
    for (int c = 0; c < colCount; ++c)
    {
        if (c == colName || c == colOffset || c == colType || c == colLength || c == colRes)
            continue;
        if (colExpr < 0 && headerHasAny(stats[c].headerLower, kExprHdr))
            colExpr = c;
    }

    // --- 7. Offset base from the smallest value seen in the offset column.
    if (colOffset >= 0 && stats[colOffset].sawInt)
    {
        if (stats[colOffset].minInt == 0)
            profile.offsetBase = 0;
        else if (stats[colOffset].minInt == 1)
            profile.offsetBase = 1;
        // any other minimum: leave the caller's existing base untouched
    }

    // --- 8. Commit. Unsure roles stay -1 so the caller can keep its own guess.
    profile.colName          = colName;
    profile.colByteOffset    = colOffset;
    profile.colDataType      = colType;
    profile.colLength        = colLength;
    profile.colResolution    = colRes;
    profile.colResolutionExpr = colExpr;
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
    o["colDescription"] = profile.colDescription;
    o["autoOffsetFromSize"] = profile.autoOffsetFromSize;
    o["offsetStartByte"] = profile.offsetStartByte;
    o["repeatCount"] = profile.repeatCount;
    o["repeatStrideBytes"] = profile.repeatStrideBytes;
    o["repeatNamePattern"] = profile.repeatNamePattern;
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
    p.colDescription = o.value("colDescription").toInt(p.colDescription);
    p.autoOffsetFromSize = o.value("autoOffsetFromSize").toBool(p.autoOffsetFromSize);
    p.offsetStartByte = o.value("offsetStartByte").toInt(p.offsetStartByte);
    p.repeatCount = o.value("repeatCount").toInt(p.repeatCount);
    p.repeatStrideBytes = o.value("repeatStrideBytes").toInt(p.repeatStrideBytes);
    p.repeatNamePattern = o.value("repeatNamePattern").toString(p.repeatNamePattern);
    profile = p;
    return true;
}
