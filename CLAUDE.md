# CLAUDE.md — PcapUdpExtractor

This file is the project memory for Claude Code. It captures everything needed to work on the codebase without re-exploring it from scratch.

**Maintenance rule:** Update this file in the same change that modifies architecture, the data model, conventions, branch state, or build instructions. If you find yourself running `Grep` / `Read` to re-learn something about the project, that knowledge belongs here once you've found it.

---

## 1. What this project is

**PcapUdpExtractor** — a Qt 5.10 / C++11 desktop GUI app (Windows, mingw53_32 / msvc kits) that:

1. Opens `.pcap` / `.pcapng` files, parses UDP packets, and exports user-defined payload fields into **Excel `.xlsx`** (was CSV until 2026-06-10; see §10.23).
2. Provides a **Live UDP** mode that listens on a socket and streams the same field extraction to Excel in real time.
3. Lets the user define fields with offsets, types, lengths, resolution expressions, **bitfield decoders**, and **conditional bitfield decoders** (whose behaviour depends on the value of a *controller* field).
4. Decodes **NMEA 0183** sentences as a per-message data format (alternative to raw Hex byte offsets).
5. **Bulk-defines** messages and fields by importing field tables from CSV/JSON or by **importing a Word `.docx` ICD** (Interface Control Document).
6. Optionally **verifies** each message during extraction (header / terminator / checksum / refresh-rate / endianness "Compare Options").

Build system: qmake (`.pro` file).

---

## 2. Hard project constraints — DO NOT VIOLATE

1. **Qt 5.10 only.** Verified against Qt 5.10.1 / mingw53_32. No Qt 6 APIs (no `qsizetype`, no `Qt::SplitBehavior`, no `QPromise`, no `QFuture::then`). Qt-6-only code is guarded with `#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)` (e.g. `QTextStream::setCodec`).
2. **No external libraries — one user-approved exception: vendored QXlsx.** Otherwise Qt builtins only (`QJsonDocument`, `QFile`, `QTextStream`, `QSet`, `QCryptographicHash`, `QStandardPaths`, `QXmlStreamReader`, and the private `QZipReader`/`QZipWriter`). On 2026-06-10 the user explicitly requested Excel `.xlsx` output using "any external library required": **QXlsx v1.4.10** (MIT) is vendored at `third_party/QXlsx/` and compiled from source via `include(third_party/QXlsx/QXlsx.pri)`; it needs only `QT += core gui-private` (already required for `.docx` import) and C++11 — no DLLs, fully offline. Do not add anything further. Guide: `docs/EXCEL_EXPORT.md`.
3. **Strictly additive changes.** Never modify the *behaviour* of an existing function. New work = new files, new slots, new connections, new menu items, OR a single appended line at the end of an existing function's body (call site only, no rewrites). Self-check: *"Could a developer revert this commit and have the app work identically to before?"* If no, the change isn't additive.
4. **Do not commit unless explicitly asked.** The user is particular about this.

---

## 3. Build

Target/verified toolchain is **Windows + Qt 5.10.1 / mingw53_32**:

```powershell
$env:PATH = 'D:\qt\5.10.1\mingw53_32\bin;D:\qt\Tools\mingw530_32\bin;' + $env:PATH
New-Item -ItemType Directory -Force build | Out-Null
Set-Location build
qmake ..\PcapUdpExtractor.pro
mingw32-make -j4
# Output: build\release\PcapUdpExtractor.exe
```

