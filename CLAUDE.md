# CLAUDE.md — PcapUdpExtractor

This file is the project memory for Claude Code. It captures everything needed to work on the codebase without re-exploring it from scratch.

**Maintenance rule:** Update this file in the same change that modifies architecture, the data model, conventions, branch state, or build instructions. If you find yourself running `Grep` / `Read` to re-learn something about the project, that knowledge belongs here once you've found it.

---

## 1. What this project is

**PcapUdpExtractor** — a Qt 5.10 / C++11 desktop GUI app (Windows, mingw53_32 / msvc kits) that:

1. Opens `.pcap` / `.pcapng` files, parses UDP packets, and exports the structured payload fields the user defines into CSV.
2. Provides a **Live UDP** mode that listens on a socket and streams the same field extraction to CSV in real time.
3. Lets the user define fields with offsets, types, lengths, resolution expressions, **bitfield decoders**, and **conditional bitfield decoders** (whose behaviour depends on the value of a *controller* field).

Build system: qmake (`.pro` file).

---

## 2. Hard project constraints — DO NOT VIOLATE

1. **Qt 5.10 only.** Verified against Qt 5.10.1 / mingw53_32. No Qt 6 APIs (no `qsizetype`, no `Qt::SplitBehavior`, no `QPromise`, no `QFuture::then`, no Qt 6 `setCodec` removal — code is guarded with `#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)`).
2. **No external libraries.** No nlohmann/json, rapidjson, Boost, fmt — use Qt builtins only (`QJsonDocument`, `QFile`, `QTextStream`, `QSet`, `QCryptographicHash`, `QStandardPaths`, etc.).
3. **Strictly additive changes.** Never modify the *behaviour* of an existing function. New work = new files, new slots, new connections, new menu items, OR a single appended line at the end of an existing function's body (call site only, no rewrites). Self-check: *"Could a developer revert this commit and have the app work identically to before?"* If no, the change isn't additive.
4. **Do not commit unless explicitly asked.** The user is particular about this.

---

## 3. Build

```powershell
$env:PATH = 'D:\qt\5.10.1\mingw53_32\bin;D:\qt\Tools\mingw530_32\bin;' + $env:PATH
New-Item -ItemType Directory -Force build | Out-Null
Set-Location build
qmake ..\PcapUdpExtractor.pro
mingw32-make -j4
# Output: build\release\PcapUdpExtractor.exe (~500 KB)
```

