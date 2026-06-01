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

### `claude/nmea-support` (current branch — based off `remove-asterix`)
NMEA 0183 decoding, added in the same shape the (now-removed) ASTERIX feature
had. `remove-asterix` strips all ASTERIX code; this branch re-introduces a
per-message `dataFormat` selector, this time supporting `"HEX"` (default) and
`"NMEA"`. Strictly additive — every NMEA path is gated on `dataFormat == "NMEA"`,
HEX behaviour is untouched.

**What NMEA is:** ASCII, comma-delimited sentences `$aaccc,d1,..,dn*hh<CR><LF>`
— `aa` talker, `ccc` formatter (GGA/RMC/…), `*hh` = XOR checksum of chars
between `$` and `*`. Variable length, null fields (`,,`). Registry-driven:
a built-in catalogue maps each formatter to named positional fields. Fields are
addressed by **comma index, not byte offset** (user choice).

**Data model:**
- `MessageDefinition.dataFormat ∈ {"HEX","NMEA"}` (default `"HEX"`) +
  `nmeaSentenceType` (3-char formatter, e.g. `"GGA"`).
- `FieldDefinition.nmeaFieldIndex` — 1-based comma position (0 for Hex fields).
  When non-zero, byteOffset/length/dataType are ignored.

**New files (mirror the old ASTERIX scaffolding, simpler):**
```
headers/NmeaTypes.h                  NmeaValueKind / NmeaFieldDef / NmeaSentenceDef (data-only)
headers/NmeaSentenceRegistry.h
sources/NmeaSentenceRegistry.cpp     built-in catalogue: GGA GLL RMC VTG GSA GSV ZDA
                                     GST GNS HDT VHW DBT DPT MWV. lookup/supportedFormatters/displayName
headers/NmeaDecoder.h
sources/NmeaDecoder.cpp              decodePacket(formatter, payload) → records.
                                     Splits sentences, XOR-validates checksum, splits
                                     comma fields, formats per kind (lat/lon/time/date).
                                     formatValue() exposed for previews.
headers/NmeaSentencePickerDialog.h + .cpp + forms/NmeaSentencePickerDialog.ui
                                     pick a formatter (mirrors AsterixCategoryPickerDialog)
headers/NmeaFieldConfigurationDialog.h + .cpp + forms/NmeaFieldConfigurationDialog.ui
                                     per-field Enable + Custom Label (no bit decoder —
                                     NMEA fields are ASCII). fieldConfig() → FieldDefinitions
                                     with name + nmeaFieldIndex.
```

**Modified (all additive, gated on `dataFormat=="NMEA"`):**
```
PcapUdpExtractor.pro                 + 4 SOURCES, 5 HEADERS, 2 FORMS
headers/AppTypes.h                   + nmeaFieldIndex on FieldDefinition
headers/MessageDefinition.h          + dataFormat + nmeaSentenceType (+ ctor inits)
forms/MessageDefinitionDialog.ui     + Data Format combo + NMEA Sentence label
headers/MessageDefinitionDialog.h/.cpp + setDataFormat/dataFormat/setNmeaSentenceType/
                                     nmeaSentenceType + onDataFormatChanged (opens picker,
                                     reverts to HEX on cancel) + NMEA branch in onSaveClicked
sources/MessageLengthFilterDialog.cpp + dataFormat/nmeaSentenceType round-trip in onAdd/onEdit;
                                     NMEA branch in configureMessageAt → NmeaFieldConfigurationDialog;
                                     hasDuplicateSignature + validateFieldsFitPayload made NMEA-aware
                                     (NMEA collides only on same nmeaSentenceType; never vs HEX)
sources/MainWindow.cpp               + buildNmeaRow() + payloadContainsNmeaFormatter() in unnamed ns;
                                     NMEA early branch in packetMatchesMessage (match by formatter,
                                     skip length/header); NMEA branches in openFieldConfigurationForMessage,
                                     validateMessageDefinitions, exportByMessageDefinitions (one row per
                                     record), startLiveCaptureWithMessages, tryRouteLivePacketByMessage,
                                     onConfigureLiveMessageFieldsClicked
sources/ProjectFile.cpp              + nmeaFieldIndex in fieldToJson/fromJson; dataFormat +
                                     nmeaSentenceType in messageToJson/fromJson
```

