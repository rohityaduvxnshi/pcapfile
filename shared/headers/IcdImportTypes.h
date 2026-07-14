#ifndef ICDIMPORTTYPES_H
#define ICDIMPORTTYPES_H

// ICD .docx import — data-only model.
//
// A Word .docx is a ZIP of XML. Stage 1 (IcdDocxImporter::extract) walks
// word/document.xml deterministically and produces a faithful structural model
// of every table found (IcdDocument). Stage 2 applies a user-declared column
// mapping (IcdMappingProfile) to selected tables and produces message drafts
// (IcdMessageDraft). Stage 1 never guesses; all interpretation is driven by the
// profile the user confirms in the dialog.

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

    // Page placement, derived from Word's saved pagination markers
    // (<w:lastRenderedPageBreak/>, <w:br w:type="page"/>, <w:pageBreakBefore/>).
    // pageNumber is the page the table STARTS on (1-based); tableOnPage is the
    // table's ordinal among tables starting on that page (1-based). When a
    // document carries no pagination markers every table reports page 1.
    int pageNumber;
    int tableOnPage;

    IcdRawTable()
        : columnCount(0),
          pageNumber(1),
          tableOnPage(1)
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
    CustomPrefix                    // customNamePrefix + "_" + running index in legacy builder
};

// A reusable column mapping. Saved/loaded as JSON so repeat imports of a
// same-shaped ICD are one click. Column roles are stored as 0-based column
// indices into each table's header row; -1 means "not mapped".
//
// For ICD review imports, Name / ByteOffset / DataType / Length may all be left
// unmapped. The loose review builder carries the affected field property as an
// empty editable cell instead of dropping the whole row.
struct IcdMappingProfile
{
    QString profileName;
    int headerRowIndex;             // which row in each table holds the column headers
    int offsetBase;                 // 0 = ICD offsets are 0-based, 1 = 1-based

    int colName;                    // optional (-1 = leave field name empty in review)
    int colByteOffset;              // optional (-1 = leave byte offset empty in review)
    int colDataType;                // optional (-1 = leave data type empty in review)
    int colLength;                  // optional (-1 = leave length empty in review)
    int colResolution;              // optional (-1 = leave resolution empty/default 1 on accept)
    int colResolutionExpr;          // optional (-1 = "1")

    int nameSource;                 // int(IcdNameSource)
    QString customNamePrefix;       // used when nameSource == CustomPrefix
    int defaultPort;                // port assigned to every drafted message
    bool autoPayloadLength;         // compute payload length from field extents when possible

    // Generic structure helpers (all default to "off" so behaviour is unchanged):
    int colDescription;             // optional description/range column for enum decoders (-1 = none)
    bool autoOffsetFromSize;        // ignore the offset column; derive offsets cumulatively from size
    int offsetStartByte;            // 1-based start byte used when autoOffsetFromSize is on

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
          autoPayloadLength(true),
          colDescription(-1),
          autoOffsetFromSize(false),
          offsetStartByte(1)
    {
    }
};

// A group of one or more tables that together form a single message. A Word ICD
// often splits one logical message across several page-spanning tables; grouping
// stitches them back together. tableIndices is parent-first
// ([parent, child1, child2, ...]); the parent's mapping/identity governs the whole
// group. A standalone (un-merged) table is simply a group of size 1.
struct IcdTableGroup
{
    IcdMappingProfile mapping;      // the parent's column mapping + message identity
    QList<int> tableIndices;        // [parent, children...] in build/concatenation order

    IcdTableGroup()
    {
    }
};

// One field row as shown in the ICD review tree. Strings are deliberately kept
// as strings so missing or invalid cells can remain blank/editable until OK.
struct IcdFieldDraftRow
{
    QString name;
    QString byteOffsetText;         // UI 1-based byte offset text, may be empty
    QString lengthText;             // byte length text, may be empty
    QString dataTypeText;           // FieldTypeLabels label, may be empty
    QString resolutionText;         // may be empty; OK defaults empty to 1
    QString resolutionExpression;   // hidden expression, default "1"
    QString descriptionText;        // raw description/range cell (for auto enum decoders)
    QString bitRulesJson;           // auto-derived BitfieldDecoder JSON (may be empty)
    int sourceTableIndex;
    int sourceRowIndex;

    IcdFieldDraftRow()
        : resolutionExpression("1"),
          sourceTableIndex(-1),
          sourceRowIndex(-1)
    {
    }
};

// A drafted message produced by applying a profile to one table/group. Carries
// per-row review strings and warnings so the dialog can surface incomplete rows
// without silently dropping them.
struct IcdMessageDraft{
    MessageDefinition message;      // name / port / payload length / fields (HEX format)
    QList<IcdFieldDraftRow> fieldRows; // editable review rows; preferred by IcdImportDialog
    QStringList warnings;           // human-readable notes about this table/group
    int sourceTableIndex;

    IcdMessageDraft()
        : sourceTableIndex(-1)
    {
    }
};

#endif // ICDIMPORTTYPES_H