Alternative kits installed under `D:\qt\5.10.1\`: `msvc2013_64`, `msvc2015`, `android_armv7`, `android_x86`. mingw is the verified path.

> **Modules:** the `.pro` needs `QT += network gui-private` (both ship with the Qt 5.10 kits) and includes `third_party/QXlsx/QXlsx.pri` (compiles QXlsx sources into the app — first build after pulling is simply qmake + make, no extra installs).

> **Shadow-build gotcha:** the user normally launches from **Qt Creator's own shadow-build directory**, e.g. `C:\GitHub\build-PcapUdpExtractor-Desktop_Qt_5_10_1_MinGW_32bit-Debug\debug\PcapUdpExtractor.exe` — a *different* folder from the `build\release\` path above. A `mingw32-make` here does **not** update that exe, so the user can rebuild via this command yet still launch a stale binary (both windows share the title "PCAP UDP Extractor"). When a change "doesn't show up," confirm the running target first: `Get-Process PcapUdpExtractor | Select Id,Path`. To refresh the user's normal run target, they must Rebuild in Qt Creator (Debug) or launch the `build\release\` exe.

> **Container note:** Claude Code on the web runs this repo in a **Linux container with no Qt installed**, so qmake/mingw builds cannot run here — all build/run verification happens on the Windows kit. Code written in a container is static-reviewed only (balanced braces, decls↔defs, API signatures against the headers) and must be compiled on Windows.

`build/`, `.qmake.stash`, `object_script.*`, `release/` are qmake-generated and should be gitignored (a `.gitignore` is still worth adding).

---

## 4. Repository layout

```
PcapUdpExtractor.pro      qmake build file — every SOURCES / HEADERS / FORMS listed explicitly; includes third_party/QXlsx/QXlsx.pri
sources/*.cpp             implementation files
headers/*.h               public headers (forward-declare Qt classes; keep includes light)
forms/*.ui                Qt Designer XML
third_party/QXlsx/        vendored QXlsx v1.4.10 (MIT) — .xlsx writer (header/ + source/ + QXlsx.pri + LICENSE)
docs/*.md                 per-feature design/working notes (see docs/ICD_DOCX_IMPORT.md, EDITING_JSON.md, EXCEL_EXPORT.md, SHORTCUTS.md)
docs/PROJECT_MINDMAP.md   function-level map: every function + where defined (file:line) + where called; full signal/slot graph + 5 pipeline traces
docs/USER_MANUAL.md       end-user walkthrough of every screen/feature; screenshots in docs/manual/*.png (re-capture via docs/manual/_uitools.ps1)
build/                    qmake-generated (untracked)
CLAUDE.md                 this file
README.md                 user-facing feature list
```

The working directory is the repo root (`/home/user/pcapfile` in the container; was `c:\GitHub\pcapfile` on the Windows dev machine).


---

## 5. Core data model

### `FieldDefinition` — [headers/AppTypes.h](headers/AppTypes.h)
A single decoded field within a UDP payload.

| Field | Type | Notes |
|---|---|---|
| `name` | `QString` | non-empty, ≤ 64 chars by convention; unique within a message |
| `byteOffset` | `int` | **1-based** (matches the UI dialog) |
| `byteOffsetcorrect` | `int` | **0-based**, MUST equal `byteOffset - 1` everywhere |
| `length` | `int` | bytes; user-definable for any type |
| `dataType` | `FieldDataType` enum class | 13 values, see below |
| `resolution` | `double` | numeric scale, default `1.0`, must be `> 0` |
| `resolutionExpression` | `QString` | text formula (e.g. `raw*0.01`), default `"1"` |
| `hasBitfieldDecoder` | `bool` | + `QList<BitDecodeRule> bitDecodeRules` |
| `hasConditionalBitfieldDecoder` | `bool` | + `ConditionalBitfieldDecoderConfig conditionalDecoder` |
| `nmeaFieldIndex` | `int` | **NMEA only.** 1-based comma position of the token in the sentence. `0` for Hex fields. When non-zero, `byteOffset`/`length`/`dataType` are ignored and the value comes from `NmeaDecoder`. |
| `nmeaValueKind` | `int` | `int(NmeaValueKind)` (0 = Text). Authoritative only for **custom** NMEA sentences (no registry entry); predefined sentences use the registry kind. |

### `FieldDataType` enum class — [headers/AppTypes.h](headers/AppTypes.h)
`RawUnsignedBE, Uint8, Int8, Uint16, Int16, Uint32, Int32, Uint64, Int64, Float32, Float64, Bool, String`. Natural length via `fieldDataTypeNaturalLength()`; `RawUnsignedBE` and `String` return 0 (length user-provided). `fieldDataTypeHasFixedLength()` = natural length > 0.

- **String:** variable-length UTF-8 text. Length is user-defined and is **not** capped at 8 bytes (the integer types are). Decoding reads `length` bytes, trims trailing NUL, decodes UTF-8. String fields cannot have bit/conditional decoders (dialogs gate on `fieldLength <= 8`).

### `BitDecodeRule` — [headers/AppTypes.h](headers/AppTypes.h)
`label`, `bitPositions` (`QList<int>`), `valueMeanings` (`QMap<quint64, QString>`), `reserved`, `unknownBehavior` ∈ `{"UNKNOWN","BLANK","RAW_BINARY"}`, `enabled`.

### `ConditionalBitfieldDecoderConfig` — [headers/AppTypes.h](headers/AppTypes.h)
`controllerFieldName`, `unknownBehavior` ∈ `{"UNKNOWN_CONTROLLER","BLANK"}`, `profiles` (`QList<ConditionalBitDecodeProfile>`). Each profile has `profileName`, `controllerValue` (quint64), `bitDecodeRules`, and `exclusionRules` (mutual-exclusivity constraints on bits).

### `CompareOptionsConfig` — [headers/AppTypes.h](headers/AppTypes.h)
Per-message verification config (see §10.9). Five checkable sections: header, terminator, checksum (`XOR`/`SUM`), refresh rate (Hz), endianness. Each section's enable flag controls whether its observed/computed CSV columns appear; blank expected value = log-only.

### `MessageDefinition` — [headers/MessageDefinition.h](headers/MessageDefinition.h)
A named message scoped to a UDP port: `messageName`, `port` (quint16), `payloadLengthBytes`, `fields` (`QList<FieldDefinition>`), `optionalHeader` (QByteArray), `hasCompareOptions` + `compareOptions`, `dataFormat` ∈ `{"HEX","NMEA"}` (default `"HEX"`), `nmeaSentenceType` (3-char formatter, e.g. `"GGA"`).
- When `dataFormat == "NMEA"`, extraction routes through `NmeaDecoder` and the message is matched by sentence formatter rather than exact byte length.
- `optionalHeader` non-empty ⇒ `packetMatchesMessage` also requires the leading bytes to match (lets two same-length messages on a port be disambiguated).

### `FilterConfiguration` — [headers/FilterTypes.h](headers/FilterTypes.h)
- `mode` — `FILTER_MODE_PORT = 0` or `FILTER_MODE_HEADER = 1`
- `commonPort`
- `filters` (`QList<MessageFilter>`) — each `{label, port, header (QByteArray)}`.

### `RawPacket` / `ParsedUdpPacket` — [headers/AppTypes.h](headers/AppTypes.h)
`RawPacket{packetNumber, tsSec, tsUsec, linkType, data}`; `ParsedUdpPacket{valid, timestamp, sourceIp, destinationIp, sourcePort, destinationPort, payloadSize, udpPayload, error}`.

### `ProjectState` — [headers/ProjectFile.h](headers/ProjectFile.h)
`appSchemaVersion`, `savedAtIso`, `pcapPath`, `inputMode`, `filterMode`, `filterCount`, `filterConfig`, `portMessagesByRow`, `headerFields`, `liveFields`, `liveFilterConfig`, `headerMessagesByRow`, `liveMessages`.

---

## 6. Key files & responsibilities

| File | Role |
|------|------|
| [sources/main.cpp](sources/main.cpp) | Entry point. `setOrganizationName`/`setApplicationName` both = `"PcapUdpExtractor"` (for `QSettings`/`QStandardPaths`), creates `MainWindow`. |
| [headers/MainWindow.h](headers/MainWindow.h), [sources/MainWindow.cpp](sources/MainWindow.cpp) | Top-level GUI. Holds **all session state** as members. ~2750 lines. |
| [headers/FieldConfigurationDialog.h](headers/FieldConfigurationDialog.h), [sources/FieldConfigurationDialog.cpp](sources/FieldConfigurationDialog.cpp) | Per-message field-table editor. CSV/JSON dropdown imports/exports + Template; per-row Edit buttons for bit / conditional decoders; **drag-and-drop** of a `.csv`/`.json` field-def file. |
| [headers/BitfieldDecoder.h](headers/BitfieldDecoder.h), [sources/BitfieldDecoder.cpp](sources/BitfieldDecoder.cpp) | Bit-rule JSON round-trip (`rulesToJson`/`rulesFromJson`), bit-position parsing, `validateRules`, per-row decode. |
| [headers/ConditionalBitfieldDecoder.h](headers/ConditionalBitfieldDecoder.h), [sources/ConditionalBitfieldDecoder.cpp](sources/ConditionalBitfieldDecoder.cpp) | Conditional decoders (`toJson`/`fromJson`). |
| `BitfieldRuleDialog`, `BitfieldDecoderDialog` | UI for editing bit rules. `BitfieldDecoderDialog` also has bulk CSV/JSON import/export (see §10.3). |
| `ConditionalProfileDialog`, `ConditionalBitfieldDecoderDialog` | UI for editing conditional profiles. |
| `MessageDefinitionDialog` | Edit a single message (name/port/length/**optional header**/**data format** HEX↔NMEA). |
| `MessageLengthFilterDialog` | Per-port/-row "manage length filters" dialog; per-row **Compare Options** button (§10.9); routes to NMEA field configurator when `dataFormat=="NMEA"`. |
| [headers/CsvExporter.h](headers/CsvExporter.h), [sources/CsvExporter.cpp](sources/CsvExporter.cpp) | Offline CSV writer (RFC-4180 quoting). Reusable `QByteArray` buffer in `appendEscapedCellUtf8` — mimic for new CSV writers. |
| `CsvStreamWriter` | Streaming CSV writer used by live mode (one per live message). |
| `PcapFileReader` | Reads pcap + pcapng. |
| `UdpPacketParser` | Parses raw packets → `ParsedUdpPacket`. |
| `ExtractionEngine` | Orchestrates file-mode extraction loop; `columnHeaders`, value decode/format incl. String. |
| `LiveUdpReceiver` | Live UDP socket receiver (emits `datagramReceived(QByteArray,QHostAddress,quint16,QDateTime)`). |
| `InputValidator` (+ `InputValidator_filters.cpp`) | Centralised validation: fields, filters, resolution expressions, ports, header hex, message filter counts. |
| `MathExpressionEvaluator` | Evaluates `resolutionExpression` strings (e.g. `raw*0.01`). |
| [headers/FieldCsvCodec.h](headers/FieldCsvCodec.h), [sources/FieldCsvCodec.cpp](sources/FieldCsvCodec.cpp) | CSV bulk import/export of field definitions + `dataTypeFromLabel`/`dataTypeToLabel`/`supportedDataTypeLabels`. Decoders never serialized. **Reused by the ICD importer for type-label resolution.** |
| [headers/ProjectFile.h](headers/ProjectFile.h), [sources/ProjectFile.cpp](sources/ProjectFile.cpp) | JSON project sidecar (`save`/`load`/`sidecarPathFor`/`exists`) + per-field-list JSON (`fieldListToJson`/`fieldListFromJson`, nested decoder objects). |
| [headers/BitRuleCsvCodec.h](headers/BitRuleCsvCodec.h), [sources/BitRuleCsvCodec.cpp](sources/BitRuleCsvCodec.cpp) | CSV bulk import/export of bit decoder rules (rows grouped by `Label`). Validates via `BitfieldDecoder::validateRules`. |
| [headers/Themes.h](headers/Themes.h), [sources/Themes.cpp](sources/Themes.cpp) | Dark/Light QSS. `Themes::apply(this)` in every window/dialog ctor after `setupUi`; `applyToAllTopLevels`; persists via `QSettings` (`ui/theme`). |
| [headers/CompareOptionsEngine.h](headers/CompareOptionsEngine.h), [sources/CompareOptionsEngine.cpp](sources/CompareOptionsEngine.cpp) | `compareColumnNames` + `compareRow` + `RefreshRateTracker` (rolling 1-s window). |
| `CompareOptionsDialog` | Modal config for the five compare sections. |
| **NMEA** — `NmeaTypes.h`, `NmeaSentenceRegistry` (.h/.cpp), `NmeaDecoder` (.h/.cpp), `NmeaSentencePickerDialog`, `NmeaFieldConfigurationDialog` | NMEA 0183 model, **87-formatter** built-in catalogue, pure decoder, formatter picker, per-field configurator. See §10.11. `NmeaSentenceRegistry.cpp` is generated (extend by appending a `NmeaSentenceDef`). |
| **ICD import** — [headers/IcdImportTypes.h](headers/IcdImportTypes.h), [headers/IcdDocxImporter.h](headers/IcdDocxImporter.h)/[.cpp](sources/IcdDocxImporter.cpp), [headers/IcdImportDialog.h](headers/IcdImportDialog.h)/[.cpp](sources/IcdImportDialog.cpp) + [forms/IcdImportDialog.ui](forms/IcdImportDialog.ui) | Word `.docx` → messages + fields. See §10.13 (flow) / §10.14 (`.ui` extraction). |

---

## 7. State held by MainWindow (volatile unless persisted via ProjectFile)