**Routing rules (when NMEA takes a new path):**
- `packetMatchesMessage`: NMEA returns `port-match && payloadContainsNmeaFormatter(...)`,
  ignoring exact length and optional header.
- `exportByMessageDefinitions` / `tryRouteLivePacketByMessage`: NMEA decodes via
  `NmeaDecoder` and emits one CSV row per sentence record.
- Field config (port/live/length-filter dialogs): NMEA opens
  `NmeaFieldConfigurationDialog` instead of `FieldConfigurationDialog`.
- `validateMessageDefinitions` / `startLiveCaptureWithMessages`: NMEA skips
  `InputValidator::validateFields`; lightweight check (registry has formatter,
  each field has `nmeaFieldIndex > 0` and a name).

**Scope boundaries:** parametric `$` sentences only (`!` AIS encapsulation deferred).
Talker is not part of the match (any talker for a formatter). Checksum is validated
and warned, not yet a CSV column. Length-filter table doesn't show format/sentence —
confirm via Edit (same known limitation ASTERIX had). Catalogue extends by appending a
`NmeaSentenceDef` in `NmeaSentenceRegistry.cpp`.

#### Verification status (NMEA)
- Clean build on Qt 5.15 (Linux compile-check; target is still Qt 5.10/mingw). Binary ~957 KB.
- No new warnings (pre-existing `fieldDataTypeValidationName`, `fieldBytesFromPayload`, and a
  Qt-5.15-only `QString::split` deprecation remain).
- Decoder unit-tested against a real `$GPGGA,...*47` sentence: checksum OK, UTC→`12:35:19`,
  lat→`48 07.038`, lon→`011 31.000`; multi-sentence payload filters to matching formatter only;
  bad-checksum sentence flagged `checksumOk=false`.
- End-to-end UI testing **pending**: length filter → Data Format = NMEA → pick GGA → Save →
  Configure Fields (enable + rename) → export over a UDP-NMEA pcap → confirm one row per
  sentence with formatted lat/lon/time. Live mode + project round-trip of
  `dataFormat`/`nmeaSentenceType`/`nmeaFieldIndex`.

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

### v12 (same branch — stacked on v8 + v9 + v10 + v11)
Five user-driven UX changes, scoped to keep behavioural deltas gated on opt-in state (empty defaults = pre-v12 behaviour):

1. **Dark / Light theme toggle.** Top-row button in the Input Mode group ("Light Theme" / "Dark Theme"). Choice persists via `QSettings` (key `ui/theme`). New `Themes` class centralizes the QSS for both palettes. Each window/dialog ctor calls `Themes::apply(this)` after `setupUi(this)` so the theme propagates everywhere (including already-open dialogs via `Themes::applyToAllTopLevels()`).
2. **CSV / JSON dropdowns in field config dialog.** The five separate buttons (Import CSV / Export CSV / Template / Import JSON / Export JSON) collapse into two `QToolButton` dropdowns (`CSV ▾`, `JSON ▾`). The five existing slots are unchanged — only the trigger UI is rebuilt.
3. **Per-row Edit buttons for bit / conditional decoders.** The Bit Decoder and Cond. Decoder columns in the field table now show an inline "Edit" button (text plus rule/profile count in parens). Click opens the matching dialog scoped to that row. Tooltip carries the prior status text. Selection is no longer required first.
4. **Length filter embedded in header mode AND live mode.**
   - Header mode: every header-filter row gets a per-row "Manage Length Filters" button + status label. Configured messages live in `m_headerMessagesByRow` (parallels `m_portMessagesByRow`). On export, when any header row has messages, `onStartClicked` routes the whole header-mode export through `exportByMessageDefinitions` (per-message CSV files) instead of the per-filter partition path.
   - Live mode: one global "Manage Length Filters" button alongside the live controls. Configured messages live in `m_liveMessages`. `startLiveCapture` early-delegates to `startLiveCaptureWithMessages` when non-empty: prompts for an output directory and opens one `CsvStreamWriter` per message. `onLiveDatagramReceived` then routes each datagram through `tryRouteLivePacketByMessage` (matched by port + length + optional header) and `stopLiveCapture` closes all per-message writers.
