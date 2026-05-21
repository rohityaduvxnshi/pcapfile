# Plan: Session Persistence + CSV Bulk-Import of Field Definitions

## Context

The user built **PcapUdpExtractor** (Qt **5.10** / C++) to extract structured data from UDP-bearing pcap files. The pain points blocking further productivity:

1. **All configuration is volatile.** Filter setup, port → message → field definitions, bitfield rules, conditional decoders, live-capture fields — everything lives only in `MainWindow`'s member variables. Closing the app (even accidentally) wipes hours of work. There is currently no `QSettings` use, no project file, and `closeEvent` only stops live capture ([MainWindow.cpp:245-250](sources/MainWindow.cpp#L245-L250)).
2. **Defining fields is slow.** For a pcap with many message types and large payloads, the user (and their teammates) spend most of their time keying in field rows in `FieldConfigurationDialog`. Bit / conditional mapping is genuinely precise work and should stay manual, but the basic field schema (name / offset / type / length / resolution) often already exists in an ICD spreadsheet — re-typing it is pure waste.

This plan delivers two features that directly address those pains while reusing the JSON serialization the project already has for bit/conditional decoders.

### Hard constraints (apply to both features)

- **Branch:** all work lands on a **new branch `version8_automationand_selfsave_v1`** branched from `main`. `main` is not touched. No commits, no edits on `main` at any point.
- **Qt 5.10 only.** No Qt 6 APIs. All required classes (`QJsonDocument`, `QJsonObject`, `QJsonArray`, `QFile`, `QFileInfo`, `QStandardPaths`, `QSettings`, `QFileDialog`, `QMessageBox`) exist in Qt 5.10 — no compatibility concerns.
- **No external libraries.** JSON via `QJsonDocument` (already used by `BitfieldDecoder`). CSV parsing written in-house mirroring `CsvExporter`'s quoting rules. No nlohmann/json, no rapidjson, no Boost.
- **Strictly additive — do NOT modify existing logic.** No existing function's behavior changes. No existing struct member is renamed or removed. The existing extraction, filtering, decoding, preview, and CSV-export paths run identically with both features merged in. Every touch point listed below is either a *new file*, a *new slot*, a *new connection*, or an *append* inside an existing function (never a rewrite of its body).

---

## Feature 1: Project File (Auto-Save + Restore)

### Decisions

