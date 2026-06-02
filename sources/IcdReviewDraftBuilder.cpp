#include "IcdReviewDraftBuilder.h"

#include "FieldCsvCodec.h"

#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>

#include <algorithm>

namespace
{
QString cellAt(const QStringList& cells, int idx)
{
    if (idx >= 0 && idx < cells.size())
        return cells.at(idx).trimmed();
    return QString();
}

int parseLeadingInt(const QString& s, bool& ok)
{
    ok = false;
    const QString t = s.trimmed();
    if (t.isEmpty())
        return 0;

    if (t.startsWith("0x", Qt::CaseInsensitive))
    {
        const int v = t.mid(2).toInt(&ok, 16);
        if (ok)
            return v;
    }

    int v = t.toInt(&ok);
    if (ok)
        return v;

    QRegularExpression re("(-?\\d+)");
    const QRegularExpressionMatch m = re.match(t);
    if (m.hasMatch())
    {
        v = m.captured(1).toInt(&ok);
        if (ok)
            return v;
    }
    ok = false;
    return 0;
}

bool rowHasAnyText(const QStringList& cells)
{
    for (int i = 0; i < cells.size(); ++i)
    {
        if (!cells.at(i).trimmed().isEmpty())
            return true;
    }
    return false;
}

bool looksLikeHeaderRow(const QStringList& cells, const IcdMappingProfile& profile)
{
    QStringList keys;
    keys << "name" << "field" << "parameter" << "signal" << "mnemonic"
         << "offset" << "position" << "byte" << "type" << "format" << "encoding"
         << "length" << "len" << "size" << "resolution" << "scale" << "expression";

    int hits = 0;
    for (int i = 0; i < cells.size(); ++i)
    {
        const QString low = cells.at(i).trimmed().toLower();
        if (low.isEmpty())
            continue;
        for (int k = 0; k < keys.size(); ++k)
        {
            if (low.contains(keys.at(k)))
            {
                ++hits;
                break;
            }
        }
    }

    // Repeated child headers usually contain several role labels. A real sparse
    // field row should not be discarded just because one cell says "byte".
    if (hits >= 2)
        return true;

    const QString nm = cellAt(cells, profile.colName).toLower();
    const QString off = cellAt(cells, profile.colByteOffset).toLower();
    const QString typ = cellAt(cells, profile.colDataType).toLower();
    return (nm.contains("name") || nm.contains("field"))
        && (off.contains("offset") || off.contains("byte") || off.contains("position"))
        && (typ.contains("type") || typ.contains("format") || typ.contains("encoding"));
}

QString normalisedTypeText(const QString& typeText, int sizeBytes, QStringList& warnings,
                           int tableIndex, int rowNo, const QString& rowName)
{
    const QString t = typeText.trimmed();
    if (t.isEmpty())
        return QString();

    // Size-aware so verbose ICD spellings ("Unsigned Integer", "Unsigned Long",
    // "Uchar", "Float", ...) resolve, using the Size column to fix integer width.
    FieldDataType dt = FieldDataType::RawUnsignedBE;
    if (!FieldCsvCodec::dataTypeFromLabelAndSize(t, sizeBytes, dt))
    {
        warnings << QString("Table %1 row %2 ('%3'): DataType '%4' is not recognised; left empty for review. Accepted: %5")
                    .arg(tableIndex + 1).arg(rowNo).arg(rowName).arg(t)
                    .arg(FieldCsvCodec::supportedDataTypeLabels().join(", "));
        return QString();
    }
    return FieldCsvCodec::dataTypeToLabel(dt);
}

// Substitute {name} and {n} in a repeat name pattern, guaranteeing both the field
// name and the block index survive even when the pattern omits a token.
QString applyRepeatPattern(const QString& pattern, const QString& name, int n)
{
    QString p = pattern.trimmed();
    if (p.isEmpty())
        p = QStringLiteral("{name}_{n}");
    QString out = p;
    out.replace("{name}", name);
    out.replace("{n}", QString::number(n));
    if (!p.contains("{name}"))
        out = name + "_" + out;
    if (!p.contains("{n}"))
        out = out + "_" + QString::number(n);
    return out;
}

int correctedOffsetFromReviewText(const QString& offText, const IcdMappingProfile& profile, bool& ok)
{
    const int raw = parseLeadingInt(offText, ok);
    if (!ok)
        return 0;
    return (profile.offsetBase == 1) ? raw - 1 : raw;
}

QString reviewOffsetText(const QString& offText, const IcdMappingProfile& profile,
                         int baseOffset, QStringList& warnings,
                         int tableIndex, int rowNo, const QString& rowName)
{
    const QString t = offText.trimmed();
    if (t.isEmpty())
        return QString();

    bool ok = false;
    const int corrected = correctedOffsetFromReviewText(t, profile, ok);
    if (!ok || corrected < 0)
    {
        warnings << QString("Table %1 row %2 ('%3'): ByteOffset '%4' is invalid; left empty for review.")
                    .arg(tableIndex + 1).arg(rowNo).arg(rowName).arg(t);
        return QString();
    }

    const int finalCorrected = corrected + baseOffset;
    return QString::number(finalCorrected + 1); // review tree is always UI 1-based
}

QString reviewLengthText(const QString& lenText, QStringList& warnings, int tableIndex, int rowNo, const QString& rowName)
{
    const QString t = lenText.trimmed();
    if (t.isEmpty())
        return QString();

    bool ok = false;
    const int len = parseLeadingInt(t, ok);
    if (!ok || len < 1)
    {
        warnings << QString("Table %1 row %2 ('%3'): Length '%4' is invalid; left empty for review.")
                    .arg(tableIndex + 1).arg(rowNo).arg(rowName).arg(t);
        return QString();
    }
    return QString::number(len);
}

QString reviewResolutionText(const QString& resText, QStringList& warnings, int tableIndex, int rowNo, const QString& rowName)
{
    const QString t = resText.trimmed();
    if (t.isEmpty())
        return QString();

    bool ok = false;
    const double v = t.toDouble(&ok);
    if (!ok || v <= 0.0)
    {
        warnings << QString("Table %1 row %2 ('%3'): Resolution '%4' is invalid; left empty/default 1 for review.")
                    .arg(tableIndex + 1).arg(rowNo).arg(rowName).arg(t);
        return QString();
    }
    return QString::number(v, 'g', 10);
}

int minCorrectedOffset(const IcdRawTable& table, const IcdMappingProfile& profile,
                       int startRow, bool& found)
{
    found = false;
    if (profile.colByteOffset < 0)
        return 0;

    int minv = 0;
    for (int rowIdx = qMax(0, startRow); rowIdx < table.rows.size(); ++rowIdx)
    {
        const QStringList& cells = table.rows.at(rowIdx);
        if (!rowHasAnyText(cells) || looksLikeHeaderRow(cells, profile))
            continue;

        bool offOk = false;
        const int corrected = correctedOffsetFromReviewText(cellAt(cells, profile.colByteOffset), profile, offOk);
        if (!offOk || corrected < 0)
            continue;

        if (!found || corrected < minv)
        {
            minv = corrected;
            found = true;
        }
    }
    return minv;
}

void appendRowsFromTable(const IcdRawTable& table,
                         const IcdMappingProfile& profile,
                         int tableIndex,
                         int startRow,
                         int baseOffset,
                         QList<IcdFieldDraftRow>& outRows,
                         int& runningExtent,
                         int& offsetCursor,
                         QStringList& warnings)
{
    for (int rowIdx = qMax(0, startRow); rowIdx < table.rows.size(); ++rowIdx)
    {
        const QStringList& cells = table.rows.at(rowIdx);
        if (!rowHasAnyText(cells))
            continue;
        if (looksLikeHeaderRow(cells, profile))
            continue;

        const int rowNo = rowIdx + 1;
        IcdFieldDraftRow row;
        row.sourceTableIndex = tableIndex;
        row.sourceRowIndex = rowIdx;

        row.name = cellAt(cells, profile.colName).simplified();
        const QString warnName = row.name.isEmpty() ? QString("row %1").arg(rowNo) : row.name;

        row.lengthText = reviewLengthText(cellAt(cells, profile.colLength), warnings, tableIndex, rowNo, warnName);
        bool lenKnownOk = false;
        const int lenInt = row.lengthText.toInt(&lenKnownOk);

        // ByteOffset: from the offset column (default) or computed cumulatively from
        // field sizes when the user enabled "offsets from size".
        if (profile.autoOffsetFromSize)
        {
            if (lenKnownOk && lenInt > 0)
            {
                row.byteOffsetText = QString::number(offsetCursor);   // 1-based
                offsetCursor += lenInt;
            }
            else
            {
                row.byteOffsetText = QString();
                warnings << QString("Table %1 row %2 ('%3'): size unknown; byte offset left empty (offsets-from-size).")
                            .arg(tableIndex + 1).arg(rowNo).arg(warnName);
            }
        }
        else
        {
            row.byteOffsetText = reviewOffsetText(cellAt(cells, profile.colByteOffset), profile,
                                                  baseOffset, warnings, tableIndex, rowNo, warnName);
        }

        row.dataTypeText = normalisedTypeText(cellAt(cells, profile.colDataType), lenInt, warnings, tableIndex, rowNo, warnName);
        row.resolutionText = reviewResolutionText(cellAt(cells, profile.colResolution), warnings, tableIndex, rowNo, warnName);

        const QString expr = cellAt(cells, profile.colResolutionExpr);
        row.resolutionExpression = expr.isEmpty() ? QString("1") : expr;

        if (profile.colName < 0)
            warnings << QString("Table %1 row %2: Name column not mapped; field name left empty for review.").arg(tableIndex + 1).arg(rowNo);
        else if (row.name.isEmpty())
            warnings << QString("Table %1 row %2: field name is empty; left empty for review.").arg(tableIndex + 1).arg(rowNo);

        if (profile.colByteOffset < 0 && !profile.autoOffsetFromSize)
            warnings << QString("Table %1 row %2 ('%3'): ByteOffset column not mapped; left empty for review.").arg(tableIndex + 1).arg(rowNo).arg(warnName);
        if (profile.colDataType < 0)
            warnings << QString("Table %1 row %2 ('%3'): DataType column not mapped; left empty for review.").arg(tableIndex + 1).arg(rowNo).arg(warnName);
        if (profile.colLength < 0)
            warnings << QString("Table %1 row %2 ('%3'): Length column not mapped; left empty for review.").arg(tableIndex + 1).arg(rowNo).arg(warnName);

        outRows.append(row);

        bool offOk = false;
        const int off1 = row.byteOffsetText.toInt(&offOk);
        if (offOk && lenKnownOk && off1 > 0 && lenInt > 0)
            runningExtent = qMax(runningExtent, (off1 - 1) + lenInt);
    }
}
}