5. **Optional header bytes for length filters.** `MessageDefinition` gains `QByteArray optionalHeader`. `MessageDefinitionDialog` adds an "Optional Header (hex)" input (0–8 hex chars, even-length, validated). `MessageLengthFilterDialog`'s table grows a column for the header. `packetMatchesMessage` checks the leading bytes when header is non-empty (empty = unchanged). Duplicate-detection in both `validateMessageDefinitions` and `MessageLengthFilterDialog::hasDuplicateSignature` keys on `port + length + headerHex`, allowing two messages with the same length on the same port if they have distinct header signatures.

**Files added (new):**
```
NEW:
  headers/Themes.h
  sources/Themes.cpp
```

**Files modified (append-mostly):**
```
  PcapUdpExtractor.pro                    + 2 lines (Themes entries)
  forms/MainWindow.ui                     + 1 line (btnToggleTheme)
                                          + 2 lines (live mode: btnManageLiveLengthFilters + lblLiveLengthFilterStatus)
  headers/MainWindow.h                    + 3 slots + 8 helpers + 6 state fields + 1 fwd-decl (QPushButton)
  sources/MainWindow.cpp                  + ~300 lines (v12 helper block appended; small additive branches in
                                            onStartClicked / startLiveCapture / onLiveDatagramReceived /
                                            stopLiveCapture / rebuildFilterInputs / setBusy / setLiveUiState /
                                            captureProjectState / applyProjectState)
  headers/MessageDefinition.h             + 1 field (optionalHeader)
  headers/MessageDefinitionDialog.h       + 2 methods (setOptionalHeaderHex, optionalHeaderHex)
  forms/MessageDefinitionDialog.ui        + 1 row (Optional Header input)
  sources/MessageDefinitionDialog.cpp     + 2 method bodies + header validation in onSaveClicked
  headers/MessageLengthFilterDialog.h     + 1 method (hasDuplicateSignature)
  sources/MessageLengthFilterDialog.cpp   + Optional Header column + header bytes round-trip + signature-aware dup check
  forms/FieldConfigurationDialog.ui       - 5 QPushButtons, + 2 QToolButtons (CSV/JSON dropdowns)
  headers/FieldConfigurationDialog.h      + 2 slots (onBitfieldEditRowClicked, onConditionalEditRowClicked)
  sources/FieldConfigurationDialog.cpp    + QMenu wiring for dropdowns + 2 slot bodies + cell widgets become QPushButton
  headers/ProjectFile.h                   + 2 fields on ProjectState (headerMessagesByRow, liveMessages)
  sources/ProjectFile.cpp                 + optionalHeaderHex round-trip in messageToJson/messageFromJson
                                          + headerMessages array + live.messages array round-trip
  + 8 dialog/window ctors                 + 1 line each: Themes::apply(this) after setupUi(this)
```

**State held by MainWindow (v12 additions):**
- `QList<QList<MessageDefinition>> m_headerMessagesByRow` — per-header-row length filters
- `QList<QPushButton*> m_headerLengthFilterButtons` — per-row Manage buttons
- `QList<MessageDefinition> m_liveMessages` — global live-mode length filters (configured set)
- `QList<MessageDefinition> m_activeLiveMessages` — snapshot taken at startLiveCapture
- `QList<CsvStreamWriter*> m_liveMessageWriters` — one writer per active live message
- `QList<quint64> m_liveMessageRowCounts`

