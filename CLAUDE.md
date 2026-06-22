# CLAUDE.md — Universal Data Suite

Project memory for the **`universal-data-suite`** branch. Read this instead of re-exploring.

**Maintenance rule:** when you change architecture, the data model, the build, conventions, or branch
state, update this file *in the same change*. If you had to grep/read to relearn something, record it here.

---

## 1. What this is

One qmake project that builds **two separate desktop apps** (Qt 5.10 / C++11, Windows / mingw53_32)
from a shared core:

| App folder | Executable | Role |
|---|---|---|
| `app_parser/` | `UniversalWiresharkLogReader.exe` | **Reads/decodes** UDP/TCP data from a `.pcap`/`.pcapng` file or a live UDP/TCP stream into engineering values; exports CSV / Excel. |
| `app_simulator/` | `UniversalDataSimulator.exe` | **Transmits** user-defined HEX and NMEA-0183 messages over UDP, TCP, or a serial COM port. |

They are each other's natural test rig (the simulator sends; the parser's Live mode receives). The two
were previously separate branches (parser = `claude/eager-mendel-gnvtpr`, simulator =
`universal-data-simulator`); this branch merges their *files* into one tree but they remain two
distinct programs. **Do not merge this branch back into either original branch.**

---

## 2. Hard constraints — DO NOT VIOLATE

1. **Qt 5.10 only** (verified 5.10.1 / mingw53_32). No Qt-6 / post-5.10 APIs (e.g. `QUdpSocket` uses
   `error(QAbstractSocket::SocketError)`, not `errorOccurred`; no `setMarkdown`, no `Qt::SplitBehavior`,
   no `qsizetype`). `QSerialPort::errorOccurred` is fine (5.8+).
2. **Qt modules only** — `core gui widgets network serialport gui-private`. The only non-Qt code is the
   **vendored QXlsx** (MIT, `third_party/QXlsx`, linked by **both** apps) for Excel. `gui-private` is for
   `QZipReader` (ICD `.docx`).
3. **Encode/decode is a contract between the two apps** (§6). The simulator encodes the exact inverse of
   the parser's decode. Big-endian is the default; per-field Little-endian is opt-in.
4. Every user-facing failure carries a **reason AND a solution**.
5. **Do not commit unless asked.**

---

## 3. Build

```powershell
$env:PATH = 'D:\qt\5.10.1\mingw53_32\bin;D:\qt\Tools\mingw530_32\bin;' + $env:PATH
New-Item -ItemType Directory -Force build-suite | Out-Null
Set-Location build-suite
qmake ..\universal-data-suite.pro     # SUBDIRS: builds both apps
mingw32-make -j4
# app_parser\release\UniversalWiresharkLogReader.exe
# app_simulator\release\UniversalDataSimulator.exe
```

Build one app only: `qmake ..\app_simulator\simulator.pro` (or `..\app_parser\parser.pro`).

- `build-*/`, `*.o`, `*.exe`, `.qmake.stash` are gitignored.
- **`.pro`/`.pri` use `$$files(... *.cpp)` globbing.** qmake only re-scans the glob when a `.pro`/`.pri`
  changes, so **after adding/removing a source/header/form, do a fresh qmake** (delete the build dir or
  re-run qmake) or the new file is silently missed. Two pre-existing warnings (vendored QXlsx unused `q`,
  and an unused `defaultLiveXlsxName()` in MainWindow) are acceptable; everything else builds 0-warning.

---

## 4. Layout & how the build is wired

```
universal-data-suite.pro      TEMPLATE=subdirs -> app_parser/parser.pro, app_simulator/simulator.pro
shared/shared.pri             include()d by BOTH app .pro; adds the shared SOURCES/HEADERS/FORMS,
                              INCLUDEPATH (shared/headers) and assets.qrc. $$PWD = shared/.
  shared/headers, shared/sources, shared/forms
  shared/assets.qrc + shared/assets/chevron_{down,up}.png   (combo/spin dropdown arrows)
app_parser/{headers,sources,forms}    parser-only code + main.cpp; parser.pro adds QXlsx + parser_manual.qrc
app_simulator/{headers,sources,forms} simulator-only code + main.cpp; simulator.pro adds QXlsx + simulator_manual.qrc
third_party/QXlsx/            vendored Excel lib (both apps; QXlsx.pri self-locates)
docs/manual/                  *_manual.md (source) + generated *.html / *.docx + screenshots + make_manuals.py
```

