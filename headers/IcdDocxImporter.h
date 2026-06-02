#ifndef ICDDOCXIMPORTER_H
#define ICDDOCXIMPORTER_H

#include "IcdImportTypes.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// Pure ICD .docx reader. No Qt Widgets — this is the only place that knows how
// a .docx is unzipped (Qt's QZipReader) and how WordprocessingML tables are
// parsed (QXmlStreamReader). The dialog and MainWindow talk to this through the
// data structures in IcdImportTypes.h only.
class IcdDocxImporter
{
public:
    // Stage 1: open the .docx and extract every table into doc. Deterministic;
    // no interpretation. Returns false (with errorMessage) if the file is not a
    // readable Office Open XML package or contains no tables.
    static bool extract(const QString& docxPath, IcdDocument& doc, QString& errorMessage);

    // Stage 2: apply a mapping profile to the selected tables, producing one
    // message draft per table. Never throws; per-row problems are collected as
    // warnings on the relevant draft (and globalWarnings for document-level notes).
    static void buildDrafts(const IcdDocument& doc,
                            const QList<int>& selectedTableIndices,
                            const IcdMappingProfile& profile,
                            QList<IcdMessageDraft>& drafts,
                            QStringList& globalWarnings);

    // Stage 2 (grouped): build one message draft per group. Each group's (parent's)
    // mapping is applied to every member table and the resulting fields are
    // concatenated into a single message. Per-group byte offsets are auto-handled:
    // a child table whose offsets restart near zero is appended after the previous
    // table's extent; a child whose offsets continue is kept as written. Additive
    // companion to buildDrafts() (which stays the one-table-per-message path).
    static void buildGroupedDrafts(const IcdDocument& doc,
                                   const QList<IcdTableGroup>& groups,
                                   QList<IcdMessageDraft>& drafts,
                                   QStringList& globalWarnings);

    // Structural auto-grouping. Among the selected tables (taken in document order),
    // flags each table that looks like a continuation of the one before it (same
    // column count, document-adjacent, blank or "...(cont.)" heading) as a child of
    // that table. Fills parentOf so parentOf[t] is t's parent table index (t itself
    // when it is a standalone/parent table). Purely structural and deterministic;
    // offsets are not consulted here (they are resolved later in buildGroupedDrafts).
    static void suggestContinuationGroups(const IcdDocument& doc,
                                          const QList<int>& selectedTableIndices,
                                          QHash<int, int>& parentOf);

    // Heuristic auto-detection. Inspects one table's *contents* (sampling the data
    // rows, not just the header text) and fills the mapping's header-row index,
    // offset base and role columns as a best guess. Column-order independent, so it
    // works whether the ICD lists byte-first or name-first. Only assigns a role when
    // reasonably confident; unsure roles are left at -1. The profile's non-mapping
    // fields (nameSource, defaultPort, autoPayloadLength, customNamePrefix) are
    // preserved. Deterministic and fully offline — no AI/ML, no network.
    static void suggestMapping(const IcdRawTable& table, IcdMappingProfile& profile);

    // Mapping-profile persistence (named JSON files under AppDataLocation).
    static QString profilesDirectory();
    static QStringList availableProfiles();
    static bool saveProfile(const IcdMappingProfile& profile, QString& errorMessage);
    static bool loadProfile(const QString& profileName, IcdMappingProfile& profile, QString& errorMessage);

    // JSON round-trip (exposed for testing / reuse).
    static QString profileToJson(const IcdMappingProfile& profile);
    static bool profileFromJson(const QString& jsonText, IcdMappingProfile& profile, QString& errorMessage);
};

#endif // ICDDOCXIMPORTER_H