**Routing rules (when does v12 take a new path):**
- `onStartClicked`, file mode + header filter: new path when `anyHeaderRowHasMessages()` is true.
- `startLiveCapture`: new path when `m_liveMessages` is non-empty.
- `onLiveDatagramReceived`: per-message routing only when `m_activeLiveMessages` is non-empty (i.e., live capture was started in per-message mode).
- All other paths: fall through to pre-v12 behaviour.

**Theme propagation:** `Themes::apply` calls `setStyleSheet` on the widget passed in. Toggling at runtime re-applies via `applyToAllTopLevels`; each newly-opened dialog gets the current theme through its own `Themes::apply(this)` call in its ctor.

#### Verification status (v12)
- Clean qmake + mingw32-make build on Qt 5.10.1 (~594 KB).
- One pre-existing warning (unused `fieldBytesFromPayload`) — not introduced by v12.
- End-to-end UI testing **pending**: toggle theme; configure length filters per header row and run export; configure live length filters and start live capture; add an optional header on a length filter and verify same-length disambiguation.

### v13 (same branch — stacked on v8 + v9 + v10 + v11 + v12)
Per-message **Compare Options** verification layer. Each length-filter message can now be tagged with expected properties — header bytes, terminator bytes, checksum (XOR or SUM), refresh rate (Hz), endianness — and during extraction the program writes the observed/computed values to CSV plus True/False + reason columns when an expected value is supplied. The feature is gated behind a per-row button inside the Length Filters dialog. Strictly additive — empty/disabled config produces pre-v13 CSV layout.

**Two-tier optionality (per check section):**
- Section disabled → no extra columns.
- Section enabled + expected value blank → only the **observed/computed** column is added (log-only mode).
- Section enabled + expected value supplied → observed *and* OK + reason columns are added.
- The checksum section always compares (its "expected" is the byte already stored in the payload); enabling it adds three columns: `ChecksumComputed`, `ChecksumStoredInPayload`, `ChecksumOK`.

**Files added (new):**
```
NEW:
  headers/CompareOptionsEngine.h
  sources/CompareOptionsEngine.cpp     (engine: compareColumnNames + compareRow + RefreshRateTracker)
  headers/CompareOptionsDialog.h
  sources/CompareOptionsDialog.cpp     (modal QDialog mirroring MessageDefinitionDialog pattern)
  forms/CompareOptionsDialog.ui        (5 QGroupBoxes, each checkable)
```

**Files modified (append-only):**
```
  PcapUdpExtractor.pro                       + 5 lines (2 SOURCES, 2 HEADERS, 1 FORMS)
  headers/AppTypes.h                         + CompareOptionsConfig struct (appended before #endif)
  headers/MessageDefinition.h                + hasCompareOptions + compareOptions field + ctor init
  headers/MainWindow.h                       + #include "CompareOptionsEngine.h"
                                             + QList<RefreshRateTracker> m_liveCompareTrackers
  headers/MessageLengthFilterDialog.h        + slot onCompareOptionsButtonClicked
  sources/MessageLengthFilterDialog.cpp      + MESSAGE_COL_COMPARE constant (=5)
                                             + 6th column "Compare Options" in header
                                             + per-row "Edit / Configure" button in refreshTable
                                             + #include "CompareOptionsDialog.h"
                                             + slot body at end of file
  sources/ProjectFile.cpp                    + compareOptionsToJson / compareOptionsFromJson helpers
                                               in the anonymous namespace
                                             + 2 inserts in messageToJson, 2 in messageFromJson
  sources/MainWindow.cpp                     + per-partition RefreshRateTracker list in
                                               exportByMessageDefinitions (file mode)
                                             + compareColumnNames append at exporter->open
                                             + compareRow append before exporter->writeRow
                                             + parallel changes in startLiveCaptureWithMessages
                                               and tryRouteLivePacketByMessage (live mode)
                                             + m_liveCompareTrackers.clear() in stopLiveCapture
```

