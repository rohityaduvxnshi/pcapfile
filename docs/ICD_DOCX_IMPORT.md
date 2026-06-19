# ICD (.docx) Import

Auto-define messages and fields by importing a Word **`.docx`** Interface Control
Document, instead of typing every field by hand. Available from **File → Import
ICD (.docx)…** (Ctrl+I) in both **File mode** and **Live mode**.

The goal is to remove the most tedious part of using the tool — entering field
offsets/types/lengths one row at a time — while keeping a strict, **no-guessing**
pipeline: the document is parsed deterministically, *you* tell the importer what
each column means, and nothing is written until you review and tick it.

---

## How it works (three stages)

1. **Extract.** The `.docx` (a ZIP of XML) is opened with Qt's built-in
   `QZipReader` and `word/document.xml` is parsed with `QXmlStreamReader`. Every
   Word **table** is read into a grid of cell strings, along with the nearest
   heading/paragraph text above it. This stage is fully offline and deterministic
   — no network, no OCR, no layout guessing.

2. **Map.** Because real ICDs use many different table layouts, you declare what
   the columns mean — once. In the import dialog you:
   - tick which tables are field tables (legend/revision/TOC tables stay unticked),
   - pick the **header row index** and whether offsets are **0-based or 1-based**,
   - map each field attribute to a column: **Name, ByteOffset, DataType**
     (required) and optionally **Length, Resolution, Resolution Expression**,
   - choose where the **message name** comes from (the heading above each table,
     or a custom prefix + number) and a **default port**.

   The dialog pre-selects likely columns by matching the header text (e.g. a
   column titled “Byte Offset” is offered for ByteOffset), but every choice is
   yours to override. Mappings can be **saved as named profiles** and reloaded, so
   the next ICD with the same layout is a one-click import.

3. **Review & commit.** Click **Build / Preview** to apply the mapping. A tree
   shows every drafted message and its fields with checkboxes; a warnings panel
   lists anything the parser flagged (unknown type labels, non-numeric offsets,
   merged cells, duplicate names, …). Tick what you want, optionally edit a
   message’s port / payload length / optional-header inline, then **OK**. Every
   kept message and field passes the same `InputValidator` gate as manual entry
   before it is written; if anything fails, the errors are listed and nothing is
   committed.

---

## DataType labels

The DataType column is resolved with the **same** label table as the CSV
importer (`FieldCsvCodec::dataTypeFromLabel`), so all of these work
(case-insensitive): `Raw Unsigned BE`/`raw`, `bool`, `uchar`/`uint8`,
`char`/`int8`, `ushort`/`uint16`, `short`/`int16`, `uint`/`uint32`,
`int`/`int32`, `ulong`/`uint64`, `long`/`int64`, `float`, `double`,
`string`/`text`. A cell whose type is not recognized is reported in the warnings
and that **row is skipped** (never guessed).

## Where imported messages land

- **File mode, port filter:** the currently selected port-filter row (or row 1).
  In port mode the row’s port is authoritative, so each imported message takes
  that row’s port.
- **File mode, header filter:** the first header-filter row’s length filters.
- **Live mode:** the live length-filter set (the *Configured Messages (Live)*
  table).

After import you can still open the normal per-message field editor (the
**Configure Fields** button) to tweak anything or attach bit / conditional
decoders — those are intentionally **never** created by the import.

---

## Known limitations (current version)

- **One mapping per import.** A single column mapping is applied to all ticked
  tables, so they should share a layout. For an ICD that mixes layouts, import the
  matching tables in one pass (save a profile), then the others in another.
- **Message metadata.** Port defaults to the mapping’s default port and payload
  length is computed from the field extents (toggleable). Per-message port /
  length / optional-header can be edited in the review tree before committing, or
  later via the existing dialogs.
- **Bit/conditional decoders are not imported** — add them in the field editor.
- **Tables only.** Field definitions must be in Word tables. Image/scanned tables
  and legacy `.doc` files are not supported.
- **Merged cells** (`gridSpan`/`vMerge`) are surfaced as-is; horizontally merged
  cells are padded so column indices line up, and anything unusual shows in the
  warnings panel for you to resolve before committing.

---

## For maintainers

| File | Role |
|------|------|
| `headers/IcdImportTypes.h` | Data-only model: `IcdRawTable`, `IcdDocument`, `IcdMappingProfile`, `IcdMessageDraft`, `IcdNameSource`. |
| `headers/IcdDocxImporter.h`, `sources/IcdDocxImporter.cpp` | Pure parser: `extract()` (QZipReader + QXmlStreamReader), `buildDrafts()` (apply mapping → drafts), profile JSON save/load. No widgets. |
| `headers/IcdImportDialog.h`, `sources/IcdImportDialog.cpp` | Review/selection dialog (built programmatically). |
| `sources/MainWindow.cpp` | `onImportIcdClicked()` + `applyImportedMessages()` (appended at end of file); one `connect` in the ctor; `actImportIcd` action in `forms/MainWindow.ui`. |

The unzip backend is isolated to `IcdDocxImporter::extract()`. Swapping QZipReader
for vendored QuaZip/minizip later means changing only that one function. The build
adds `QT += gui-private` for the `QZipReader` header.