The shared core is **compiled into each app** via the `.pri` (one source of truth, compiled twice).
A static lib was deliberately avoided to dodge Qt-static-lib uic/moc/resource fragility.

---

## 5. Shared vs app-specific (the merge map)

**Shared (`shared/`, one copy, used by both):** the data model (`AppTypes.h`, `MessageDefinition.h`),
`FieldCsvCodec`, `ExcelFieldCodec`, `MessageJsonCodec`, `InputValidator` (+`FilterTypes.h`),
`MathExpressionEvaluator`, the **connection model** (`ConnectionTypes.h` = `ConnectionDefinition` +
`makeConnectionId()`, `ConnectionJsonCodec`, `NetworkAdapterList` = numbered adapters + loopback), the
full **NMEA stack** (`NmeaTypes.h`, `NmeaSentenceRegistry`, `NmeaDecoder`, `NmeaSentencePickerDialog`),
**Themes**, the **ICD extraction layer** (`IcdImportTypes.h`, `IcdDocxImporter`, `IcdTableSettingsDialog`,
`IcdTablePickerDialog`), and the **`HelpManualDialog`**.

**Reconciled to a superset** (a file existed on both branches with different content):
- `AppTypes.h`, `MessageDefinition.h`, `FieldCsvCodec.cpp` → the **simulator's** versions. They already
  carry the parser's fields (decode structs, `compareOptions`, `optionalHeader`) **plus** the sender
  fields (`FieldEndianness endianness`, `sendValueText`, `sendFrequencyHz`, `sendEnabled`, `nmeaTalker`)
  and an optional CSV `Endianness` column. The parser ignores the sender fields; the simulator ignores
  the decode ones.
- `InputValidator` → the **parser's full** set (the simulator simply never calls the filter validators).

**App-specific (each app keeps its own copy — same class name is fine in separate folders/shadow builds):**
- Parser only: `MainWindow`, `ExtractionEngine`, `PcapFileReader`, `UdpPacketParser`, `LiveUdpReceiver`,
  `LiveTcpReceiver`, `ConfigureConnectionsDialog` (live-mode connection manager), `CompareOptions*`,
  `ProjectFile`, `Excel*`, the bitfield/conditional **decoder** classes + dialogs, `IcdEnumDecoder`,
  `FieldConfigurationDialog`, `MessageLengthFilterDialog` (now titled "Configure Messages"), `FilterRowWidget`.
- Simulator only: `SimulatorWindow`, `PayloadBuilder`, `DataSender`/`UdpDataSender`/`SerialDataSender`/
  `TcpDataSender`, `SimConnectionsDialog` (multi-destination connection manager + the `buildSender`
  factory; **replaced** the old single-link `ConnectionSettingsDialog`), `BitValueEditorDialog`,
  `SimSetupFile`, `SimFieldConfigurationDialog`.
- **Diverged, same class name, one copy per app:** `MessageDefinitionDialog` (parser = header/compare
  editor; sim = rate/format editor), `NmeaFieldConfigurationDialog` (sim adds a Value column),
  `IcdImportDialog` (+`IcdImportDialogTableButtons`, `IcdTablePreviewDialog.ui`) and
  `IcdReviewDraftBuilder` (parser keeps the Decoder column + enum-decode; sim strips them).

---

## 6. Data model & encode/decode contract

`shared/headers/AppTypes.h` — `FieldDefinition` carries, beyond name/byteOffset(1-based,
`byteOffsetcorrect=byteOffset-1`)/length/`dataType`/resolution: the parser's decode configs
(`bitDecodeRules`, conditional decoder, `nmeaFieldIndex`/`nmeaValueKind`) **and** the simulator's
`sendValueText` + `FieldEndianness endianness`. `MessageDefinition` carries the parser's
`optionalHeader`/`compareOptions`/`dataFormat`/`nmeaSentenceType` **and** the simulator's
`sendFrequencyHz`/`sendEnabled`/`nmeaTalker`, the shared `offsetUnit` (display-only: `"BYTES"`
default or `"WORDS"`, 1 word = 2 bytes; `field.byteOffset` is always in bytes), **and** the shared
`connectionId` (empty = unbound/default; otherwise the id of a `ConnectionDefinition`).