**CSV column contract** (`CompareOptionsEngine::compareColumnNames` / `compareRow` must stay in lockstep):

| Section enabled | Always-emitted observed columns | Emitted only when expected present |
|---|---|---|
| Header | `HeaderObserved` | `HeaderExpected`, `HeaderOK` |
| Terminator | `TerminatorObserved` | `TerminatorExpected`, `TerminatorOK` |
| Checksum | `ChecksumComputed`, `ChecksumStoredInPayload`, `ChecksumOK` | (always — stored byte is the expected value) |
| Refresh rate | `RefreshRateObservedHz` | `RefreshRateExpectedHz`, `RefreshRateOK` |
| Endianness (per multi-byte numeric field) | `<name>_BE`, `<name>_LE` | `<name>_EndianOK` |
| Endianness (once) | — | `EndianConfigured` |
| Any comparison active | — | `CompareReason` |

**Timestamps for refresh-rate computation:**
- File mode: `rawPacket.tsSec * 1000 + rawPacket.tsUsec / 1000` (already on the `RawPacket` in scope at the row-write site).
- Live mode: `arrivalTimeUtc.toMSecsSinceEpoch()` (carried through `tryRouteLivePacketByMessage`).
- `RefreshRateTracker` is a `QQueue<qint64>`; pops everything older than `currentTs - 1000` and returns the queue size as Hz.

**Endianness check — known limitation:** the per-field `_EndianOK` column reduces to `"True" iff expectedEndianness == "BIG"`, because the project's `ExtractionEngine` always decodes multi-byte integers/floats as big-endian and there is no oracle for what the "right" interpretation is. The user gets both `_BE` and `_LE` reads side-by-side and can judge visually. A future iteration could add a per-field expected reference value to enable real detection.

**State added to MainWindow (v13):**
- `QList<RefreshRateTracker> m_liveCompareTrackers` — parallel to `m_activeLiveMessages`, sized at `startLiveCaptureWithMessages`, cleared at `stopLiveCapture`.

**Routing rules (when v13 takes a new path):**
- File-mode `exportByMessageDefinitions` row write: appends compare-row when `partition.definition.hasCompareOptions`.
- Live-mode `tryRouteLivePacketByMessage`: appends compare-row when `msg.hasCompareOptions`.
- Both paths preserve pre-v13 column counts when `hasCompareOptions == false` (engine returns empty list).

**MessageOutputPartition (sources/MainWindow.cpp:71):** unchanged — the tracker lives as a parallel `QList<RefreshRateTracker>` local to `exportByMessageDefinitions`, not as a member of the partition struct.

#### Verification status (v13)
- Clean qmake + mingw32-make build on Qt 5.10.1 (~660 KB).
- No new warnings introduced (two pre-existing warnings remain: `fieldDataTypeValidationName` in InputValidator.cpp, `fieldBytesFromPayload` in MainWindow.cpp).
- End-to-end UI testing **pending**: open length-filter dialog → per-row "Configure" → set header (`AA55`) + checksum (XOR over range) + refresh rate (e.g. 50 Hz) + endianness (BIG); run export over a matching pcap; confirm new columns appear with correct True/False results. Verify log-only mode by leaving expected values blank. Confirm sidecar `.pcproj.json` round-trips the compareOptions object.

### v15 (same branch — stacked on v8 + v9 + v10 + v11 + v12 + v13 + v14)
ASTERIX decoding for CAT021 / CAT034 / CAT048 / CAT062. Strictly additive: existing Hex extraction paths run unchanged unless a message is explicitly tagged with `dataFormat == "ASTERIX"`.

