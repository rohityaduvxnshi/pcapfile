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
`ExcelFieldCodec`, `MessageJsonCodec`, `FieldTypeLabels` (FieldDataType ⇄ label helpers, used by Excel +
ICD), `InputValidator` (+`FilterTypes.h`), `MathExpressionEvaluator`, the **connection model**
(`ConnectionTypes.h` = `ConnectionDefinition` + `makeConnectionId()`, `ConnectionJsonCodec`,
`NetworkAdapterList` = numbered adapters + loopback), the full **NMEA stack** (`NmeaTypes.h`,
`NmeaSentenceRegistry`, `NmeaDecoder`, `NmeaSentencePickerDialog`), **Themes**, the **ICD extraction
layer** (`IcdImportTypes.h`, `IcdDocxImporter`, `IcdTableSettingsDialog`, `IcdTablePickerDialog`), the
**`HelpManualDialog`**, **`AppPaths`** (standard Documents folders, below) and **`PcapWriter`**
(synthesizes Eth/IPv4/UDP|TCP frames + writes pcapng; reused by the simulator's pcapng export + packet
inspector).

> **CSV is gone (Excel-only).** `FieldCsvCodec` and the parser's `BitRuleCsvCodec` were **deleted**; field
> and bit-rule dialogs import/export **Excel (.xlsx) + JSON only**. The data-type label helpers that used
> to live on `FieldCsvCodec` now live on **`FieldTypeLabels`**. No `*.csv` UI or drop handling remains.

**Standard folders (`AppPaths`, both apps):** everything user-facing lands under
`<Documents>/UniversalDataSuite/`: `Output Files/` (default dir for every Export dialog — Excel, JSON,
pcapng) and `Projects/` (auto-saves + Save Project/Setup default). Created on demand; survives a clean
rebuild (PDF ICD import was evaluated and **deferred** — Qt 5.10 has no PDF module and PDF has no
table structure; `.docx` only).

**Reconciled to a superset** (a file existed on both branches with different content):
- `AppTypes.h`, `MessageDefinition.h`, `FieldCsvCodec.cpp` → the **simulator's** versions. They already
  carry the parser's fields (decode structs, `compareOptions`, `optionalHeader`) **plus** the sender
  fields (`FieldEndianness endianness`, `sendValueText`, `sendFrequencyHz`, `sendEnabled`, `nmeaTalker`)
  and an optional Excel `Endianness` column. The parser ignores the sender fields; the simulator ignores
  the decode ones.
- `InputValidator` → the **parser's full** set (the simulator simply never calls the filter validators).

**App-specific (each app keeps its own copy — same class name is fine in separate folders/shadow builds):**
- Parser only: `MainWindow`, `ExtractionEngine`, `PcapFileReader`, `UdpPacketParser`, `LiveUdpReceiver`,
  `LiveTcpReceiver`, `ConfigureConnectionsDialog` (live-mode connection manager), `CompareOptions*`,
  `ProjectFile`, `Excel*`, the bitfield/conditional **decoder** classes + dialogs, `IcdEnumDecoder`,
  `FieldConfigurationDialog` (now drag-reorder + multi-select + Alt+Up/Down, mirroring the simulator),
  `MessageLengthFilterDialog` (now titled "Configure Messages"), `FilterRowWidget`.
- Simulator only: `SimulatorWindow`, `PayloadBuilder`, `DataSender`/`UdpDataSender`/`SerialDataSender`/
  `TcpDataSender`, `SimConnectionsDialog` (multi-destination connection manager + the `buildSender`
  factory; **replaced** the old single-link `ConnectionSettingsDialog`), `BitValueEditorDialog`,
  `PacketInspectorDialog` (Wireshark-style view opened by double-clicking a history row),
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
- **Parser (receive):** the live-capture group in `MainWindow` shows **only** a **Configure Connections…**
  button + the status/packet-match grid — **all** transport settings (transport / adapter / port / TCP
  role / host / multicast) live **only** in `ConfigureConnectionsDialog` (no in-window Transport/Port row
  any more). Start Live **requires ≥1 connection** (clear error → Configure Connections… otherwise). On
  Start, `MainWindow` spins up **one receiver per connection** (`startSessionReceivers`) into
  `m_liveSessionReceivers` + `m_receiverConnectionId`; each datagram routes only against messages whose
  `connectionId` matches (unbound = any). Connections persist in `ProjectFile` (`live.connections`).