- **File format: JSON**, single file per project, called `<pcap-basename>.pcproj.json` placed next to the source pcap. JSON is chosen because:
  - Qt 5.10 ships `QJsonDocument` (zero new deps), already used by `BitfieldDecoder::rulesToJson` ([BitfieldDecoder.cpp:28-67](sources/BitfieldDecoder.cpp#L28-L67)) and `ConditionalBitfieldDecoder::toJson`. We **reuse those exact functions verbatim** — no parallel serialization to maintain, no risk of behavioural drift from the existing bit/conditional logic.
  - Human-readable; user can hand-edit if something goes wrong, diff in git, share with teammates.
  - Versionable via a `"version": 1` field at the top so future schema changes don't break old files.
- **Sidecar location**: same folder as the pcap, derived from `ui->txtFilePath->text()` via `QFileInfo`. Falls back to `QStandardPaths::AppDataLocation` keyed by hashed pcap path when the pcap folder is read-only (e.g. network share).
- **Save trigger**: **explicit save only** (Ctrl+S / `File → Save Project`) plus an unconditional final save in `closeEvent`. No background autosave timer — keeps disk activity minimal, keeps mental model closer to traditional editors. A modified-marker (`*` in window title) tells the user when unsaved changes exist so the close-save is never a surprise.
- **Restore UX**:
  - On `onBrowseClicked` selecting a pcap, if a matching `.pcproj.json` exists → modal prompt: "Saved progress for this capture was found (saved <relative time>). Restore?" with **Restore / Discard / Cancel**.
  - On app startup, also remember the **last-active project path** via `QSettings` (`organizationName`/`applicationName` set in `main.cpp`). Prompt: "Resume your previous session?" — lets the user recover from a crash even before they re-pick the pcap.
- **Explicit menu actions**: add a `File` menu with **Open Project…**, **Save Project (Ctrl+S)**, **Save Project As…**. The current menubar is empty ([MainWindow.ui](forms/MainWindow.ui)) so we add one. (CSV import/export lives inside the field-config dialog, not the main menu — see Feature 2.)

### Schema (v1)

```json
{
  "version": 1,
  "savedAt": "2026-05-21T14:32:11Z",
  "appVersion": "v7",
  "pcapPath": "C:/captures/flight_log.pcap",
  "inputMode": "file" | "live",
  "filterMode": "port" | "header",
  "filterCount": 3,
  "filterConfig": {
      "commonPort": 5000,
      "filters": [ { "label": "...", "port": 5001, "headerHex": "AB CD" }, ... ]
  },
  "portMessages": [
      {
        "filterRow": 0,
        "messages": [
          {
            "messageName": "Telemetry",
            "port": 5001,
            "payloadLengthBytes": 64,
            "fields": [ <FieldDefinition JSON>, ... ]
          }
        ]
      }
  ],
  "headerFields": [ <FieldDefinition JSON>, ... ],
  "live": {
      "fields": [ <FieldDefinition JSON>, ... ],
      "filterConfig": { ... }
  }
}
```

Where `<FieldDefinition JSON>` is:
```json
{ "name": "...", "byteOffset": 4, "byteOffsetCorrect": 4,
  "length": 2, "dataType": "Uint16",
  "resolution": 0.01, "resolutionExpression": "raw*0.01",
  "bitfieldDecoder": <BitfieldDecoder::rulesToJson output>,
  "conditionalDecoder": <ConditionalBitfieldDecoder::toJson output> }
```

`dataType` is serialized as a string label (not int) so the schema survives enum reordering.

### Critical files to add / modify

- **New file:** [headers/ProjectFile.h](headers/ProjectFile.h), [sources/ProjectFile.cpp](sources/ProjectFile.cpp) — single class `ProjectFile` with static `save(const ProjectState&, const QString& path, QString& err)` / `load(...)`. `ProjectState` is a small struct gathering every `MainWindow` member to be persisted. **No existing struct or class is touched.**
- **Append to** [headers/MainWindow.h](headers/MainWindow.h) (additive — existing members, methods, signals untouched):
  - New members: `QString m_projectPath`, `bool m_isDirty`.
  - New private slots (declared at the end of the existing `private slots:` block): `onSaveProject()`, `onSaveProjectAs()`, `onOpenProject()`, `markDirty()`.
- **Append to** [sources/MainWindow.cpp](sources/MainWindow.cpp) (additive — existing slots' bodies untouched):
  - In the constructor (after current setup), add `connect(...)` calls to bind `markDirty()` to filter widget signals and dialog `accepted()` signals. Initialise `m_isDirty = false`, `m_projectPath = ""`.
  - **Append** to `closeEvent` after the existing two lines: if dirty, prompt Save/Discard/Cancel; on Save, write project file (resolving a path via Save-As if `m_projectPath` is empty). The existing `stopLiveCapture()` + base-class call run first, unchanged. Last-active path persisted via `QSettings`.
  - **Append** to `onBrowseClicked` after the existing body: if the chosen path has a sibling `<name>.pcproj.json`, show the Restore/Discard/Cancel prompt. The existing file-selection logic runs first, unchanged.
  - New slot bodies for `onSaveProject` / `onSaveProjectAs` / `onOpenProject` / `markDirty` added at the end of the file.
- **Append to** [sources/main.cpp](sources/main.cpp) — add `QCoreApplication::setOrganizationName(...)` / `setApplicationName(...)` calls before constructing the main window so `QSettings` has a consistent backing. Pre-existing main flow untouched.
- **Modify** [forms/MainWindow.ui](forms/MainWindow.ui) — populate the existing empty `menuBar` with a `File` menu (**Open Project…**, **Save Project** (Ctrl+S), **Save Project As…**). No existing widget removed or repositioned.
- **Reuse verbatim (zero changes):** `BitfieldDecoder::rulesToJson` / `rulesFromJson` ([BitfieldDecoder.cpp:28-137](sources/BitfieldDecoder.cpp#L28-L137)), `ConditionalBitfieldDecoder::toJson` / `fromJson`. This keeps bit/conditional decoding behaviourally identical.

### Safety

- Atomic save: write to `<file>.tmp` then `QFile::rename` — never leave a half-written project file.
- Backup-on-load: when restoring, copy the existing file to `<file>.bak` first; if load fails midway the user still has their work.
- Path-mismatch warning: if the project file's stored `pcapPath` ≠ the pcap currently open, warn before merging.

---

## Feature 2: CSV Bulk-Import of Field Definitions

### Decisions

- **Scope per import**: one **message** (or the standalone header / live fields list) at a time. Multi-message import is tempting but couples the format to filter-row ordering and creates ambiguity when ports collide. Per-message keeps the mental model identical to the existing UI: open the dialog for a message, click *Import CSV…*, pick a file, fields populate.
- **Bitfield / conditional decoders stay manual** — explicitly required by the user. The CSV format simply does not carry those columns. After import, the *Bit Decoder* / *Cond. Decoder* cells remain empty and the user fills them via the existing dialogs.
- **Column schema (header row required, case-insensitive, order-flexible):**

  | Column                  | Required | Type                   | Notes                                       |
  |-------------------------|----------|------------------------|---------------------------------------------|
  | `Name`                  | yes      | string                 | non-empty, ≤ 64 chars                       |
  | `ByteOffset`            | yes      | int ≥ 0                | validated against `payloadLengthBytes`      |
  | `DataType`              | yes      | enum label             | one of the 12 names in `FieldDataType`      |
  | `Length`                | optional | int ≥ 1                | if blank → natural length for the type; required for `RawUnsignedBE` |
  | `Resolution`            | optional | double                 | default `1.0`                               |
  | `ResolutionExpression`  | optional | string                 | default `"1"`                               |

  Blank columns or rows are skipped. Lines beginning with `#` are treated as comments.

- **Merge behaviour**: ask the user up-front in the import dialog — **Replace existing fields** (default) or **Append**. Append validates for overlapping byte ranges.
- **Provide an Export-to-CSV counterpart**: same schema, written via existing `CsvExporter` patterns ([CsvExporter.h](headers/CsvExporter.h)). Round-trip is the killer feature — define once, share across pcaps, edit in Excel, re-import.
- **Template generator**: a *Download CSV Template…* item that writes an empty CSV with just the header row + a couple of comment-line examples, so first-time users don't have to guess the format.

### Critical files to add / modify

- **New file:** [headers/FieldCsvCodec.h](headers/FieldCsvCodec.h), [sources/FieldCsvCodec.cpp](sources/FieldCsvCodec.cpp) — pure free functions, no existing code touched:
  - `bool importFromCsv(const QString& path, int payloadLengthBytes, QList<FieldDefinition>& out, QStringList& warnings, QString& err)`
  - `bool exportToCsv(const QString& path, const QList<FieldDefinition>& fields, QString& err)`
  - `QString dataTypeToLabel(FieldDataType)` / `bool dataTypeFromLabel(const QString&, FieldDataType&)`
  - CSV parsing written in-house mirroring the same quoting rules as `CsvExporter` (RFC-4180-ish: `"…"`, doubled `""` inside quoted fields, comma separators, CRLF or LF). No external CSV library.
- **Append to** [headers/FieldConfigurationDialog.h](headers/FieldConfigurationDialog.h) (additive — existing slots and members untouched):
  - New private slots: `onImportCsvClicked()`, `onExportCsvClicked()`, `onTemplateCsvClicked()`.
- **Append to** [sources/FieldConfigurationDialog.cpp](sources/FieldConfigurationDialog.cpp):
  - New slot bodies added at end of file. They call `FieldCsvCodec::*`, then on success call the existing `refreshFieldTable()` (or the same code path that `setFields(...)` already uses). The existing Add/Edit/Remove/Save flow is untouched and continues to operate identically.
- **Modify** [forms/FieldConfigurationDialog.ui](forms/FieldConfigurationDialog.ui) — add three buttons (**Import CSV…**, **Export CSV…**, **Template…**) alongside the existing button row. Existing buttons keep their positions and signal bindings.
- **Reuse**: the `FieldDefinition` constructor defaults handle all optional CSV columns gracefully (no struct change needed).

### Validation rules surfaced to the user

- Unknown `DataType` label → error with the list of accepted labels.
- Negative offset / negative length → error.
- `byteOffset + length > payloadLengthBytes` → error with row number.
- Duplicate `Name` → error with row number.
- Overlapping byte ranges (only flagged in *Append* mode) → warning (does not block).

All errors collected and shown in one `QMessageBox`, not one popup per line.

---

## Verification

After implementing both features, manually verify:

### Feature 1
1. Open a pcap, define a filter with 2 ports, define 3 fields with one bitfield rule and one conditional rule on each. Close the app cold (Task Manager kill).
2. Re-open the app → "Resume previous session?" prompt fires → click yes → every field, rule and filter is intact, including bit positions and conditional profiles.
3. Pick a different pcap that has a sibling `.pcproj.json` → prompt offers to restore that project.
4. Move the pcap to a read-only network share, repeat — sidecar should fall back to `AppDataLocation` and still round-trip.
5. Hand-edit the JSON, break it deliberately, reopen → user sees a clear error and `.bak` exists.

### Feature 2
1. Inside `FieldConfigurationDialog`, click *Template…* → save → open in Excel — header row + sample comments visible.
2. Fill 10 fields in Excel, save as CSV, *Import CSV…* with *Replace* → table populated, dialog accepts, message round-trips through save/load (Feature 1).
3. Import a CSV with one invalid `DataType` → error dialog lists all bad rows at once, table unchanged.
4. *Export CSV…* a manually-built message, then *Import CSV…* the exported file → identical field list.
5. After CSV import, manually add a bitfield rule to one field → confirm the CSV path doesn't clobber it on the next save.

### Build
- `qmake && make` against **Qt 5.10** (or run via Qt Creator on the project's existing kit) — verify no new warnings, no Qt6-only API usage slipped in.
- Confirm `PcapUdpExtractor.pro` is unchanged except for the three new source/header files appended to `SOURCES` / `HEADERS` and the new `.ui` button additions picked up automatically.

### No-regression check (critical)
- With both features disabled in practice (don't open/save any project, don't import any CSV), the app must behave **bit-for-bit identically** to the current build:
  - File-mode extraction produces the same CSV.
  - Live-mode capture, filter matching, bit/conditional decoding all unchanged.
  - The existing `addfe50` user-defined length flow still works.
- Spot-check: `git diff main -- sources/MainWindow.cpp sources/FieldConfigurationDialog.cpp` should show **only appended slot bodies and new `connect()` calls inside the constructor + appended branches at the end of `closeEvent` / `onBrowseClicked`**. No existing line's behaviour should have been rewritten.