**New files:**
```
NEW (data + decoder):
  headers/AsterixTypes.h                  — AsterixItemKind / AsterixValueKind /
                                            AsterixSubItem / AsterixItemDef /
                                            AsterixCategoryDef (data-only)
  headers/AsterixUapRegistry.h
  sources/AsterixUapRegistry.cpp          — singleton UAPs for CAT021/34/48/62
  headers/AsterixDecoder.h
  sources/AsterixDecoder.cpp              — FSPEC walk + per-kind readers
                                            (Fixed/Extended/Repetitive/Compound/
                                            ExplicitLength) + value formatters
                                            (TimeOfDay, Lat/Lon, Mode-3A, Callsign,
                                            etc.)

NEW (UI):
  headers/AsterixCategoryPickerDialog.h
  sources/AsterixCategoryPickerDialog.cpp
  forms/AsterixCategoryPickerDialog.ui    — small modal: pick CAT021/34/48/62
  headers/AsterixFieldConfigurationDialog.h
  sources/AsterixFieldConfigurationDialog.cpp
  forms/AsterixFieldConfigurationDialog.ui — UAP-driven field list; per-row
                                              Enable / Custom Label / Bit Decoder
                                              button. Bit decoder reuses the
                                              existing BitfieldDecoderDialog.
```

**Files modified (append-only):**
```
  PcapUdpExtractor.pro                    + 4 SOURCES, 5 HEADERS, 2 FORMS
  headers/AppTypes.h                      + asterixItemId on FieldDefinition
                                            (empty for Hex fields; non-empty
                                            for ASTERIX UAP items)
  headers/MessageDefinition.h             + dataFormat ("HEX" default) +
                                            asterixCategory (0 = unset) + ctor inits
  forms/MessageDefinitionDialog.ui        + 2 rows: Data Format combo + ASTERIX
                                            Category label
  headers/MessageDefinitionDialog.h       + setDataFormat / setAsterixCategory
                                            / dataFormat() / asterixCategory()
                                            + onDataFormatChanged slot +
                                            promptForAsterixCategory helper
  sources/MessageDefinitionDialog.cpp     + ctor signal connect; slot bodies;
                                            ASTERIX validation in onSaveClicked
  sources/MessageLengthFilterDialog.cpp   + dataFormat/asterixCategory round-trip
                                            in onAdd/onEditMessageClicked;
                                            ASTERIX branch in configureMessageAt
                                            (uses AsterixFieldConfigurationDialog)
  sources/MainWindow.cpp                  + buildAsterixRow() in unnamed namespace;
                                            ASTERIX branch in
                                            openFieldConfigurationForMessage,
                                            onConfigureLiveMessageFieldsClicked,
                                            exportByMessageDefinitions (one CSV
                                            row per decoded record),
                                            tryRouteLivePacketByMessage;
                                            ASTERIX-aware field validation in
                                            validateMessageDefinitions and
                                            startLiveCaptureWithMessages
  sources/ProjectFile.cpp                 + asterixItemId round-trip in
                                            fieldToJson/fromJson +
                                            fieldListToJson/fromJson;
                                            dataFormat + asterixCategory round-trip
                                            in messageToJson/fromJson
```

#### Data model

`MessageDefinition.dataFormat ∈ {"HEX", "ASTERIX"}` (default `"HEX"`). `MessageDefinition.asterixCategory ∈ {0, 21, 34, 48, 62}`.

`FieldDefinition.asterixItemId` empty for Hex fields; on ASTERIX it carries the UAP item ID (e.g. `"I048/010"`). The Hex extraction path ignores it; the ASTERIX export path uses it to map UAP-decoded items back to user-configured fields.

#### Decoder

Single entry point: `AsterixDecoder::decodePacket(int expectedCategory, const QByteArray& payload)` returns `Result { QList<AsterixDecodedRecord> records; QStringList warnings; bool fatalError; }`. Each record is `{ category, recordLengthBytes, QList<AsterixDecodedItem> items }`. Each item carries `frn`, `id`, `defaultName`, `rawBytes`, `formattedValue`.