void IcdReviewDraftBuilder::buildGroupedDrafts(const IcdDocument& doc,
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
        if (group.tableIndices.isEmpty())
            continue;

        const IcdMappingProfile& profile = group.mapping;
        const int parentIdx = group.tableIndices.at(0);
        if (parentIdx < 0 || parentIdx >= doc.tables.size())
            continue;

        IcdMessageDraft draft;
        draft.sourceTableIndex = parentIdx;

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

        int runningExtent = 0;
        int offsetCursor = profile.autoOffsetFromSize ? profile.offsetStartByte : 1;
        for (int gi = 0; gi < group.tableIndices.size(); ++gi)
        {
            const int tIdx = group.tableIndices.at(gi);
            if (tIdx < 0 || tIdx >= doc.tables.size())
                continue;

            const IcdRawTable& table = doc.tables.at(tIdx);
            const int startRow = (gi == 0) ? (profile.headerRowIndex + 1) : 0;

            // Merge-time offset handling only applies when offsets come from the
            // offset column; offsets-from-size uses the continuous cursor instead.
            int baseOffset = 0;
            if (!profile.autoOffsetFromSize && gi > 0 && profile.colByteOffset >= 0)
            {
                bool found = false;
                const int childMin = minCorrectedOffset(table, profile, startRow, found);
                if (found && childMin < runningExtent)
                {
                    baseOffset = runningExtent;
                    draft.warnings << QString("Table %1 merged by appending after byte %2 (its offsets restart at %3).")
                                      .arg(tIdx + 1).arg(runningExtent).arg(childMin);
                }
                else
                {
                    draft.warnings << QString("Table %1 merged using its own (absolute) offsets.")
                                      .arg(tIdx + 1);
                }
            }
            else if (!profile.autoOffsetFromSize && gi > 0)
            {
                draft.warnings << QString("Table %1 merged without offset auto-adjustment because ByteOffset is not mapped.")
                                  .arg(tIdx + 1);
            }

            appendRowsFromTable(table, profile, tIdx, startRow, baseOffset,
                                draft.fieldRows, runningExtent, offsetCursor, draft.warnings);
        }

        // Repeat-block replication: the rows built above are one block; clone the
        // whole block (repeatCount - 1) more times at a fixed stride, renaming each.
        if (profile.repeatCount > 1 && !draft.fieldRows.isEmpty())
        {
            const QList<IcdFieldDraftRow> baseRows = draft.fieldRows;
            int minOff = -1;     // smallest 0-based offset in the base block
            int maxExt = 0;      // largest 0-based end (offset + len) in the base block
            for (int i = 0; i < baseRows.size(); ++i)
            {
                bool oOk = false, lOk = false;
                const int off1 = baseRows.at(i).byteOffsetText.toInt(&oOk);
                const int len = baseRows.at(i).lengthText.toInt(&lOk);
                if (!oOk || off1 <= 0)
                    continue;
                const int off0 = off1 - 1;
                if (minOff < 0 || off0 < minOff)
                    minOff = off0;
                const int end = (lOk && len > 0) ? (off0 + len) : (off0 + 1);
                if (end > maxExt)
                    maxExt = end;
            }

            int stride = profile.repeatStrideBytes;
            if (stride <= 0)
                stride = (minOff >= 0 && maxExt > minOff) ? (maxExt - minOff) : 0;

            if (stride <= 0)
            {
                draft.warnings << QString("Repeat x%1 requested but the block stride could not be determined "
                                          "(need numeric offsets and sizes); block not replicated.")
                                  .arg(profile.repeatCount);
            }
            else
            {
                QList<IcdFieldDraftRow> expanded;
                for (int k = 0; k < profile.repeatCount; ++k)
                {
                    for (int i = 0; i < baseRows.size(); ++i)
                    {
                        IcdFieldDraftRow r = baseRows.at(i);
                        bool oOk = false;
                        const int off1 = r.byteOffsetText.toInt(&oOk);
                        if (oOk && off1 > 0)
                            r.byteOffsetText = QString::number(off1 + k * stride);
                        r.name = applyRepeatPattern(profile.repeatNamePattern, baseRows.at(i).name, k + 1);
                        expanded.append(r);
                    }
                }
                draft.fieldRows = expanded;
                runningExtent = qMax(runningExtent, maxExt + (profile.repeatCount - 1) * stride);
                draft.warnings << QString("Replicated the field block x%1 at stride %2 bytes (%3 fields total).")
                                  .arg(profile.repeatCount).arg(stride).arg(expanded.size());
            }
        }

        if (profile.autoPayloadLength)
            msg.payloadLengthBytes = runningExtent;

        if (draft.fieldRows.isEmpty())
            draft.warnings << QString("Group starting at Table %1 produced no reviewable field rows.")
                              .arg(parentIdx + 1);

        drafts.append(draft);
    }
}