Alternative kits installed: `msvc2013_64`, `msvc2015`, `android_armv7`, `android_x86` — all under `D:\qt\5.10.1\`. mingw is the verified path.

`build/` is qmake-generated and should be gitignored (it currently is *not* — `build/`, `.qmake.stash`, `object_script.*`, `release/` show as untracked. Worth adding a `.gitignore` when convenient).

---

## 4. Repository layout

```
PcapUdpExtractor.pro      qmake build file — every SOURCES / HEADERS / FORMS listed explicitly
sources/*.cpp             implementation files
headers/*.h               public headers (forward-declare Qt classes; keep includes light)
forms/*.ui                Qt Designer XML
build/                    qmake-generated (untracked)
CLAUDE.md                 this file
```

Working directory in tools: `c:\GitHub\pcapfile`.

---

## 5. Core data model

### `FieldDefinition` — [headers/AppTypes.h:106](headers/AppTypes.h#L106)
A single decoded field within a UDP payload.

| Field | Type | Notes |
|---|---|---|
| `name` | `QString` | non-empty, ≤ 64 chars by convention |
| `byteOffset` | `int` | **1-based** (matches the UI dialog) |
| `byteOffsetcorrect` | `int` | **0-based**, MUST equal `byteOffset - 1` everywhere |
| `length` | `int` | bytes; user-definable for any type since commit `addfe50` |
| `dataType` | `FieldDataType` enum class | 12 values, see below |
| `resolution` | `double` | numeric scale, default `1.0` |
| `resolutionExpression` | `QString` | text formula (e.g. `raw*0.01`), default `"1"` |
| `hasBitfieldDecoder` | `bool` | + `QList<BitDecodeRule> bitDecodeRules` |
| `hasConditionalBitfieldDecoder` | `bool` | + `ConditionalBitfieldDecoderConfig conditionalDecoder` |

### `FieldDataType` enum class — [headers/AppTypes.h:60](headers/AppTypes.h#L60)
`RawUnsignedBE, Uint8, Int8, Uint16, Int16, Uint32, Int32, Uint64, Int64, Float32, Float64, Bool, String`. Natural length via `fieldDataTypeNaturalLength()`; `RawUnsignedBE` and `String` return 0 (length user-provided).

**String** (v11): variable-length UTF-8 text. Length is user-defined and is **not capped at 8 bytes** like the integer types are — strings can span an arbitrary slice of the payload. Decoding reads `length` bytes from the payload, trims trailing NUL bytes, and decodes as UTF-8. String fields cannot have bit/conditional decoders (the existing dialogs gate on `fieldLength <= 8`).

### `BitDecodeRule` — [headers/AppTypes.h:11](headers/AppTypes.h#L11)
- `label`, `bitPositions` (`QList<int>`), `valueMeanings` (`QMap<quint64, QString>`), `reserved`, `unknownBehavior` ∈ `{"UNKNOWN", "BLANK", "RAW_BINARY"}`, `enabled`.

### `ConditionalBitfieldDecoderConfig` — [headers/AppTypes.h:48](headers/AppTypes.h#L48)
- `controllerFieldName`, `unknownBehavior` ∈ `{"UNKNOWN_CONTROLLER", "BLANK"}`, `profiles` (`QList<ConditionalBitDecodeProfile>`).
- Each profile has `profileName`, `controllerValue` (quint64), `bitDecodeRules`, and `exclusionRules` (mutual-exclusivity constraints on bits).

### `MessageDefinition` — [headers/MessageDefinition.h:10](headers/MessageDefinition.h#L10)
A named message scoped to a UDP port: `messageName`, `port` (quint16), `payloadLengthBytes`, `fields` (`QList<FieldDefinition>`).

### `FilterConfiguration` — [headers/FilterTypes.h:25](headers/FilterTypes.h#L25)
- `mode` — `FILTER_MODE_PORT = 0` or `FILTER_MODE_HEADER = 1`
- `commonPort`
- `filters` (`QList<MessageFilter>`) — each has `label`, `port`, `header` (QByteArray).

---

## 6. Key files & responsibilities

| File | Role |
|------|------|
| [sources/main.cpp](sources/main.cpp) | Entry point. Sets `setOrganizationName`/`setApplicationName` (for `QSettings`), creates `MainWindow`. |
| [headers/MainWindow.h](headers/MainWindow.h), [sources/MainWindow.cpp](sources/MainWindow.cpp) | Top-level GUI. Holds **all session state** as member variables. |
| [headers/FieldConfigurationDialog.h](headers/FieldConfigurationDialog.h), [sources/FieldConfigurationDialog.cpp](sources/FieldConfigurationDialog.cpp) | Per-message field-table editor. Buttons: Add / Edit / Remove / Bitfield Decoder / Conditional Decoder / **Import CSV / Export CSV / Template** (v8). |
| [headers/BitfieldDecoder.h](headers/BitfieldDecoder.h), [sources/BitfieldDecoder.cpp](sources/BitfieldDecoder.cpp) | Bit-rule JSON round-trip (`rulesToJson` / `rulesFromJson`), bit-position parsing, rule validation, per-row decode. |
| [headers/ConditionalBitfieldDecoder.h](headers/ConditionalBitfieldDecoder.h), [sources/ConditionalBitfieldDecoder.cpp](sources/ConditionalBitfieldDecoder.cpp) | Same idea for conditional decoders (`toJson` / `fromJson`). |
| `headers/BitfieldRuleDialog.h`, `headers/BitfieldDecoderDialog.h` | UI for editing bit rules. |
| `headers/ConditionalProfileDialog.h`, `headers/ConditionalBitfieldDecoderDialog.h` | UI for editing conditional profiles. |
| `headers/MessageDefinitionDialog.h` | UI for editing a single message definition. |
| `headers/MessageLengthFilterDialog.h` | Per-port "manage length filters" dialog. |
| [headers/CsvExporter.h](headers/CsvExporter.h), [sources/CsvExporter.cpp](sources/CsvExporter.cpp) | Offline CSV writer (RFC-4180 quoting). Uses a reusable `QByteArray` buffer in `appendEscapedCellUtf8` — pattern to mimic for any new CSV writer. |
| `headers/CsvStreamWriter.h`, `sources/CsvStreamWriter.cpp` | Streaming CSV writer used by live mode. |
| `headers/PcapFileReader.h`, `sources/PcapFileReader.cpp` | Reads pcap + pcapng. |
| `headers/UdpPacketParser.h`, `sources/UdpPacketParser.cpp` | Parses raw packets → `ParsedUdpPacket`. |
| `headers/ExtractionEngine.h`, `sources/ExtractionEngine.cpp` | Orchestrates file-mode extraction loop. |
| `headers/LiveUdpReceiver.h`, `sources/LiveUdpReceiver.cpp` | Live UDP socket receiver. |
| `headers/InputValidator.h`, `sources/InputValidator.cpp`, `sources/InputValidator_filters.cpp` | Centralised validation rules for fields, filters, resolution expressions, message definitions. |
| `headers/MathExpressionEvaluator.h`, `sources/MathExpressionEvaluator.cpp` | Evaluates `resolutionExpression` strings (e.g. `raw*0.01`). |
| **[headers/FieldCsvCodec.h](headers/FieldCsvCodec.h), [sources/FieldCsvCodec.cpp](sources/FieldCsvCodec.cpp)** | **v8: CSV bulk import/export of field definitions.** Pure free functions. Bitfield/conditional decoders NEVER serialized. |
| **[headers/ProjectFile.h](headers/ProjectFile.h), [sources/ProjectFile.cpp](sources/ProjectFile.cpp)** | **v8: JSON project file (sidecar to the pcap).** `ProjectState` struct + `ProjectFile::save` / `load` / `sidecarPathFor`. |
| **[headers/BitRuleCsvCodec.h](headers/BitRuleCsvCodec.h), [sources/BitRuleCsvCodec.cpp](sources/BitRuleCsvCodec.cpp)** | **v9: CSV bulk import/export of bit decoder rules.** Pure free functions. Rows grouped by `Label` → one `BitDecodeRule`. Validates via `BitfieldDecoder::validateRules` after parsing. |

---

## 7. State held by MainWindow (volatile unless persisted via ProjectFile)

- `QList<FieldDefinition> m_headerFields` — header/standalone fields
- `QList<QList<MessageDefinition>> m_portMessagesByRow` — per filter-row, messages configured for that port
- `QList<FieldDefinition> m_liveFields` — live-mode fields
- `FilterConfiguration m_liveFilterConfig`
- `QList<QSpinBox*> m_portFilterBoxes`, `QList<QLineEdit*> m_headerFilterBoxes` — dynamically rebuilt by `rebuildFilterInputs()` whenever `spinFilterCount` changes
- Live capture: `LiveUdpReceiver* m_liveReceiver`, `QTimer* m_livePreviewTimer`, `CsvStreamWriter m_liveWriter`, `m_liveRunning`, `m_livePacketsReceived`, `m_livePacketsMatched`, `m_liveShortPackets`
- **v8 addition:** `QString m_projectPath` — path to current `.pcproj.json` sidecar (empty until Save As or sidecar-restore)

---

## 8. Conventions

- **Signals/slots:** old-style string-based `connect(obj, SIGNAL(foo()), this, SLOT(bar()))`. The only new-style functor connect is one lambda inside `FieldConfigurationDialog::setTypeCell`.
- **`byteOffset` is 1-based in the UI**, 0-based internally as `byteOffsetcorrect`. Every conversion uses `byteOffsetcorrect = byteOffset - 1`.
- **Bit / conditional decoder JSON** is stored in the field-name `QTableWidgetItem::UserRole` (bit) and `UserRole+1` (conditional) inside `FieldConfigurationDialog`. Serialise via `BitfieldDecoder::rulesToJson` / `ConditionalBitfieldDecoder::toJson` — never roll your own.
- **Forward-declare Qt classes** in headers (`class QSpinBox;`) to keep includes light.
- **C++ standard:** Qt 5 → `c++11` per the `.pro` file conditional; Qt 6 → `c++17` (we don't ship Qt 6).
- **CSV / JSON encoding:** UTF-8. `QTextStream::setCodec("UTF-8")` is guarded by `#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)` (Qt 6 makes UTF-8 the default).
- **File dialogs** uniformly use `QFileDialog::getOpenFileName` / `getSaveFileName`.

---

## 9. Active branch state

### `main`
Released through commit `15d3c4d` (Merge PR #4 — user-defined length for any data type, v7-optimized refactor before that).

### `version8_automationand_selfsave_v1` (current branch as of v8 work — NOT yet committed)
Adds two features. **Strictly additive.** Build verified: `build/release/PcapUdpExtractor.exe` (~498 KB), zero errors.

**Files (8 modified, 4 new):**

```
NEW:
  headers/FieldCsvCodec.h
  sources/FieldCsvCodec.cpp
  headers/ProjectFile.h
  sources/ProjectFile.cpp

MODIFIED (append-only):
  PcapUdpExtractor.pro                  + 4 lines (new SOURCES/HEADERS entries)
  forms/MainWindow.ui                   + 23 lines (File menu + 3 actions)
  forms/FieldConfigurationDialog.ui     + 3 lines (3 buttons)
  headers/MainWindow.h                  + 12 lines (slots, helpers, m_projectPath)
  headers/FieldConfigurationDialog.h    + 3 lines (3 slots)
  sources/MainWindow.cpp                + 204 lines (slot bodies + helpers + 3 small appends)
  sources/FieldConfigurationDialog.cpp  + 109 lines (slot bodies + 3 connect lines)
  sources/main.cpp                      + 3 lines (org/app name)
```

#### Feature A — Project save/restore (sidecar JSON)
- Format: JSON sidecar `<pcap-basename>.pcproj.json` next to the pcap. Fallback: `QStandardPaths::AppDataLocation` keyed by MD5 hash of the absolute pcap path when the pcap folder is read-only (network share / locked).
- Schema (versioned at `"version": 1`): `appVersion`, `savedAt` (ISO UTC), `pcapPath`, `inputMode` (`"file"`/`"live"`), `filterMode` (`"port"`/`"header"`), `filterCount`, `filterConfig`, `portMessages` (array of `{filterRow, messages}`), `headerFields`, `live{fields, filterConfig}`. Field-level decoder configs round-trip through `BitfieldDecoder::rulesToJson` / `ConditionalBitfieldDecoder::toJson` — no parallel serialization.
- Triggers:
  - **Explicit save:** File menu → Save Project (Ctrl+S) / Save Project As (Ctrl+Shift+S)
  - **Explicit open:** File menu → Open Project (Ctrl+O)
  - **Silent save on close:** `closeEvent` calls `autoSaveProjectOnClose()` after the existing `stopLiveCapture()` line
  - **Restore prompt:** `onBrowseClicked` calls `tryRestoreProjectForPcap()` after the existing `setStatus` line. Shows Restore/Discard/Cancel only if a sidecar exists.
- Atomic write: `<file>.tmp` → rename, with `<file>.bak` keeping the previous version.
- Helper methods on `MainWindow`: `captureProjectState`, `applyProjectState`, `tryRestoreProjectForPcap`, `autoSaveProjectOnClose`.

#### Feature B — CSV field-definition import/export
- Three buttons in `FieldConfigurationDialog`: **Import CSV…**, **Export CSV…**, **Template…**.
- Per-message scope only.
- Columns: `Name, ByteOffset, DataType, Length, Resolution, ResolutionExpression`. Header row is case-insensitive and order-flexible. `#`-prefixed and blank lines are skipped. `Length` is optional for fixed-size data types (defaults to natural length).
- DataType labels accepted: human labels (`Raw Unsigned BE`, `bool`, `uchar`, `char`, `ushort`, `short`, `uint`, `int`, `ulong`, `long`, `float`, `double`) plus enum spellings (`Uint16`, `Int32`, etc.), case-insensitive.
- Bitfield + conditional decoders are **never** imported from CSV (user requirement). Round-trip preserves only the structural columns.
- Import has Replace / Append / Cancel prompt. All row errors collected and shown in one `QMessageBox`; on failure the table is left untouched.

#### Verification status (v8)
- Clean qmake + mingw32-make build on Qt 5.10.1.
- End-to-end UI testing **pending**: launch the exe, verify File menu actions, sidecar prompt on Browse, CSV import/export/template inside the field-config dialog.

### v9 (same branch `version8_automationand_selfsave_v1` — stacked on top of v8)
Bit-mapping bulk import / export — addresses the "click-by-click rule entry" bottleneck once field schemas are in via v8.

**Files added (new):**
```
NEW:
  headers/BitRuleCsvCodec.h
  sources/BitRuleCsvCodec.cpp
```

**Files modified (append-only):**
```
  PcapUdpExtractor.pro                    + 2 lines
  forms/BitfieldDecoderDialog.ui          + 4 buttons (Import CSV / Import JSON / Export / Template)
  headers/BitfieldDecoderDialog.h         + 4 new private slots
  sources/BitfieldDecoderDialog.cpp       + 7 connect() lines + 4 slot bodies + 5 new #includes
```

**Conditional decoder dialog is NOT modified** — bulk import on `ConditionalBitfieldDecoderDialog` is out of scope (deeply nested profile structure). Per-profile bit rules still go through `BitfieldDecoderDialog`, so they benefit from this feature indirectly.

**CSV schema (one mapping per row, rows merged by Label):**
```
Label,Bits,Reserved,UnknownBehavior,Enabled,Value,Binary,Meaning
Status,0-2,false,UNKNOWN,true,0,,Idle
Status,0-2,false,UNKNOWN,true,1,,Active
Flag,3,false,UNKNOWN,true,0,,Off
ReservedBlock,4-5,true,UNKNOWN,false,,,
```
- `Bits`: same syntax `BitfieldDecoder::parseBitPositions` accepts. Use `;` as the list separator inside the CSV cell (`0;1;2`) so it survives CSV comma splitting; ranges (`0-2`) work too.
- `Binary` takes precedence over `Value` when both are filled.
- Reserved rows: empty Value / Binary / Meaning is fine.
- Rules with same `Label` across rows: merged into one `BitDecodeRule`. `Bits` / `Reserved` / `UnknownBehavior` / `Enabled` must match across all rows that share a Label.
- Header row is case-insensitive, order-flexible. `#`-prefixed lines and blank lines skipped.

**JSON schema:** uses the existing `BitfieldDecoder::rulesToJson` output format verbatim. Importing calls `BitfieldDecoder::rulesFromJson`.

**Validation:** in addition to per-row CSV validation, every imported rule list is fed through `BitfieldDecoder::validateRules` before applying — the exact gate the manual Save path uses, so import can never produce state the dialog wouldn't otherwise accept.

**Export:** single "Export…" button. The `QFileDialog` filter dropdown (CSV vs JSON) determines the format. Round-trips with both import paths.

#### Verification status (v9)
- Clean qmake + mingw32-make build on Qt 5.10.1.
- End-to-end UI testing **pending**: open `BitfieldDecoderDialog`, exercise Template / Import CSV / Import JSON / Export buttons; confirm Replace/Append flow; confirm errors collected into one dialog; confirm `ConditionalBitfieldDecoderDialog` has no new buttons.

### v10 (same branch — stacked on v8 + v9)
Per-message JSON import/export inside `FieldConfigurationDialog` + a hand-editing guide.

**Why:** the user wants a CSV → JSON → hand-edit-to-add-bit-decoders → JSON-back-in workflow. CSV defines the structural fields; JSON is the format that can also carry bit-decoder rules; hand-editing the JSON is the cheapest way to add bit mapping for users who already have an ICD bit table on paper.

**Files added (new):**
```
NEW:
  docs/EDITING_JSON.md   — comprehensive hand-editing guide (field structure, bit decoder
                            structure, conditional decoder structure, common mistakes,
                            quick-reference skeleton)
```

**Files modified (append-only):**
```
  headers/ProjectFile.h                   + 2 new public static methods
                                            (fieldListToJson / fieldListFromJson)
  sources/ProjectFile.cpp                 + 2 method bodies + 2 helper functions in a
                                            second anonymous namespace at the bottom
  headers/FieldConfigurationDialog.h      + 2 new private slots
                                            (onImportJsonClicked / onExportJsonClicked)
  sources/FieldConfigurationDialog.cpp    + 2 connect() lines + 2 slot bodies +
                                            3 new #includes
  forms/FieldConfigurationDialog.ui       + 2 buttons (Import JSON... / Export JSON...)
```

**Key design choice:** the v10 JSON format nests `bitfieldDecoder` and `conditionalDecoder` as **JSON objects** (not stringified JSON), which makes the file human-editable. The internal `ProjectFile` save format keeps using stringified JSON for backward compatibility. Both shapes are accepted on import via the `jsonStringToValue` / `jsonValueToString` helpers in `ProjectFile.cpp`.

**Top-level shape of exported field-list JSON:**
```json
{ "version": 1, "kind": "PcapUdpExtractorFieldList", "exportedAt": "...",
  "fields": [ { ...field..., "bitfieldDecoder": <object|null>, "conditionalDecoder": <object|null> } ] }
```

**Validation:** `fieldListFromJson` runs `BitfieldDecoder::rulesFromJson` (which itself runs `validateRules`) and `ConditionalBitfieldDecoder::fromJson` per field. Decoder-level failures are reported as warnings — the field still imports without that decoder. Hard JSON parse errors abort the import.

#### Verification status (v10)
- Clean qmake + mingw32-make build on Qt 5.10.1 (~497 KB).
- End-to-end UI testing **pending**: import a CSV, click *Export JSON…*, hand-add a bitfieldDecoder block per the docs guide, click *Import JSON…*, confirm the bit decoder loads.

### v11 (same branch — stacked on v8 + v9 + v10)
Adds a **`String` data type** for variable-length UTF-8 text fields (callsigns, names, message tags, etc.). No new files — touches existing dispatchers only, all additive (new enum value + new switch cases + one relaxed guard for the length cap).

**Files modified (additive, switch-case extensions):**
```
  headers/AppTypes.h                      enum + naturalLength: +2 lines
  sources/ExtractionEngine.cpp            +1 helper (extractStringValue), +1 early-return
                                            in valueFromPayload, +1 branch in valuesFromPayload,
                                            +1 case in formatRawValue
  sources/InputValidator.cpp              relaxed length<=8 guard to allow Strings,
                                            +1 case in fieldDataTypeValidationName
  sources/FieldConfigurationDialog.cpp    +1 combobox entry, +1 case in isKnownDataType
  sources/FieldCsvCodec.cpp               +1 case in dataTypeToLabel, +4 entries in kTypeLabels
                                            (string/String/str/text), +1 entry in supportedDataTypeLabels
  sources/ProjectFile.cpp                 +1 case in dataTypeToJsonString,
                                            +1 case in dataTypeFromJsonString
```

**Decoding semantics:** read `length` bytes from the payload at `byteOffsetcorrect`. Trim trailing `0x00` bytes (common in fixed-width C-style strings). Decode the remainder as UTF-8 (ASCII passes through unchanged). Returns `"N/A"` if the slice is out of payload bounds.

**Validation:** `InputValidator::validateFields` now allows `length > 8` only when `dataType == String`. All other types keep the 1–8 byte cap unchanged.

**Bit / conditional decoders:** strings are **not** eligible for bit decoding. The existing dialogs gate on `1 <= fieldLength <= 8`, so any String field (typically > 8 bytes) is silently rejected — no UI changes needed.

#### Verification status (v11)
- Clean qmake + mingw32-make build on Qt 5.10.1.
- End-to-end UI testing **pending**: add a String field of length 16, run extraction over a pcap containing readable ASCII at that offset, verify the value appears as text in the output CSV.

---

## 10. Common recipes

- **Adding a new property to `FieldDefinition`:**
  1. Extend the struct in [headers/AppTypes.h](headers/AppTypes.h).
  2. Surface it in [sources/FieldConfigurationDialog.cpp](sources/FieldConfigurationDialog.cpp) — table columns, `collectFields()`, `refreshFieldTable()`.
  3. Round-trip it in [sources/ProjectFile.cpp](sources/ProjectFile.cpp) (`fieldToJson` / `fieldFromJson`).
  4. Decide whether [sources/FieldCsvCodec.cpp](sources/FieldCsvCodec.cpp) should expose it.

- **Adding a new menu action:**
  1. Add `<action>` element in [forms/MainWindow.ui](forms/MainWindow.ui) (siblings of the existing v8 actions).
  2. Declare a slot in [headers/MainWindow.h](headers/MainWindow.h).
  3. `connect()` in the `MainWindow` constructor (group with the other v8 menu connects).
  4. Implement the slot body at the end of [sources/MainWindow.cpp](sources/MainWindow.cpp).

- **Adding a new data type:**
  1. Extend the `FieldDataType` enum class in [headers/AppTypes.h](headers/AppTypes.h).
  2. Update `fieldDataTypeNaturalLength()` in the same header.
  3. Add a combobox entry in `FieldConfigurationDialog::setTypeCell` ([sources/FieldConfigurationDialog.cpp](sources/FieldConfigurationDialog.cpp)).
  4. Add a CSV label in `FieldCsvCodec::dataTypeToLabel` / `dataTypeFromLabel` ([sources/FieldCsvCodec.cpp](sources/FieldCsvCodec.cpp)).
  5. Add a JSON label in `dataTypeToJsonString` / `dataTypeFromJsonString` ([sources/ProjectFile.cpp](sources/ProjectFile.cpp)).
  6. Handle decode in the extraction engine if the new type needs new byte-interpretation logic.

---

## 11. What NOT to do

- Do **not** refactor working extraction / parsing / decoding logic — it's validated against real captures.
- Do **not** pull in any external dependency.
- Do **not** assume Qt 6 features exist.
- Do **not** change the `byteOffset` / `byteOffsetcorrect` 1-vs-0-based convention.
- Do **not** commit unless explicitly asked.
- Do **not** "tidy up" existing slot bodies. Append at the end; never rewrite.