Algorithm:
1. Walk `(CAT, LEN)`-prefixed blocks back-to-back in the datagram. Blocks whose `CAT != expectedCategory` are skipped with a warning.
2. Within each block, parse FSPEC bytes until FX=0 → set of FRNs.
3. For each set FRN look up `AsterixItemDef` and dispatch by kind: `Fixed`, `Extended`, `Repetitive`, `Compound`, `ExplicitLength` (SPF/RE), or `Unknown` (stop record, warning).
4. Format each item's bytes per its `AsterixValueKind` (`formatValue` is also exposed publicly).

#### Routing rules (when ASTERIX takes a new path)

- File-mode `exportByMessageDefinitions`: per-partition branch when `partition.definition.dataFormat == "ASTERIX"`. Emits one CSV row per decoded record (multi-record datagrams produce multiple rows). v13 Compare-Options columns still append per row.
- Live-mode `tryRouteLivePacketByMessage`: per-message branch when `msg.dataFormat == "ASTERIX"`. Same one-row-per-record semantics.
- File-mode port path (`openFieldConfigurationForMessage`) and live-mode (`onConfigureLiveMessageFieldsClicked`) + `MessageLengthFilterDialog::configureMessageAt`: when message is ASTERIX, open `AsterixFieldConfigurationDialog` instead of `FieldConfigurationDialog`.
- `validateMessageDefinitions` and `startLiveCaptureWithMessages`: ASTERIX messages skip `InputValidator::validateFields` (which assumes Hex byteOffset/length) — replaced by a lightweight check (UAP exists, every field has non-empty asterixItemId).

#### CSV column contract for ASTERIX

`ExtractionEngine::columnHeaders(fields)` is reused as-is — it walks `field.name` plus bit-decoder rule columns (`<name>_<rule>`). The ASTERIX FieldDefinition list uses the same shape, so headers are deterministic per category once the user picks which items to enable.

#### Bit decoding

Reuses `BitDecodeRule` + `BitfieldDecoder::decodeRule`. Only available on UAP items of kind `Fixed` with `fixedLength ∈ {1..8}` (matching the existing Hex bit-decoder gate). Extended / Repetitive / Compound items have the "Bit Decoder" button disabled in the configurator.

#### Known limitations
- Compound items with sub-FSPEC bits beyond what `compoundSubItems` describes cause the record to stop walking at that point (warning emitted). Most practical traffic on supported categories is covered, but exotic items (esp. CAT062 I062/380, I062/390, I062/500) may produce partial records.
- Length filter table in `MessageLengthFilterDialog` does not (yet) display the format / category. Confirm by clicking *Edit* on the row.
- ASTERIX field configs are not yet wired into `FieldCsvCodec` / `BitRuleCsvCodec` import-export (CSV/JSON bulk import). Round-trip via `ProjectFile` works.

#### Verification status (v15)
- Clean qmake + mingw32-make build on Qt 5.10.1 (~780 KB).
- No new warnings introduced (pre-existing `fieldDataTypeValidationName` warning remains).
- End-to-end UI testing **pending**: add length filter → Data Format = ASTERIX → pick CAT048 → Save → Configure Fields → enable items + rename one + attach bit decoder → run export over a CAT048 pcap → verify per-message CSV, custom labels, bit-decoder sub-columns. Save Project → reload → confirm `dataFormat`/`asterixCategory`/`asterixItemId` round-trip.

### v14 (same branch — stacked on v8 + v9 + v10 + v11 + v12 + v13)
**Live Mode UI cleanup.** Removes the pre-v12 single-field path's UI surface so Live Mode only exposes the length-filter workflow:

1. **Removed widgets from Live UDP Capture row:** `btnConfigureLiveFields` ("Configure Live Fields" button) and `lblLiveFieldStatus` (field-count label). The slot `onConfigureLiveFieldsClicked()` and the field-list member `m_liveFields` stay defined for project-file backward compatibility but are no longer reachable from the UI.
2. **Hidden in Live Mode:** the entire `filterGroup` (Message Filters: Number of Filters + Port/Header radio + per-row filter table). Live mode binds a single UDP port and disambiguates messages via per-message *Optional Header* bytes (v12) — port-vs-header radio is meaningless there.
3. **New widget:** `liveConfiguredMessagesGroup` containing `tblLiveConfiguredMessages` — a read-only table that mirrors file mode's `tblConfiguredMessages` pattern. Columns: Message Name | Payload Length | Optional Header | Fields | Configure Fields (button). Backed by `m_liveMessages`.
4. **Start guard:** `startLiveCapture` now requires `m_liveMessages` to be non-empty. Empty state shows a friendly warning and aborts. The pre-v12 single-writer path below the guard stays as dead code (additive per CLAUDE.md).
5. **Per-row Configure Fields:** new slot `onConfigureLiveMessageFieldsClicked()` resolves the button's `liveMessageIndex` property and reuses the existing `configureFieldList(fields, length, title)` helper that file-mode messages use — no new field-editing logic.

**Files modified (additive):**
```
  forms/MainWindow.ui                     - 2 widgets (btnConfigureLiveFields, lblLiveFieldStatus)
                                          + 1 group (liveConfiguredMessagesGroup with tblLiveConfiguredMessages)
  headers/MainWindow.h                    + 1 slot (onConfigureLiveMessageFieldsClicked)
                                          + 1 helper (refreshLiveConfiguredMessagesTable)
  sources/MainWindow.cpp                  + ctor block: configure tblLiveConfiguredMessages columns,
                                            initial refresh call; removed dead connect for btnConfigureLiveFields
                                          + onInputModeChanged: extra 2 setVisible lines (filterGroup,
                                            liveConfiguredMessagesGroup)
                                          + startLiveCapture: m_liveMessages-empty early-return guard
                                          + openLiveLengthFilterDialog: refreshLiveConfiguredMessagesTable
                                            after accept
                                          + applyProjectState: refreshLiveConfiguredMessagesTable after
                                            existing refresh calls
                                          + setBusy / setLiveUiState / refreshStandaloneFieldStatus:
                                            removed lines that referenced removed widgets
                                          + ~60 lines: refreshLiveConfiguredMessagesTable +
                                            onConfigureLiveMessageFieldsClicked appended to v12 helper block
```

**Files NOT changed:**
- `ProjectFile.cpp` / `ProjectFile.h` — `state.liveFields` and `state.liveFilterConfig` keys still emitted/read; older project files load unchanged.
- File Mode and Header Filter Mode behaviours.

**Dead code (intentionally left, per CLAUDE.md additive rule):**
- The branch of `startLiveCapture` after the new guard (m_liveFields validation, header/port FilterConfiguration build, single QFileDialog::getSaveFileName, m_liveWriter open). Unreachable because m_liveMessages is now required.
- `onConfigureLiveFieldsClicked` slot body (no widget triggers it).
- `liveHeaderMatches()` and `extractLiveRowValues()` (only called from the dead branch).

#### Verification status (v14)
- Clean qmake + mingw32-make build on Qt 5.10.1 (~661 KB).
- No new warnings introduced (only pre-existing `fieldBytesFromPayload` warning remains).
- End-to-end UI testing **pending**: toggle Live Mode → confirm Configure Live Fields button gone, Message Filters group hidden, Configured Messages (Live) table visible. Click *Manage Length Filters* → add two same-length messages with different *Optional Header* bytes → confirm both rows in `tblLiveConfiguredMessages`. Click *Configure Fields* on a row → FieldConfigurationDialog opens scoped to that message. Click *Start Live Capture* with no messages → confirm friendly warning; with messages → confirm directory prompt + per-message CSVs. Save project → reload → confirm state restored.

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