`AppTypes.h` also provides inline helpers: `offsetUnitIsWords()`, `byteOffsetToUnit()`, `unitToByteOffset()`.

**Multi-connection model (both apps).** A `ConnectionDefinition` (`shared/headers/ConnectionTypes.h`) is a
named transport endpoint — superset fields for both apps: `id`/`name`/`transport` (`"UDP"`/`"TCP"`/
`"SERIAL"`), adapter (`adapterName`/`adapterAddress` for the parser's bind), `port`, TCP `tcpRole`/`host`,
and serial settings (simulator). `MessageDefinition::connectionId` binds a message to one.
- **Parser (receive):** live mode has a **Configure Connections…** button → `ConfigureConnectionsDialog`
  (UDP = adapter + port only, no IP; TCP adds Listen/Connect). On Start, `MainWindow` spins up **one
  receiver per connection** into `m_liveSessionReceivers` + `m_receiverConnectionId`; each datagram is
  routed only against messages whose `connectionId` matches (unbound = any; empty connId on a legacy
  single-port receiver = all). `LiveUdpReceiver::setBindAddress` binds to the adapter IP (multicast joins
  on that adapter); `LiveTcpReceiver::start` takes a bind address for Listen. No connections defined =
  the legacy single Transport/Port row. Connections persist in `ProjectFile` (`live.connections`).
- **Simulator (send):** the connection bar's Configure… opens `SimConnectionsDialog` (list of UDP/TCP/
  serial destinations + per-connection **Test**). On Start Sending, `openSendersForPlan` opens + health-
  checks one `DataSender` per referenced connection into `m_openSenders` (keyed by id); each message
  transmits on `senderForMessage()` (its bound connection, or the first as default). Persist in
  `SimSetup.connections` (legacy single-destination setups synthesize one connection on load).
- The **Connection** column/combo in both apps' message tables does the binding; `ConnectionJsonCodec`
  serialises the list for both save formats.

**Encode (simulator, `app_simulator/.../PayloadBuilder.cpp`)** is the exact inverse of **decode (parser,
`app_parser/.../ExtractionEngine.cpp`)**:
- `raw = round(value / resolution)`, big-endian, `length` bytes; sign two's-complement at `length*8` bits;
  `float`/`double` fixed 4/8 bytes; `String` = UTF-8 NUL-padded.
- Per-field **Little-endian** reverses the numeric/float bytes only (String/Bool unchanged); a single
  helper applies it at the final wire-bytes step, so the bit editor stays big-endian internally.
- NMEA: `$` + talker + formatter + `,`-joined tokens + `*HH` XOR checksum + CRLF.

**Wide-field decode (reader only):** the simulator's encode is capped at 8 bytes (quint64). The reader
can now **decode fields of any length** — `ExtractionEngine` uses `unsignedBytesToDecimalString()` for
base-256→base-10 exact conversion and `unsignedBytesToDouble()` for resolution-scaled values. Two's
complement is handled for wide signed types. `InputValidator::validateField()` takes an optional
`maxNumericLength` parameter (default 8; reader passes `kNoNumericLengthCap = 0` to disable the cap).

**Field interchange codecs:** `MessageJsonCodec` serialises the **full union** of both apps'
FieldDefinition/MessageDefinition (including bit rules, conditional bits, compare options, send settings,
offsetUnit) into a single JSON format, lossless in both directions. `ExcelFieldCodec` reads/writes field
lists as `.xlsx` (same column layout as `FieldCsvCodec`). `FieldCsvCodec` remains for CSV.

---

## 7. ICD `.docx` import

Shared **extraction**: `IcdDocxImporter::extract` walks `word/document.xml` (via `QZipReader`) into an
`IcdDocument`; `suggestMapping` auto-detects columns + offset base. Each app's **own** `IcdImportDialog`
(2 boxes after the picker: selected tables w/ per-table `IcdTableSettingsDialog` → build & review) turns
it into `MessageDefinition`s. The parser's review keeps decoder derivation; the simulator's is send-only.

