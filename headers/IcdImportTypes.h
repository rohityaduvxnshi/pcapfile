#ifndef ICDIMPORTTYPES_H
#define ICDIMPORTTYPES_H

// ICD .docx import — data-only model.
//
// A Word .docx is a ZIP of XML. Stage 1 (IcdDocxImporter::extract) walks
// word/document.xml deterministically and produces a faithful structural model
// of every table found (IcdDocument). Stage 2 (IcdDocxImporter::buildDrafts)
// applies a user-declared column mapping (IcdMappingProfile) to selected tables
// and produces message drafts (IcdMessageDraft). No guessing happens in Stage 1;
// all interpretation is driven by the profile the user confirms in the dialog.

#include "MessageDefinition.h"

#include <QList>
#include <QString>
#include <QStringList>

// One table extracted from the document, as a grid of trimmed cell strings.
struct IcdRawTable
{
    QString precedingHeading;       // nearest non-empty paragraph text before the table
    QList<QStringList> rows;        // rows x cells (cell text already assembled/merged)
    int columnCount;                // max column count across all rows

    IcdRawTable()
        : columnCount(0)
    {
    }
};

// The whole document's structural extraction.
struct IcdDocument
{
    QList<IcdRawTable> tables;
};

// Where a message's name comes from.
enum class IcdNameSource
{
    PrecedingHeading = 0,           // the heading/paragraph immediately above the table
    CustomPrefix                    // customNamePrefix + "_" + running index
};

// A reusable column mapping. Saved/loaded as JSON so repeat imports of a
// same-shaped ICD are one click. Column roles are stored as 0-based column
// indices into each table's header row; -1 means "not mapped".
struct IcdMappingProfile
{
    QString profileName;
    int headerRowIndex;             // which row in each table holds the column headers
    int offsetBase;                 // 0 = ICD offsets are 0-based, 1 = 1-based

    int colName;                    // required
    int colByteOffset;              // required
    int colDataType;                // required
    int colLength;                  // optional (-1 = use natural length)
    int colResolution;              // optional (-1 = 1.0)
    int colResolutionExpr;          // optional (-1 = "1")

    int nameSource;                 // int(IcdNameSource)
    QString customNamePrefix;       // used when nameSource == CustomPrefix
    int defaultPort;                // port assigned to every drafted message
    bool autoPayloadLength;         // compute payload length from field extents

    IcdMappingProfile()
        : headerRowIndex(0),
          offsetBase(0),
          colName(-1),
          colByteOffset(-1),
          colDataType(-1),
          colLength(-1),
          colResolution(-1),
          colResolutionExpr(-1),
          nameSource(0),
          customNamePrefix("Message"),
          defaultPort(5000),
          autoPayloadLength(true)
    {
    }
};

// A drafted message produced by applying a profile to one table. Carries the
// per-table warnings so the dialog can surface everything the parser flagged.
struct IcdMessageDraft
{
    MessageDefinition message;      // name / port / payload length / fields (HEX format)
    QStringList warnings;           // human-readable notes about this table
    int sourceTableIndex;

    IcdMessageDraft()
        : sourceTableIndex(-1)
    {
    }
};

#endif // ICDIMPORTTYPES_H