- `QList<FieldDefinition> m_headerFields` — header/standalone fields
- `QList< QList<MessageDefinition> > m_portMessagesByRow` — per port-filter row, messages configured for that port
- `QList< QList<MessageDefinition> > m_headerMessagesByRow` — per header-filter row length filters
- `QList<MessageDefinition> m_liveMessages` — global live-mode length filters (configured set), rendered in `tblLiveConfiguredMessages`
- `QList<QSpinBox*> m_portFilterBoxes`, `QList<QLineEdit*> m_headerFilterBoxes`, `QList<QPushButton*> m_headerLengthFilterButtons` — rebuilt by `rebuildFilterInputs()` on `spinFilterCount` change
- Live capture: `LiveUdpReceiver* m_liveReceiver`, `QTimer* m_livePreviewTimer`, `m_liveRunning`, `m_livePacketsReceived/Matched/ShortPackets`
- Live per-message writers: `m_activeLiveMessages` (snapshot at start), `QList<ExcelStreamWriter*> m_liveMessageWriters`, `m_liveMessageRowCounts`, `QList<RefreshRateTracker> m_liveCompareTrackers`
- `QString m_projectPath` — current `.pcproj.json` sidecar path (empty until Save As or sidecar-restore)

---

## 8. Conventions

- **Signals/slots:** old-style string-based `connect(obj, SIGNAL(foo()), this, SLOT(bar()))`. New-style functor connects are rare (one lambda in `FieldConfigurationDialog::setTypeCell`).
- **`byteOffset` is 1-based in the UI**, 0-based internally as `byteOffsetcorrect = byteOffset - 1`. Every conversion uses this.
- **Bit / conditional decoder JSON** lives in the field-name `QTableWidgetItem::UserRole` (bit) and `UserRole+1` (conditional) inside `FieldConfigurationDialog`. Serialise via `BitfieldDecoder::rulesToJson` / `ConditionalBitfieldDecoder::toJson` — never roll your own.
- **Forward-declare Qt classes** in headers (`class QSpinBox;`) to keep includes light.
- **C++ standard:** Qt 5 → `c++11`; Qt 6 → `c++17` (we don't ship Qt 6).
- **Encoding:** UTF-8. `QTextStream::setCodec("UTF-8")` guarded by `#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)`.
- **File dialogs** uniformly use `QFileDialog::getOpenFileName` / `getSaveFileName`.
- **Themes:** every top-level window/dialog ctor calls `Themes::apply(this)` after `setupUi(this)`.
- **Import-error UX pattern:** collect all row errors into a `QStringList`, show one `QMessageBox`, and leave existing state untouched on failure (see `FieldCsvCodec::importFromCsv`, the field/bit-rule importers, and `IcdImportDialog::onAccept`).
- **Menu actions:** `<action>`+`<addaction>` in `forms/MainWindow.ui` → slot decl in `MainWindow.h` → `connect()` in ctor → slot body appended at end of `MainWindow.cpp`.

---

## 9. Branch state & lineage

**Current working branch: `claude/eager-mendel-gnvtpr`** — stacked on `claude/practical-mendel-dM2RL` (the P1–P3b generic-ICD line). It contains every feature in the catalogue below, including the 2026-06-10 batch: checksum compute-range UI, ICD page-numbered table list + per-table preview, Excel `.xlsx` output everywhere, and the UI-polish/shortcuts pass (§10.21–10.23, §10.25). **Serial Mode (formerly §10.24) and a substantial amount of dead code were removed on 2026-06-10 at the user's request** — see §13 for the removal log.

Lineage (newest → oldest):
```
claude/loving-mayer-5P4Dw   ICD .docx import            (this branch)
  └ drag-and-drop-on-nmea    drag-and-drop of project + field-def files
      └ claude/nmea-support   NMEA 0183 as a per-message data format
          └ (remove-asterix)  ASTERIX feature stripped out (commit c494301)
              └ version8…v15   v8–v14 automation/self-save line + (removed) v15 ASTERIX
                  └ main        base extraction/parsing/live engine
```

**Present on this branch:** project save/restore, CSV field import/export, bit-rule bulk CSV/JSON, per-field JSON + hand-editing guide, String type, dark/light themes, header- & live-mode length filters, optional-header disambiguation, Compare Options verification, Live-mode UI cleanup, NMEA 0183, drag-and-drop import, ICD `.docx` import.

**Removed (do not reintroduce):** **ASTERIX** decoding (`Asterix*` files, `dataFormat == "ASTERIX"`) was removed on `remove-asterix` (commit `c494301`). The `dataFormat` selector now toggles `"HEX"` ↔ `"NMEA"` only. NMEA was modelled on the old ASTERIX scaffolding shape.

`main` is released through the multi-filter / user-defined-length refactor; all later features stack on top as described in §10.

---

## 10. Feature catalogue (all present on the current branch)

### 10.1 Project save / restore (sidecar JSON) — `ProjectFile`
- JSON sidecar `<pcap-basename>.pcproj.json` next to the pcap; fallback to `QStandardPaths::AppDataLocation` keyed by MD5 of the absolute pcap path when the pcap folder is read-only.
- Schema `"version": 1`: `appVersion`, `savedAt` (ISO UTC), `pcapPath`, `inputMode`, `filterMode`, `filterCount`, `filterConfig`, `portMessages`, `headerFields`, `live{fields, filterConfig}`, `headerMessages`, `live.messages`. Decoder configs round-trip via `BitfieldDecoder::rulesToJson` / `ConditionalBitfieldDecoder::toJson`.
- Triggers: File menu → Open/Save/Save As (Ctrl+O/S/Shift+S); silent save in `closeEvent`→`autoSaveProjectOnClose()`; restore prompt in `onBrowseClicked`→`tryRestoreProjectForPcap()`. Atomic write `<file>.tmp`→rename with `<file>.bak`.
- Helpers: `captureProjectState`, `applyProjectState`, `tryRestoreProjectForPcap`, `autoSaveProjectOnClose`, `loadProjectFromPath`.

### 10.2 CSV field import/export + template — `FieldCsvCodec`
- Three affordances in `FieldConfigurationDialog` (now a `CSV ▾` dropdown): Import / Export / Template. Per-message scope.
- Columns: `Name, ByteOffset, DataType, Length, Resolution, ResolutionExpression`. Header row case-insensitive, order-flexible; `#` and blank lines skipped; `Length` optional for fixed-size types.
- DataType labels: human (`Raw Unsigned BE`, `bool`, `uchar`, `char`, `ushort`, `short`, `uint`, `int`, `ulong`, `long`, `float`, `double`, `string`/`text`) + enum spellings (`Uint16`, `Int32`, …), case-insensitive.
- **Bitfield + conditional decoders are never CSV-serialized.** Import has Replace/Append/Cancel; all errors collected into one dialog; table untouched on failure.

### 10.3 Bit-mapping bulk import/export — `BitRuleCsvCodec` (on `BitfieldDecoderDialog`)
- CSV schema (one mapping/row, rows merged by `Label`): `Label,Bits,Reserved,UnknownBehavior,Enabled,Value,Binary,Meaning`. `Bits` uses `;` as the in-cell list separator (`0;1;2`) or ranges (`0-2`). `Binary` beats `Value`. Same `Label` rows must agree on `Bits/Reserved/UnknownBehavior/Enabled`.
- JSON schema = `BitfieldDecoder::rulesToJson` verbatim. Every imported list runs through `BitfieldDecoder::validateRules`. Single Export button; filter dropdown (CSV vs JSON) picks format. `ConditionalBitfieldDecoderDialog` is **not** modified.

### 10.4 Per-field JSON import/export + hand-editing — `ProjectFile::fieldListToJson/fieldListFromJson`
- `JSON ▾` dropdown in `FieldConfigurationDialog`. Exported shape nests `bitfieldDecoder`/`conditionalDecoder` as **objects** (human-editable), not stringified JSON; import accepts both. Top level: `{ "version":1, "kind":"PcapUdpExtractorFieldList", "exportedAt":…, "fields":[…] }`.
- Decoder-level failures are warnings (field still imports without that decoder); hard JSON errors abort. See `docs/EDITING_JSON.md`.

### 10.5 String data type
- Variable-length UTF-8. `InputValidator::validateFields` allows `length > 8` only when `dataType == String`. Decode: read `length` bytes at `byteOffsetcorrect`, trim trailing `0x00`, UTF-8 decode; `"N/A"` if out of bounds. Not eligible for bit decoding.

### 10.6 Dark / Light theme — `Themes`
- Top-row toggle button in the Input Mode group. Persists via `QSettings` (`ui/theme`). `Themes::apply` sets the widget stylesheet; runtime toggle re-applies via `applyToAllTopLevels`.

### 10.7 Length filters in header & live modes
- **Header mode:** each header-filter row has a "Manage Length Filters" button + status label; messages in `m_headerMessagesByRow`. On export, if `anyHeaderRowHasMessages()`, `onStartClicked` routes header-mode export through `exportByMessageDefinitions` (per-message CSV files).
- **Live mode:** global "Manage Length Filters"; messages in `m_liveMessages`. `startLiveCapture` delegates to `startLiveCaptureWithMessages` when non-empty (prompts for an output directory, opens one `CsvStreamWriter` per message). `onLiveDatagramReceived` routes via `tryRouteLivePacketByMessage`; `stopLiveCapture` closes all writers.

### 10.8 Optional header bytes — `MessageDefinition.optionalHeader`
- `MessageDefinitionDialog` "Optional Header (hex)" input (0–8 hex chars, even-length, validated). `MessageLengthFilterDialog` shows a header column. `packetMatchesMessage` checks leading bytes when non-empty. Duplicate-detection keys on `port + length + headerHex`, so two same-length messages on a port are allowed if their header signatures differ.

### 10.9 Compare Options verification — `CompareOptionsEngine` / `CompareOptionsDialog`
Per-message expected properties; observed/computed + OK/reason columns appended to CSV during extraction. Per-row "Configure" button in `MessageLengthFilterDialog`. Gated by `MessageDefinition.hasCompareOptions` (empty ⇒ pre-feature CSV layout).

**CSV column contract** (`compareColumnNames` / `compareRow` stay in lockstep):

| Section enabled | Always-emitted | Emitted only when expected present |
|---|---|---|
| Header | `HeaderObserved` | `HeaderExpected`, `HeaderOK` |
| Terminator | `TerminatorObserved` | `TerminatorExpected`, `TerminatorOK` |
| Checksum | `ChecksumComputed`, `ChecksumStoredInPayload`, `ChecksumOK` | (always — stored byte is the expected) |
| Refresh rate | `RefreshRateObservedHz` | `RefreshRateExpectedHz`, `RefreshRateOK` |
| Endianness (per multi-byte numeric field) | `<name>_BE`, `<name>_LE` | `<name>_EndianOK` |
| Endianness (once) | — | `EndianConfigured` |
| Any comparison active | — | `CompareReason` |

- Timestamps for refresh rate: file mode `tsSec*1000 + tsUsec/1000`; live mode `arrivalTimeUtc.toMSecsSinceEpoch()`. `RefreshRateTracker` is a `QQueue<qint64>` popping entries older than `now-1000ms`.
- **Endianness limitation:** `_EndianOK` = `"True" iff expectedEndianness=="BIG"` (the engine always decodes BE; both BE/LE reads are shown side-by-side for visual judgement).
- Live trackers: `m_liveCompareTrackers` parallels `m_activeLiveMessages`, sized at `startLiveCaptureWithMessages`, cleared at `stopLiveCapture`.

### 10.10 Live-mode UI cleanup
- Removed the pre-length-filter single-field Live surface. `filterGroup` hidden in Live mode. `tblLiveConfiguredMessages` (Name | Payload Length | Optional Header | Fields | Configure Fields) is backed by `m_liveMessages` via `refreshLiveConfiguredMessagesTable`. `startLiveCapture` requires `m_liveMessages` non-empty and delegates straight to `startLiveCaptureWithMessages` (per-message Excel writers). The dead single-writer path was deleted in the 2026-06-10 cleanup (§13).

### 10.11 NMEA 0183 (per-message data format)
- ASCII comma-delimited sentences `$aaccc,d1,..,dn*hh<CR><LF>` — `aa` talker, `ccc` formatter (GGA/RMC/…), `*hh` XOR checksum of chars between `$` and `*`. Variable length, null fields (`,,`).
- `MessageDefinition.dataFormat ∈ {"HEX","NMEA"}` + `nmeaSentenceType` (3-char formatter, may be a **custom** formatter). Fields addressed by **comma index** (`FieldDefinition.nmeaFieldIndex`, 1-based), not byte offset. `nmeaValueKind` is authoritative only for custom sentences.
- Files: `NmeaTypes.h` (data), `NmeaSentenceRegistry` (all **87** approved NMEA 0183 v3.01 §6.3 parametric formatters; curated names for common GNSS/nav, type-correct heuristic names for the rest; **generated** file), `NmeaDecoder` (`decodePacket(formatter,payload)`→records: split, XOR-validate, comma-parse; `formatValue()` for lat/lon/time/date previews), `NmeaSentencePickerDialog` (pick/Custom Formatter, 3 alnum), `NmeaFieldConfigurationDialog` (registry-driven Enable+Custom Label, or free-form editor for custom sentences).
- **Routing (gated on `dataFormat=="NMEA"`):** `packetMatchesMessage` = `port-match && payloadContainsNmeaFormatter(...)`, ignoring length/header; `exportByMessageDefinitions` / `tryRouteLivePacketByMessage` decode via `NmeaDecoder`, one CSV row per sentence record; field config dialogs open `NmeaFieldConfigurationDialog`; validation skips `InputValidator::validateFields` for a lightweight check (registry has formatter; each field has `nmeaFieldIndex>0` + a name). `buildNmeaRow` re-formats custom-sentence tokens by `nmeaValueKind`.
- **Scope:** parametric `$` sentences only (`!` AIS deferred). Talker not part of the match. Checksum validated/warned, not yet a CSV column.

### 10.12 Drag-and-drop import
- **MainWindow** (`dragEnterEvent`/`dropEvent`, `firstProjectFile`): dropping a `.pcproj.json` loads it as a project (= Open Project). Only `.pcproj.json` is accepted.
- **FieldConfigurationDialog** (`firstFieldDefFile`): dropping a `.csv` or `.json` field-definition file imports it (`importCsvFromPath`/`importJsonFromPath`, mirroring its Import buttons).
- Both are additive; neither `dropEvent` is touched by later features (e.g. the ICD importer uses a menu action, not drop, to stay additive).

### 10.13 ICD `.docx` import — `IcdDocxImporter` + `IcdImportDialog`
Bulk-define messages + fields from a Word ICD. Triggers: **File → Import ICD (.docx)…** (`actImportIcd`, Ctrl+I) **and** the **`btnImportIcd`** button in the Input Mode row (both `connect` to the same `onImportIcdClicked` slot). Works in File and Live mode. Strictly additive; nothing written until the user confirms.

**Pipeline (3 deterministic stages):**
1. **Extract** — `IcdDocxImporter::extract()` unzips the `.docx` with **`QZipReader`** (`QT += gui-private`, `#include <private/qzipreader_p.h>`) and walks `word/document.xml` with `QXmlStreamReader` into `IcdDocument` (`QList<IcdRawTable>` grids + each table's preceding heading; horizontal `gridSpan` padded, nested tables skipped). No guessing.
2. **Select & configure per table** — `IcdImportDialog` (`forms/IcdImportDialog.ui`, 3 boxes — see §10.15). Box 1 ticks the field tables; box 2 lists each selected table with a per-table **Settings** button → **`IcdTableSettingsDialog`** (own `.ui`) holding that table's **column mapping** (header row, **offset base 0/1**, Name/ByteOffset/DataType required + Length/Resolution/Expr optional; type labels via `FieldCsvCodec::dataTypeFromLabel`), **message identity** (blank name ⇒ ICD heading; default port), and **Table joining** (tick continuation tables to merge into this parent; untick / "Unmerge all" to undo). Mappings save/load as named JSON **profiles** under `AppDataLocation/icd_mapping_profiles` (`IcdDocxImporter::saveProfile`/`loadProfile`/`availableProfiles`/`profileToJson`/`profileFromJson`).
   - **Auto-detect (`IcdDocxImporter::suggestMapping`)** — content-aware, column-order-independent heuristic (pure/deterministic, no AI). Per table: scores the first ≤6 rows for role keywords → **header row**; samples each column's *data* cells → **DataType** (header "type/format/encoding" confirms); **Offset** = mostly-numeric column with the widest increasing/distinct range; **Length** = small repeating numeric column; **Name** = textual high-distinctness column; **offset base** from the offset column's min value. Returns -1 for unsure roles. Runs automatically when a table is first ticked (seeds `m_tableMapping[t]`) and on the settings dialog's **Auto-detect** button; always user-overridable.
3. **Build & review** — Build assembles `IcdTableGroup`s (parent + merged children) and calls `IcdDocxImporter::buildGroupedDrafts()` → `IcdMessageDraft`s; `QTreeWidget` of messages→fields with checkboxes (port/length/header inline-editable) + a per-message **Preview** button (shows the merged raw rows via `forms/IcdTablePreviewDialog.ui`) + warnings panel. On OK every kept message/field passes `InputValidator::validatePortValue`/`validateHeaderHexText`/`validateFields`; on any failure nothing commits. (`buildDrafts()`, the original one-table-per-message path, is retained but no longer called.)

**Data-model rules:** fields built like the manual dialog — `byteOffset` 1-based, `byteOffsetcorrect = byteOffset - 1` (offset-base aware), `dataFormat="HEX"`, `nmeaFieldIndex=0`, **no** bit/conditional decoders (never auto-created).

**Routing (`MainWindow::applyImportedMessages`):** Live → `m_liveMessages` (+`refreshLiveConfiguredMessagesTable`); File/header → `m_headerMessagesByRow[0]` (+`refreshHeaderLengthFilterStatus`); File/port → selected `tblPortFilters` row's `m_portMessagesByRow[row]`, row port stamped on (+`refreshPortFilterTable`+`refreshConfiguredMessagesTable`). Persistence is free (ordinary `MessageDefinition`s round-trip via `ProjectFile`).

**Library note:** `QZipReader` (Qt private) was chosen over vendored QuaZip/minizip — fully offline, zero new dependency, no GPL/LGPL beyond Qt. The unzip is isolated to `extract()`, so swapping backends later touches one function.

**Known limitations:** column mapping is **per table** now (§10.15) — each selected table maps independently and continuation tables inherit the parent's mapping; cross-table merges assume the merged tables share the parent's column layout; per-message port/length/header are mapping-default + tree-editable, not parsed from arbitrary metadata cells; tables only (no scanned/image tables, no legacy `.doc`); non-String fields keep the 1–8 byte length cap (flagged at build/commit). See `docs/ICD_DOCX_IMPORT.md`.

#### Verification status (ICD import) — BUILDS CLEAN on Windows kit
- **Built 2026-06-02 on Qt 5.10.1 / mingw53_32:** qmake + `mingw32-make` produce `build/release/PcapUdpExtractor.exe` (~909 KB, 32-bit — grew with the `btnImportIcd` button + `suggestMapping` auto-detect). `QT += gui-private` resolves `<private/qzipreader_p.h>` and `QZipReader` links against `libQt5Gui.a` with no errors. `suggestMapping` and the dialog auto-detect wiring compile warning-clean; only the two pre-existing `-Wunused-function` warnings remain (`fieldDataTypeValidationName`, `fieldBytesFromPayload`).
- E2E **pending**: click **Import ICD (.docx)…** button (or Ctrl+I) → confirm auto-detect fills the column mapping on table selection (try a byte-first and a name-first ICD) and the **Auto-detect columns** button re-runs it → Build/Preview → tick → OK → confirm in the active mode's configured-messages table → export over a matching pcap; save+reuse a mapping profile; Save Project → reload round-trip.

### 10.14 Dialog UI extracted to `.ui` forms (compact layouts)
Two hand-built UIs were moved out of C++ into Qt Designer forms. This is a **deliberate, user-approved exception** to the additive-only rule (§2 #3): behaviour is preserved, only the *construction mechanism* changed (same widgets, names, signals/slots).
- **`IcdImportDialog`** — the frame now lives in **`forms/IcdImportDialog.ui`** (compact reorg: 2-column mapping/identity grids, `lstTables` capped at 110px, `txtWarnings` at 70px, `tree` carries the layout stretch; opens ~900×720 to sit well inside 1920×1080). `buildUi()` no longer `new`s widgets — it binds the existing `m_*` member pointers to `ui->*`, keeps the review-tree `setColumnCount`/`setHeaderLabels`/`setEditTriggers` in code, and wires the same connects (locally-named buttons reached via `ui->btnBuild`/`btnSaveP`/`btnLoadP`/`btnAll`/`btnNone`/`buttonBox`). The header forward-declares `Ui::IcdImportDialog`, holds `Ui::IcdImportDialog* ui`, and adds `~IcdImportDialog()`→`delete ui`. **No public API change**; every slot still addresses widgets by `m_*`.
- **Header-filter rows** — each per-row `{label, hex QLineEdit, "Manage Length Filters" button, status label}` built in `MainWindow::rebuildFilterInputs()` now instantiates **`forms/FilterRowWidget.ui`** (`Ui::FilterRowWidget rowUi; rowUi.setupUi(row);`, then set `lblRow` text, bind `txtHeader`/`btnManageLengths`). The status label keeps `objectName == "lblHeaderLengthFilterStatus"` in the form so `refreshHeaderLengthFilterStatus()`'s `findChild` still locates it, and each row stays a **direct** child of `headerFilterBoxContainer`. (Port-filter "rows" are individual `QTableWidget` cell-widgets — a spinbox + button dropped into cells — so they have no single composite row widget to template and are intentionally left as-is.)

Both forms omit a baked stylesheet (like `NmeaFieldConfigurationDialog.ui`); `Themes::apply` styles at runtime. `.pro` gains `FORMS += forms/IcdImportDialog.ui` and `forms/FilterRowWidget.ui`.

#### Verification status (UI extraction) — STATIC-REVIEWED, Windows build PENDING
- Written in the Linux container (no Qt) → static checks only: both forms are well-formed XML; every `ui->name` ↔ `name=` parity verified; `rowUi.*` names present in `FilterRowWidget.ui`; member-init order matches declaration order; braces balanced in both edited `.cpp`.
- **Compile + E2E on the Windows Qt 5.10.1 / mingw kit is pending.** Verify: ICD dialog opens, auto-detect/map/Build/OK still commit; the dialog fits the screen; header-filter rows render, accept hex, open length filters, and show the correct "N messages"/"No length filters" status after filter-count edits and project restore.

### 10.15 ICD multi-table merge + per-table settings (3-box redesign)
A single logical ICD message is often split across several page-spanning Word tables, so the importer used to emit one message per table. This redesign lets continuation tables be **merged** into one message, with **per-table** column mapping. It **supersedes the `IcdImportDialog` parts of §10.14** (the dialog no longer holds the mapping combos or `m_*` widget members; it addresses widgets via `ui->*`). The core engine, `MessageDefinition`, and the dialog's public API (`setDocument`/`selectedMessages`) are unchanged — this is all pre-build transformation.

**New data + importer (additive; `buildDrafts`/`extract` untouched):**
- `IcdTableGroup` (`IcdImportTypes.h`) — `{ IcdMappingProfile mapping; QList<int> tableIndices; }`, parent-first.
- `IcdDocxImporter::buildGroupedDrafts(doc, groups, drafts, warnings)` — one draft per group; applies the **parent's** mapping to every member table and concatenates fields. **Per-group offset auto-detect:** a child whose lowest offset is below the running extent is *appended* (baseOffset = runningExtent); otherwise its offsets are kept *absolute*. Header/blank rows in children are skipped by the existing name-empty / non-numeric-offset rule (so a repeated header column row drops out). Helpers `appendFieldsFromTable` / `minCorrectedOffset` (file-local).
- `IcdDocxImporter::suggestContinuationGroups(doc, selected, parentOf)` — structural pre-merge: a selected table that is document-adjacent to the previous one, shares its column count, and has a blank/"…cont." heading becomes that table's child. Offsets are **not** consulted here.

**Dialog flow (`IcdImportDialog`, 3 boxes in `forms/IcdImportDialog.ui`):**
- **Box 1 `lstTables`** — tick field tables (`itemChanged` → `onTableSelectionChanged`). Newly ticked tables get an auto-detected mapping (`m_tableMapping[t]`) and start standalone; on the first non-empty selection `suggestContinuationGroups` seeds `m_parentOf` once (`m_autoSeeded`). De-ticking frees a parent's children.
- **Box 2 `tblSelected`** (Table | Status | Settings) — `refreshSelectedTablesTable()`. Status = `Standalone` / `Parent (N merged)` / `Merged into Table X`. The **Settings** button (cell widget, `property("tableIndex")`) is **disabled for merged children** (configure them from the parent) and opens `IcdTableSettingsDialog` for parents/standalone.
- **`IcdTableSettingsDialog`** (`forms/IcdTableSettingsDialog.ui`, `headers/`+`sources/`) — Column mapping (+per-table Save/Load profile, Auto-detect), Message identity, and a checkable Table-joining list of `candidateChildrenFor(parent)` (free standalone tables + this parent's current children). Returns `mapping()` + `mergedChildren()`; the import dialog updates `m_parentOf`/`m_tableMapping` and re-renders box 2.
- **Box 3** — `onBuildClicked()` → `buildGroups()` (one group per parent, children appended in doc order) → `buildGroupedDrafts` → review `tree` (5th column = a **Preview** button per message → `previewGroup()` shows the merged raw rows in `forms/IcdTablePreviewDialog.ui`, a class-less form instantiated like `FilterRowWidget`). `onAccept()` is unchanged in spirit (validate ticked rows → `m_result`).

State on `IcdImportDialog`: `m_selectedTables` (doc order), `QHash<int,int> m_parentOf` (table→parent, self=standalone), `QHash<int,IcdMappingProfile> m_tableMapping`, `bool m_autoSeeded`. `.pro` gains `sources/IcdTableSettingsDialog.cpp`, `headers/IcdTableSettingsDialog.h`, `forms/IcdTableSettingsDialog.ui`, `forms/IcdTablePreviewDialog.ui`.

#### Verification status (multi-table merge) — STATIC-REVIEWED, Windows build PENDING
- Written in the Linux container (no Qt): all four `.ui` well-formed + layout `stretch`/item counts match; `ui->`/`pv.` ↔ `name=` parity verified for all three dialogs; every declared method has a definition; braces balanced in `IcdImportDialog.cpp` / `IcdTableSettingsDialog.cpp` / `IcdDocxImporter.cpp`; `MainWindow` still uses only the unchanged `setDocument`/`selectedMessages` API.
- **Compile + E2E on the Windows Qt 5.10.1 / mingw kit is pending** (a large drop — expect a compile pass). Verify: load a multi-table ICD → continuation tables show pre-merged in box 2 → open a parent's Settings, map/auto-detect, tick/untick joins → Build → Preview shows merged rows → offsets correct for both absolute and restart-at-0 continuation layouts → tick → OK → messages land in the active mode → export over a matching pcap.

### 10.16 ICD tolerant review rows + editable DataType dropdown
ICD field rows must survive into the review tree even when the ICD is missing data, instead of being dropped at build. Column mapping for **Name / ByteOffset / DataType / Length is now fully optional** (each combo offers `(not mapped)`; `onBuildClicked` no longer hard-requires the three former "required" columns). Two builders now exist:
- **`IcdReviewDraftBuilder::buildGroupedDrafts`** ([sources/IcdReviewDraftBuilder.cpp](sources/IcdReviewDraftBuilder.cpp)) — the path `onBuildClicked` actually calls. Tolerant: a missing/unmapped/invalid Name/ByteOffset/DataType/Length/Resolution becomes an **empty string** on an `IcdFieldDraftRow` (with a warning) rather than a skipped row; only fully-blank rows and detected repeated-header rows (`looksLikeHeaderRow`) are dropped. Keeps the §10.15 per-group offset auto-detect. Produces `IcdMessageDraft.fieldRows` (string cells), leaving `message.fields` empty.
- `IcdDocxImporter::buildGroupedDrafts` (§10.15) is **retained but no longer called**.

**Data model:** `IcdMessageDraft` gained `QList<IcdFieldDraftRow> fieldRows`; `IcdFieldDraftRow` holds the per-row review **strings** (`name`, `byteOffsetText`, `lengthText`, `dataTypeText`, `resolutionText`, hidden `resolutionExpression`). `IcdImportTypes.h` documents the four columns as optional.

**Review tree (`populateReviewTree`/`onAccept` in [sources/IcdImportDialog.cpp](sources/IcdImportDialog.cpp)):** field rows are editable; the 5 columns reuse the message columns as Name / ByteOffset / Length / **DataType** / Resolution. **DataType is a per-row `QComboBox`** (`configureTypeCombo`: blank "not set" + the 13 predefined `FieldDataType` labels via `FieldCsvCodec::dataTypeToLabel`), set with `setItemWidget(ci, TREE_COL_HEADER, combo)` — this is the fix for "unmapped DataType throws because the app has predefined types": the user just picks one. `onAccept` is **tolerant** — `collectFieldFromItem` reads the type from the combo and routes any still-incomplete ticked field to a `skipped` list (skip the field, not block); a message with no complete field is skipped; a final `QMessageBox::question` lists the skips and imports the complete messages on confirm. Only message-identity edits (empty/duplicate name, bad port/len/header) remain blocking `errors`. `InputValidator::validateFields` still gates every imported message (now skip-with-note on failure).

**Bulk table select:** box 1 gained **Check All / Uncheck All** (`btnCheckAllTables`/`btnUncheckAllTables`) wired by Qt auto-connection (`on_btn..._clicked` slots live in [sources/IcdImportDialogTableButtons.cpp](sources/IcdImportDialogTableButtons.cpp), a second TU of `IcdImportDialog` methods). `.pro` gains `IcdReviewDraftBuilder.{h,cpp}` + `IcdImportDialogTableButtons.cpp`. See [docs/ICD_IMPORT_TOLERANT_REVIEW.md](docs/ICD_IMPORT_TOLERANT_REVIEW.md).

> Lineage note: the tolerant builder, editable rows and table buttons were added on this branch via the GitHub connector (no local compile); this entry's DataType-dropdown + tolerant-`onAccept` fix was added on top. **Whole ICD review path is Windows-build PENDING.**

### 10.17 Generic ICD automation — size-aware types, offsets-from-size, repeat-block replication (P1)
First phase of the "generic ICD" plan (`~/.claude/plans/see-the-test-files-purring-pearl.md`). Goal:
import **any** ICD `.docx` (PRTI 16-byte-header family, ICD-133F 1-byte-id family, …) with far less
hand-work. All additive; the extraction core is untouched.
- **Size-aware data-type resolver** — `FieldCsvCodec::dataTypeFromLabelAndSize(label,size,out)`
  ([sources/FieldCsvCodec.cpp](sources/FieldCsvCodec.cpp)): exact alias first, then keyword+size logic
  (`"Unsigned Integer"`/`"Unsigned Long"`/`"Uchar"`/`"Float"`/`"Signed Short"`… disambiguated by the Size
  column; ICD integers default unsigned unless "signed"). The exact alias table also gained the verbose/C
  spellings (`unsigned integer`, `byte`, `word`, `dword`, `boolean`, `real`, …). The review builder's
  `normalisedTypeText` now calls it (size from the Length cell), so far fewer fields land with empty DataType.
- **Offsets-from-size** (Feature D) — `IcdMappingProfile.autoOffsetFromSize` + `offsetStartByte`: the builder
  ignores the offset column and lays fields back-to-back from the Size column via a running cursor
  (continuous across merged tables). Reproduces the cumulative-offset style of `test_files/fieldsAT1.json`.
- **Repeat-block replication** (Feature B) — `IcdMappingProfile.repeatCount`/`repeatStrideBytes`/
  `repeatNamePattern`: after a group's base rows are built, `IcdReviewDraftBuilder::buildGroupedDrafts`
  clones the whole block `repeatCount` times at `stride` (0 = auto = base block extent), renaming via
  `applyRepeatPattern` (`{name}`/`{n}`). Solves the "8 identical Target blocks in one message" case.
  `IcdDocxImporter::suggestRepeatCount(table)` proposes N from "Target/Item/Channel N" phrasing; the
  Settings dialog pre-fills it.
- **UI**: `IcdTableSettingsDialog` ([forms/IcdTableSettingsDialog.ui](forms/IcdTableSettingsDialog.ui)) gained a
  **"Structure (offsets & repeated blocks)"** group (offsets-from-size + start byte; repeat count/stride/name
  pattern). `applyMappingToUi`/`collectMappingFromUi` round-trip the new `IcdMappingProfile` fields, which
  also persist via `IcdDocxImporter::profileToJson`/`profileFromJson`.
- **New profile fields** (`headers/IcdImportTypes.h`): `colDescription` (reserved for P2 enum decoders),
  `autoOffsetFromSize`, `offsetStartByte`, `repeatCount`, `repeatStrideBytes`, `repeatNamePattern` — all
  default to off/identity.

> **Deferred (later phases):** P2 = auto bit/enum decoders from a mapped Description column (`IcdEnumDecoder`)
> + per-row Edit-decoder in review; P3 = Live multicast `joinMulticastGroup` + opt-in ICD verification checks.
> **P1 compiles and runs on the Windows Qt 5.10.1/mingw kit.**

**Dialog sizing rule (must fit 1920×1080).** After P1 the ICD dialogs were over-tall (table-settings min
~928 px; the skip-confirmation `QMessageBox` grew past the screen listing every skipped field). Conventions
now applied — follow them for any new/edited dialog:
- Content-heavy dialogs wrap their body in a **`QScrollArea`** (`widgetResizable=true`, `NoFrame`) with the
  button box **pinned outside/below** it, so OK/Cancel are always reachable (see `IcdTableSettingsDialog.ui`).
- Keep inner list/tree/table/warnings `minimumSize` **small** and use `maximumSize` to cap them, so the
  layout minimum can't force the window taller than the screen (see `IcdImportDialog.ui`).
- Dialogs set a `maximumSize` height of **1000 px**; open height ≤ ~720.
- Long lists in a `QMessageBox` go in **`setDetailedText`** (collapsible "Show Details"), never the main
  `text` — see `IcdImportDialog::onAccept`.

### 10.18 Generic ICD automation — auto bit/enum decoders from a Description column (P2)
Second phase of the generic-ICD plan. Turns the enum prose in an ICD's Range/Description column
("`0x01 - MANUALLY`", "`1 - Enabled / 0 - Disabled`") into bit-decoder rules automatically, as a
**reviewable starting point** the user edits — never applied silently. Additive; extraction core untouched.
- **`IcdEnumDecoder`** ([headers/IcdEnumDecoder.h](headers/IcdEnumDecoder.h)/[.cpp](sources/IcdEnumDecoder.cpp)) —
  `rulesFromDescription(text,label) -> QList<BitDecodeRule>`. Finds `value [-=:)/dash] meaning` anchors
  (`0xNN` preferred over decimals; values ≤255; meaning must start with a letter so numeric ranges like
  "0-359.9 deg" are ignored); needs ≥2 mapped values. Emits one rule with low bits covering the max value
  (1 bit → SINGLE_BIT, else GROUPED_BITS). **Reuses the `BitDecodeRule` model + `BitfieldDecoder::rulesToJson`**.
- **Mapping**: `IcdMappingProfile.colDescription` (added P1) is now surfaced as a **Description column** combo
  in `IcdTableSettingsDialog` (gMap grid; auto-detected by keyword "description/range/meaning/…", and only
  overridden by an explicit stored mapping so the keyword pick survives). `fillCombosForTable`/collect/apply
  updated.
- **Builder**: `IcdReviewDraftBuilder::appendRowsFromTable` captures `IcdFieldDraftRow.descriptionText` and,
  when an enum is found, `IcdFieldDraftRow.bitRulesJson` (via `IcdEnumDecoder` + `rulesToJson`). Repeat-block
  clones copy both, so every replicated block gets the decoder.
- **Review/commit** ([sources/IcdImportDialog.cpp](sources/IcdImportDialog.cpp)): the review tree gained a
  6th **"Decoder"** column. Field rows with an auto-decoder or a description show an **Add/Edit decoder**
  button (`onEditFieldDecoderClicked`, found via `miIndex`/`ciIndex` properties) that opens the existing
  **`BitfieldDecoderDialog`** seeded with the rules; edits are stored back in the row's `FIELD_ROLE_BITS`
  data. `collectFieldFromItem` parses that JSON on OK and attaches `bitDecodeRules` + `hasBitfieldDecoder`
  to the built `FieldDefinition`.
- `.pro` gains `IcdEnumDecoder.{h,cpp}`. **P2 BUILDS CLEAN + launches on the Windows kit (verified 2026-06-03, Qt 5.10.1/mingw53_32; full clean rebuild exit 0). ICD import dialog opens, parses a real ICD-133F `.docx`, Build/Preview populates the review tree.** Full E2E extraction over a matching pcap not re-run.

### 10.19 Live multicast capture (P3a)
The ICDs are multicast (239.x.x.x) but `LiveUdpReceiver::start` only bound `AnyIPv4:port` — multicast
datagrams were never received. Additive fix:
- `LiveUdpReceiver` ([.h](headers/LiveUdpReceiver.h)/[.cpp](sources/LiveUdpReceiver.cpp)) gains
  `setMulticastGroup(QString)` + `m_multicastGroup`. When non-empty, `start()` binds with
  `ShareAddress|ReuseAddressHint` (so multiple subscribers coexist) and calls `joinMulticastGroup()` after
  bind; a failed join is **non-fatal** (emits `socketError`, unicast still works). Blank = unchanged unicast.
- `MainWindow` ([forms/MainWindow.ui](forms/MainWindow.ui)) adds a **"Multicast group"** `QLineEdit`
  (`txtLiveMulticast`) next to the bind port; `startLiveCapture` calls `m_liveReceiver->setMulticastGroup(...)`
  before either start path (single-writer or per-message), so both modes join the group.
- **P3a BUILDS CLEAN + launches on the Windows kit (verified 2026-06-03).** The **"Multicast group"** `QLineEdit` renders in Live mode next to the bind port. Multicast receive on a live source not yet exercised.

### 10.20 Opt-in ICD verification checks (P3b)
Two new **opt-in** Compare Options sections (default off) that catch wrong framing/extraction; they extend
the §10.9 system additively (new `CompareOptionsConfig` fields; columns appended **after** the endianness
block and **before** `CompareReason` so `compareColumnNames`/`compareRow` stay in lockstep). Reuse the
engine's `readStoredChecksum` (BE read) + `hexFixedWidth`.
- **Data-length field check** (`checkDataLength`, `dataLengthByteOffset`, `dataLengthSize`, `dataLengthAdjust`):
  reads a BE length field and compares to `payload.size() - dataLengthAdjust` (adjust = header bytes the
  field excludes). CSV: `DataLenStored`/`DataLenComputed`/`DataLenOK`.
- **Message-ID check** (`checkMessageId`, `messageIdByteOffset`, `messageIdSize`, `expectedMessageId`):
  reads a BE value at an offset and compares to an expected ID (works for a leading byte-1 ID *and* a
  mid-header byte-9 ID — the generic stand-in for mid-payload routing). CSV: `MsgIdObserved`/`MsgIdExpected`/`MsgIdOK`.
- UI: two checkable groups in `CompareOptionsDialog` ([forms/CompareOptionsDialog.ui](forms/CompareOptionsDialog.ui),
  now 820×640 capped at 1000) wired in `setConfig`/`config`/`hasCompareOptions`; the ID is entered/displayed as hex.
- Round-trips via `ProjectFile`'s `compareOptionsToJson`/`compareOptionsFromJson`.
- **P3b BUILDS CLEAN + launches on the Windows kit (verified 2026-06-03).** Both new checkable groups (Data-length, Message-ID) render in `CompareOptionsDialog` (full 7-section dialog fits the screen). This completes the generic-ICD plan (P1–P3). CSV columns for the two new checks not yet verified against a live extraction.

> **Build-verification note (2026-06-03):** a full clean `qmake` + `mingw32-make -j4` of the current branch (`claude/practical-mendel-dM2RL`) source completed **exit 0**, producing `build/release/PcapUdpExtractor.exe`. The exe launches and all dialogs render — so the P1–P3b work, previously "compiles/runs PENDING," **does compile and run** on the Windows kit. (CLAUDE.md §9's "current branch" still names the older `claude/loving-mayer-5P4Dw`; the live branch is `claude/practical-mendel-dM2RL`.) Remaining gap: end-to-end extraction over matching captures.

### 10.21 Checksum compute-range UI ("from byte X to byte Y")
The engine + JSON already carried `checksumRangeStart/End` (0-based, end-exclusive), but
`CompareOptionsDialog::config()` hardcoded `start=0, end=storedOffset`. Now exposed:
- [forms/CompareOptionsDialog.ui](forms/CompareOptionsDialog.ui) `grpChecksum` gained a second row:
  `spinCsumRangeFrom` ("Compute from byte", 1-based inclusive, default 1) + `spinCsumRangeTo`
  ("to byte", 1-based inclusive, `specialValueText` **Auto** at 0). Auto = classic behaviour
  (byte 1 up to the byte before the stored checksum), so old projects look and act unchanged.
- Mapping in `config()`: `rangeStart = from-1`; `rangeEnd = (to>0) ? to : storedOffset0` (a 1-based
  inclusive "to" equals the 0-based exclusive end). `setConfig()` shows Auto when the stored config
  equals the auto-derived range. `onSaveClicked` validates from/to bounds; the old "stored offset must
  be ≥2" error now applies only when "to byte" is Auto.
- Engine `computeChecksum` + ProjectFile round-trip were already range-aware — untouched.

### 10.22 ICD table list: page numbers + per-table Preview
- `IcdRawTable` gained `pageNumber` / `tableOnPage` (1-based). `IcdDocxImporter`'s document walker
  counts Word pagination markers (`<w:lastRenderedPageBreak/>`, `<w:br w:type="page"/>`,
  `<w:pageBreakBefore/>`) including inside table cells; the old `readElementText` calls were replaced by
  `readElementTextCountingBreaks` (identical text assembly, since readElementText would swallow the
  markers). Docs saved without cached pagination report page 1 for everything.
- **Box 1 of `IcdImportDialog` (`lstTables`) is now a 2-column `QTableWidget`** (same objectName):
  col 0 = checkable label `Page %1, Table %2: %3 [%4 rows x %5 cols]` (user-requested format; full
  title always in the tooltip), col 1 = per-row **Preview** button (`onTableListPreviewClicked` →
  `previewSingleTable`, reusing `IcdTablePreviewDialog.ui`) so truncated titles can be inspected before
  ticking. `itemChanged(QTableWidgetItem*)` now drives `onTableSelectionChanged`; the Check/Uncheck-All
  helpers in [sources/IcdImportDialogTableButtons.cpp](sources/IcdImportDialogTableButtons.cpp) iterate rows.
  `tableLabel()` carries the new format everywhere (box 2, settings, join list); box-2 status uses
  `tableShortRef()` ("Merged into Page 3 Table 2").

### 10.23 Excel `.xlsx` output everywhere (vendored QXlsx)
User-requested replacement of every extraction CSV with Excel (config import/export CSV/JSON stays).
- **Library:** QXlsx v1.4.10 vendored at `third_party/QXlsx/` (see §2 #2 + `docs/EXCEL_EXPORT.md`).
- **Wrappers:** [`ExcelExporter`](headers/ExcelExporter.h) mirrors `CsvExporter` (+`finalize(err)` because
  the workbook saves on close); [`ExcelStreamWriter`](headers/ExcelStreamWriter.h) mirrors
  `CsvStreamWriter` (`flush()` = the real save). Shared `ExcelExporter::cellVariant`: decimal numbers
  become numeric cells; hex/`N/A`/`True`/ >15-digit integers stay text. Bold blue header row.
- **Rewired surfaces (MainWindow):** partition structs hold `ExcelExporter*`; `defaultXlsxName`/
  `defaultLiveXlsxName`; `buildPartitionExportPath`/`buildMessageExportPath` (renamed from *CsvPath*,
  emit `.xlsx`); save dialogs filter `Excel Workbook (*.xlsx)`; `m_liveWriter` + `m_liveMessageWriters`
  are Excel writers; `closePartitions`/`closeMessagePartitions` gained an optional save-error collector
  surfaced in the summary; `closeLiveMessageWriters` warns when a workbook fails to save.
- **Caveat (documented):** xlsx can't append on disk — rows live in memory until stop/finish; killing
  the app mid-capture loses the unfinished workbook; a file open in Excel makes the final save fail.
- `CsvExporter`/`CsvStreamWriter` remain compiled (unused) for easy reversion.

### 10.24 (removed) Serial Mode
Serial Mode was added on 2026-06-10 then removed the same day at the user's request. See §13.

### 10.25 UI polish, tooltips, keyboard shortcuts
- **Themes ([sources/Themes.cpp](sources/Themes.cpp)):** both QSS palettes rebuilt — regular-weight
  "Segoe UI" 10pt (was bold Bahnschrift 11pt), 6–8px radii, calmer accents (#4c8dff), padded menus, and a
  styled **`QToolTip`** rule in each theme (dark: light-on-navy; light: soft cream) so tooltips are always
  readable at 1920×1080. **Default theme flipped to Light** (`QSettings` fallback "light"); stored choices
  win. The dark theme remains available via the toggle (Ctrl+T).
- **Shortcuts:** main window `QShortcut`s — Ctrl+1/2 modes, F5 mode-aware Start, Shift+F5 Stop, Ctrl+B
  Browse, Ctrl+T theme; existing Ctrl+O/S/Shift+S/Ctrl+I unchanged. New **Help menu → Keyboard
  Shortcuts (F1)** (`actShortcuts` → `onShowShortcutsHelp`, rich-text table).
  `FieldConfigurationDialog`: Insert = add row, Ctrl+E = edit, Ctrl+Delete = remove
  (`Qt::WidgetWithChildrenShortcut`). List: `docs/SHORTCUTS.md`.
- **Tooltips** added across MainWindow.ui (mode radios, filter spin, start/browse) and the new
  checksum-range spins; the box-1 ICD table list tooltips carry the full table title.

#### Verification status (10.21–10.25) — STATIC-REVIEWED, Windows build PENDING
Written in the Linux container (no Qt): braces/parens balanced in every touched file; all four edited
`.ui` files parse as XML; every new MainWindow method has decl+def; `ui->` name parity verified against
the `.ui` files; QXlsx v1.4.10 grep-audited for post-5.10 APIs (none found; `.pri` is c++11 +
core/gui-private). **Compile + E2E on the Windows Qt 5.10.1/mingw kit is pending.** Verify: clean qmake
(new modules + QXlsx .pri) → build; checksum range Auto/explicit; ICD list shows pages + Preview;
file/live/serial exports produce valid .xlsx (open in Excel); serial COM + replay; F1 help; light theme
default + readable tooltips; project round-trip incl. serial messages.

---

## 11. Common recipes

- **Add a property to `FieldDefinition`:** (1) extend the struct in `headers/AppTypes.h`; (2) surface in `FieldConfigurationDialog` (table columns, `collectFields()`, `refreshFieldTable()`); (3) round-trip in `ProjectFile.cpp` (`fieldToJson`/`fieldFromJson` and `fieldListToJson`/`FromJson`); (4) decide whether `FieldCsvCodec` and the ICD importer should expose it.
- **Add a menu action:** `<action>`+`<addaction>` in `forms/MainWindow.ui` → slot in `MainWindow.h` → `connect()` in ctor (group with the File-menu connects) → slot body at end of `MainWindow.cpp`.
- **Add a data type:** (1) extend `FieldDataType` + `fieldDataTypeNaturalLength()` in `AppTypes.h`; (2) combobox entry in `FieldConfigurationDialog::setTypeCell`; (3) labels in `FieldCsvCodec::dataTypeToLabel`/`dataTypeFromLabel` (+`kTypeLabels`/`supportedDataTypeLabels`); (4) JSON labels in `ProjectFile` `dataTypeToJsonString`/`dataTypeFromJsonString`; (5) decode logic in `ExtractionEngine` if new byte interpretation is needed.
- **Add an importer/exporter:** mirror the collect-all-errors-into-one-dialog + Replace/Append/Cancel + leave-state-untouched-on-failure pattern; reuse `FieldCsvCodec::dataTypeFromLabel` and `InputValidator` rather than re-validating by hand.

---

## 12. What NOT to do

- Do **not** refactor working extraction / parsing / decoding logic — it's validated against real captures.
- Do **not** pull in any further external dependency (the lone vendored library is QXlsx, §2 #2; the private-API uses are `QZipReader`/`QZipWriter`).
- Do **not** assume Qt 6 features exist.
- Do **not** change the `byteOffset` / `byteOffsetcorrect` 1-vs-0-based convention.
- Do **not** reintroduce ASTERIX.
- Do **not** reintroduce **Serial Mode** without explicit ask — it was added and removed on 2026-06-10 (§13).
- Do **not** commit unless explicitly asked.
- Do **not** "tidy up" existing slot bodies. Append at the end; never rewrite.

---

## 13. Removal log — 2026-06-10 cleanup

User explicitly asked to remove Serial Mode and "all dead code from the app which is not useful in any case for using". The additive-only rule (§2 #3) was waived for this commit so the codebase could be pruned.

**Removed entirely** (files deleted):
- **Serial Mode** — `headers/SerialPortReceiver.h`, `sources/SerialPortReceiver.cpp`, `docs/SERIAL_MODE.md`, plus every `serial*` slot, member, widget, layout, JSON field, and `QT += serialport` from the `.pro`. Ctrl+3 shortcut and `onSelectSerialMode` are gone. `applyImportedMessages` no longer has a serial branch. `ProjectState.serialMessages` and the `"serial"` JSON object are gone (loading a file that contains them silently ignores the key).
- **CSV writers** — `headers/CsvExporter.{h,cpp}`, `headers/CsvStreamWriter.{h,cpp}` (every output surface now uses `ExcelExporter`/`ExcelStreamWriter`; the comment block claiming the CSV writers "remain compiled for easy reversion" was a lie after this delete).
- **Dead live-mode legacy** — `m_liveFields`, `m_liveFilterConfig`, `m_liveWriter` (the single-writer), the `onConfigureLiveFieldsClicked` slot, `liveHeaderMatches()`, `extractLiveRowValues()`, the unreachable single-writer block in `startLiveCapture` (everything after the `if (!m_liveMessages.isEmpty())` early return), and the `else` branch in `refreshLivePreview` that read from `m_liveWriter`. `onLiveDatagramReceived` collapses to one line: `tryRouteLivePacketByMessage(...)`. `ProjectState.liveFields` / `liveFilterConfig` removed from `ProjectFile` save+load (load tolerates old files with those keys present).
- **Dead ICD path** — `IcdDocxImporter::buildDrafts` (the original one-table-per-message path, no longer called since the grouped/tolerant review path took over).
- **Dead unused helpers** — `fieldBytesFromPayload` in `MainWindow.cpp` (the real one lives in `ExtractionEngine.cpp`); `fieldDataTypeValidationName` in `InputValidator.cpp` (unused).
- **Stray non-source files** — `headers/you-know-the-whole-enumerated-pearl.md`, `docs/i-made-this-project-imperative-knuth.md`, `forms/MainWindowuiform.txt`, `forms/test.txt`.

**Kept on purpose:**
- `buildOutputHeaders` / `buildPreviewHeaders` / `buildLiveFieldHeaders` — all still on the live + file paths.
- `IcdDocxImporter::suggestContinuationGroups`, `suggestMapping`, `suggestRepeatCount`, profile save/load — still wired into the import dialog.
- The dark theme — switchable via the toggle / Ctrl+T even though Light is the default.

Verification: static checks only (Linux container, no Qt) — brace/paren balance, XML well-formedness, decl↔def parity, `ui->` name parity, `.pro` file existence, no stale identifiers. Windows Qt 5.10.1/mingw build + smoke test pending.