**Table picker** (shared `IcdTablePickerDialog`): a 1100×750 resizable dialog with a horizontal
`QSplitter` — left: checkable table list with Check/Uncheck All + summary; right: scrollable
`QTextBrowser` rendering every ICD table as HTML with anchors. Clicking a row scrolls the preview;
clicking a heading in the preview selects the row. A "Hide/Show Preview" button toggles the right pane.
On accept, the selected table indices flow back into `IcdImportDialog::applyTableSelection`, which seeds
mappings, auto-merges continuations, and populates step 2. Box 1 of the old dialog is now a one-line
summary ("N of M tables selected") with a "Select Tables…" button to re-open the picker.

---

## 8. Look & feel — `Themes`

Modern Light (default; bg `#F8FAFC`, card `#FFF`, indigo `#4F46E5`) + Slate Dark (`#0F172A`/`#1E293B`,
sky `#38BDF8`), toggle **Ctrl+T**. Combo/spin **dropdown arrows** are embedded chevrons
(`:/icons/chevron_*.png`). Accent-filled primary buttons by objectName (sim: `btnConnect`/
`btnStartSending`; parser: `btnStart`/`btnStartLive`; stop buttons red). `.ui` files carry no inline stylesheets — style by objectName in `Themes`.
(**Note:** the parser's `FieldConfigurationDialog.ui` previously had a baked-in dark stylesheet; it was
removed so `Themes::apply` controls appearance.) The parser's `main.cpp` re-applies the theme via
`QTimer::singleShot(0, …, applyToAllTopLevels)` after `show()` to defeat a startup repaint glitch.

---

## 9. In-app manuals & help (`HelpManualDialog` + `docs/make_manuals.py`)

- Source of truth: `docs/manual/{parser,simulator}_manual.md` (overview, getting started, per-panel
  walkthrough, **Common functions**, **Import from a Word ICD**, **Troubleshooting**, shortcuts,
  glossary) + screenshots in `docs/manual/{parser,sim}/`. Both manuals embed a **full screenshot set**
  of every major dialog/flow (parser: main light/dark/live/configured, output-preview, help, ICD
  import/picker/settings/review, connections, configure-messages, message-def, field-config, bitfield
  decoder/rule, conditional; sim: main light/dark/configured, help, connection-settings, configure-fields,
  bit-editor, message-def + NMEA def/picker/field-config, icd-import). The root `docs/manual/m-*.png`
  are an older, unused capture set.