- **Simulator (send):** the connection bar's Configure… opens `SimConnectionsDialog` (list of UDP/TCP/
  serial destinations + per-connection **Test**). The UDP page now has a **Send via adapter** combo
  (`NetworkAdapterList`) → `ConnectionDefinition.adapterAddress`; `UdpDataSender::setBindAddress` binds
  the socket to that local interface (and sets the multicast send interface) so a multi-homed PC no longer
  only reaches loopback. On Start Sending, `openSendersForPlan` opens + health-checks one `DataSender` per
  referenced connection into `m_openSenders` (keyed by id). Persist in `SimSetup.connections`.
- **Simulator multi-destination (one message → many connections).** A simulator message can fan out to
  **several** connections at once. `MessageDefinition::connectionIds` (QStringList) holds the explicit
  ticked set; `connectionId` (single) is kept synced to its first id for back-compat/parser. Shared helper
  `messageConnectionIds(m)` resolves the effective list (multi-list → else single → else empty = default).
  The Messages-table **Connection** column is a **`CheckableComboBox`** (simulator-only QComboBox subclass
  in `app_simulator/{headers,sources}`: checkable popup that stays open on click, paints an "N selected"
  summary). `connectionsForMessage()` returns every bound+valid connection (dedup; first as default when
  none); `openSendersForPlan` opens a sender per distinct connection across **all** destinations;
  `sendActive` loops the payload to **each** destination (one frame + one history line each). The UDP
  datagram-size cap in `buildOneMessage` triggers if **any** destination is UDP. Connection changes apply
  on the next Start (a brand-new destination can't be opened mid-stream). The old singular
  `senderForMessage`/`connectionForMessage` were removed — all routing is plural.
- The **Connection** column/combo in both apps' message tables does the binding (simulator = multi-select);
  `ConnectionJsonCodec` serialises the connection list, and `connectionIds` round-trips through
  `SimSetupFile` **and** `MessageJsonCodec` (both lenient: derive from `connectionId` when the array is
  absent, so older files and parser-written JSON still load).

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

**Hex value entry (simulator).** `PayloadBuilder::rawFromTypedValue` accepts a `0x` prefix for **all**
integer types (unsigned and signed — for signed, the hex is the raw two's-complement bit pattern for the
field width). The field dialog's read-only **Hex** column shows the exact bytes as you type.

**Field interchange codecs:** `MessageJsonCodec` serialises the **full union** of both apps'
FieldDefinition/MessageDefinition (including bit rules, conditional bits, compare options, send settings,
offsetUnit) into a single JSON format, lossless in both directions. **Field-list import is lenient**:
`fieldsFromJson` no longer hard-fails on a blank field name (it auto-names `field_N` and returns the note
as a warning), so a file written by one app always loads in the other. `ExcelFieldCodec` reads/writes
field lists as `.xlsx`. Both apps surface JSON Import/Export by the message table (simulator: buttons next
to the table + File menu; parser: in Configure Messages).

---

## 7. ICD `.docx` import

Shared **extraction**: `IcdDocxImporter::extract` walks `word/document.xml` (via `QZipReader`) into an
`IcdDocument`; `suggestMapping` auto-detects columns + offset base. Each app's **own** `IcdImportDialog`
(2 boxes after the picker: selected tables w/ per-table `IcdTableSettingsDialog` → build & review) turns
it into `MessageDefinition`s. The parser's review keeps decoder derivation; the simulator's is send-only.

**Editable review + lenient field rules (both apps, identical logic).** The "Build & review" `tree` is
fully editable inline (name / offset / length / resolution cells + a DataType dropdown; edit triggers on).
On accept, `collectFieldFromItem` **skips a field only when ≥2 of the three unique keys {name, byteOffset,
type} are absent**; with at most one missing it fills a default (blank name → `field_<row>`, missing
offset → continues from the previous field's end via a running offset, missing type → `RawUnsignedBE`) and
notes it. Length is not a unique key (missing → the type's natural width, else 1). Duplicate field names
within a message are de-duplicated with a `_2`, `_3`, … suffix on the later one.

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
(**Note:** the parser's `FieldConfigurationDialog.ui` **and** `BitfieldDecoderDialog.ui` previously had
baked-in dark stylesheets; both were removed so `Themes::apply` controls appearance.) The parser's
`main.cpp` re-applies the theme via `QTimer::singleShot(0, …, applyToAllTopLevels)` after `show()` to
defeat a startup repaint glitch.
The shared input rule (`QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,…`) now carries **`min-height:22px`**
(both themes) so spin/combo text inside table cells is never vertically clipped.

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
  Back/Forward. Opened from **Help → User Manual (F1)** in both apps (shortcuts box = Shift+F1). The
  `QTextBrowser` is pinned to a **fixed light "document" page** (white bg, dark ink, 18px document margin)
  via its own stylesheet, independent of the app theme — otherwise Slate Dark's QSS painted dark text on
  a dark widget. The HTML CSS is tuned for Qt's rich-text engine (block margins for spacing — line-height
  is ignored; attribute-based table borders; padded code/pre/blockquote), and **`make_manuals.py` emits
  scaled `width`/`height` on every `<img>`** (Qt ignores `max-width`) so wide screenshots fit the pane
  instead of forcing a horizontal scrollbar.
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
  them (type/endian combo cells; **both** field tables' deferred drag-reorder; `QTimer::singleShot`; the
  simulator's 30 s autosave timer + UDP-adapter Refresh).
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
9. *(pending commit)* — **15-item hardening pass** (this change set). New shared `AppPaths`
   (Documents/UniversalDataSuite/{Output Files,Projects}), `PcapWriter` (Eth/IPv4/UDP|TCP synth + pcapng),
   `FieldTypeLabels`. **CSV deleted everywhere** (`FieldCsvCodec`, `BitRuleCsvCodec` removed; Excel+JSON
   only). Simulator: **UDP "Send via adapter"** bind (loopback fix), **hex value entry** for all int
   types, **JSON Import/Export buttons** by the message table, **Export pcapng (Ctrl+E)** of the sent
   history + **double-click `PacketInspectorDialog`** (Wireshark-style; needs the new raw-byte
   `m_sentRecords` buffer), **autosave → Projects** (+30 s timer, restore on launch). Parser:
   **live-mode UI = Configure Connections + status only** (transport row removed; live capture needs ≥1
   connection), **field table drag-reorder/multi-select/Alt+Up-Down**, **live-mode autosave → Projects**.
   Both: **ICD review editable + ≥2-missing skip rule + `_2` dup-name suffix**, **lenient `fieldsFromJson`**,
   **exports default to Output Files**, **input `min-height` clip fix**, ICD picker preview `showEvent`
   re-split. **PDF ICD import deferred** (no Qt 5.10 PDF; `.docx` only).
10. *(committed: `e1464bf`/`29b7a45`/`d2a79e6`)* — manuals refreshed for the 15-item set; **in-app HTML
    rendering** made robust (HelpManualDialog pins a fixed light "document" page so Slate Dark no longer
    paints dark-on-dark; CSS spacing; `make_manuals.py` emits scaled `<img>` width/height); **tooltip
    black-box fix** (dropped `border-radius` from the `QToolTip` rule in `Themes`).
11. *(pending commit)* — **Parser file-mode simplified to one Message Definitions box** (Reader UI parity).
    The whole **Message Filters** machinery is gone (multi-port filter rows, the Header signature filter
    mode, the standalone *Configure Header Fields* extractor, common port, `FilterRowWidget`,
    `m_portMessagesByRow`/`m_headerMessagesByRow`/`m_headerFields`). File mode now holds a single
    `QList<MessageDefinition> m_messages`; the main window has one editable table + **Add / Edit / Remove /
    Import JSON / Export JSON** (simulator-style). Each message carries its **own port** — the parser
    `MessageDefinitionDialog` gained a **UDP Port** field (and lost its baked stylesheet). `onStartClicked`
    just validates `m_messages` and calls the unchanged `exportByMessageDefinitions` (routing by
    port + length + optional header is preserved). `ProjectState` gained a flat `messages` array;
    `applyProjectState` **migrates** older projects by flattening `portMessagesByRow`/`headerMessagesByRow`
    (stamping each row's port) into `m_messages`.
12. *(pending commit)* — **Simulator word-offset save fix** (`SimFieldConfigurationDialog::collectFields`
    now runs the typed offset through `unitToByteOffset` like every other read path, so a WORDS offset no
    longer drifts on save/reopen) — committed `c6a5712`. Plus **one message → many connections** (see §6):
    new `MessageDefinition::connectionIds` + `messageConnectionIds()` helper, new simulator
    `CheckableComboBox`, the Messages-table Connection column is now multi-select, send routing fans out to
    every bound destination, and `connectionIds` round-trips through `SimSetupFile` + `MessageJsonCodec`.

**Verified:** clean `qmake universal-data-suite.pro` + `mingw32-make -j4` builds both exes (0 errors;
reader ≈2.5 MB, sim ≈1.9 MB); both launch without crashing; the standard folders auto-create under
Documents. **pcap round-trip proven**: a `PcapWriter` UDP frame re-parses through the reader's own
`PcapFileReader` + `UdpPacketParser` with exact payload/IPs/ports (so Wireshark reads it too). New files
need a fresh qmake (delete `build-suite/`) because the `.pro`/`.pri` globs only re-scan on a `.pro` change.

**E2E pending (manual):** parser ↔ simulator UDP/TCP round-trip; serial send; ICD table-picker →
build/review → import; Excel/JSON round-trips; per-field BE/LE on the wire; Bytes/Words offset display;
live-edit-while-streaming; multi-connection no cross-mixing; **item-specific:** simulator sends out a real
LAN adapter (not loopback); hex value entry shows correct bytes; pcapng export opens in Wireshark **and**
re-parses in the reader; double-click history → inspector; parser field-table drag-reorder keeps decoders;
ICD import keeps a 1-key-missing field, skips a 2-key-missing one, renames dup names `_2`; live + simulator
autosave restore on relaunch from the Projects folder.

---

## 12. Planned / requested work (remaining)

All original change-set items, the multi-connection suite, and the 15-item hardening pass have landed.
Potential follow-ups:

- **Regenerate the manuals** — the docs under `docs/manual/` still describe CSV, the old parser live-mode
  Transport/Port row, the removed Message Filters section, and lack the new pcapng export / packet
  inspector / hex-entry / adapter-send / single Message Definitions box. Re-shoot the affected
  screenshots and re-run `python docs/make_manuals.py`, then rebuild so the in-app Help embeds them.
- Bytes/Words **auto-detect on ICD import** (small follow-up to the existing offsetUnit selector).
- **PDF ICD import** — deferred; would need Qt 5.14+/PDFium (violates the no-external-lib constraint) and
  heuristic table detection. An `.xlsx` ICD importer (QXlsx already linked) is the cheaper alternative.
- **Per-connection live status** in the parser (the status label is still shared across receivers).