- `python docs/make_manuals.py` (offline, python-docx) regenerates per manual: the in-app **HTML**
  (heading `id`/anchors so the dialog's TOC + search work), the **.docx**, and the per-app **`.qrc`**
  (HTML + that app's screenshots under `:/manual/`). Re-run it after editing a manual or adding shots.
- `shared/.../HelpManualDialog` = `QTextBrowser` + search (find next/prev, wrap) + TOC list +
  Back/Forward. Opened from **Help → User Manual (F1)** in both apps (shortcuts box = Shift+F1).
- Screenshot automation lives in PowerShell: dot-source `docs/manual/_uitools.ps1` (DPI-aware native
  helpers: `Get-TopWindows`, `Move-Window`, `Set-Foreground`, `Click-At`, `Send-Keys`,
  `Capture-WindowPrint`). Recipe: Qt on PATH, `Start-Process` the release exe, `Move-Window` to a fixed
  origin, `Capture-WindowPrint` (PrintWindow + PW_RENDERFULLCONTENT — works on background windows). Theme
  is Modern Light by default; toggle with `Ctrl+T` (no registry needed). Reach modal dialogs by clicking
  buttons at coordinates derived from a prior capture (`screen = window_origin + image_offset`); the
  modal then opens as a new top window you find + capture. To populate dialogs realistically, import
  `test_files/sample_icd.docx` (2 messages) and decode `test_files/sample_capture.pcap` (port 5000,
  32-byte payloads — set a message length to 32 and untick "Verify…" to get decoded rows).

---

## 10. Conventions

- Old-style `connect(obj, SIGNAL(...), this, SLOT(...))`; functor lambdas only where the originals had
  them (type/endian combo cells; the field table's deferred drag-reorder; `QTimer::singleShot`).
- `byteOffset` is **1-based** in the UI; `byteOffsetcorrect = byteOffset - 1` internally.
- `itemChanged` fires on programmatic writes — guard table refills with `m_refreshing`/`m_refreshingTable`.
- Errors: collect into a QStringList → ONE QMessageBox (`>4` entries → `setDetailedText`); each line =
  reason + "Solution: …". C-locale number parsing.
- `.ui`: no inline stylesheets; `<class>` == the C++ class name; `Themes::apply(this)` right after
  `setupUi`. Per-app `*_manual.qrc` is generated, not hand-edited.

---

## 11. Branch state & verification

Branch `universal-data-suite`, forked from the parser branch. Key commits after the initial merge:

1. `e691b34` — Part A (Sim UI rework: connection pop-out, messages up + Send/Stop, outgoing history,
   advanced bit grouping) + Part E (TCP: `TcpDataSender`/`ConnectionSettingsDialog` in sim,
   `LiveTcpReceiver` in reader, transport selector).
2. `e109867` — Part B (reader wide-field decode: 8-byte cap removed, `ExtractionEngine` extended;
   reader `FieldConfigurationDialog.ui` baked stylesheet removed) + Part C JSON (`MessageJsonCodec`,
   whole-message JSON import/export in both apps).
3. `51f0635` — Part D Bytes/Words (`offsetUnit` on `MessageDefinition`, selector in both field dialogs).
4. `54db3e4` — Part C Excel (`ExcelFieldCodec` shared; Excel import/export in both field dialogs;
   QXlsx linked into the simulator).
5. `f0268ef` — Documentation update (CLAUDE.md + both manuals + regenerated HTML/docx).
6. `98134a9` — Part D ICD picker (`IcdTablePickerDialog` shared; both apps' `IcdImportDialog`
   box 1 replaced with summary + "Select Tables…" button).
7. *(pending commit)* — **Full manual refresh**: both manuals rewritten with a complete embedded
   screenshot set (29 shots captured via `_uitools.ps1`), a new **Import from a Word ICD** walkthrough
   (incl. the table picker), expanded errors/solutions + glossary; regenerated HTML/docx/qrc and rebuilt
   both exes so the in-app Help boxes embed the new manuals. Also a 1-line `IcdTablePickerDialog.ui`
   layout fix (`rootLayout stretch="0,1,0"`).
8. *(pending commit)* — **Multi-connection suite** (see §6): shared `ConnectionTypes`/`ConnectionJsonCodec`/
   `NetworkAdapterList` + `connectionId` on every message; parser `ConfigureConnectionsDialog` + one
   live receiver per connection + per-message Connection binding; simulator `SimConnectionsDialog`
   (replaces `ConnectionSettingsDialog`) + `m_openSenders` routing + per-message Connection column;
   parser rename **Manage Length Filters → Configure Messages**; JSON Import/Export buttons inside the
   parser's Configure Messages dialog (and that dialog's baked stylesheet removed).

**Verified:** `qmake universal-data-suite.pro` + `mingw32-make -j4` builds both exes (0 errors,
reader ≈2.0 MB, sim ≈1.7 MB); only the 2 known pre-existing warnings. New files need a fresh qmake
(delete `build-suite/`) because the `.pro`/`.pri` globs only re-scan on a `.pro` change.

**E2E pending (manual):** parser ↔ simulator UDP/TCP round-trip; serial send; ICD table-picker →
build/review → import; Excel/CSV/JSON round-trips; per-field BE/LE on the wire; Bytes/Words offset
display; live-edit-while-streaming; **multi-connection: define 2+ parser connections on different
adapters/ports, bind messages, confirm no cross-mixing; define 2+ sim connections, bind messages,
confirm each sends to its own destination; setup/project save-load round-trips the connection list.**

---

## 12. Planned / requested work (remaining)

All original change-set items + the multi-connection suite have landed. Potential follow-ups:

- **Reader UI parity** — make the reader's main window layout match the simulator's cleaner design.
- Bytes/Words **auto-detect on ICD import** (small follow-up to the existing offsetUnit selector).
- **Multicast on a specific sim destination** and **per-connection live status** (currently the live
  status label is shared); **delete the now-unused `ConnectionSettingsDialog` manual page** references.
