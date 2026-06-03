# PROJECT_MINDMAP — PcapUdpExtractor

> **Purpose.** A function-level map of the entire codebase: every function, **where it is defined** (`file:line`) and **where/why it is called** (its triggers and call chains). Read this together with [CLAUDE.md](../CLAUDE.md) (architecture, data model, feature catalogue, constraints). Between the two, an AI or developer can understand the whole project without re-exploring it.
>
> **How to read the notation**
> - `Foo::bar()  ⟨sources/Foo.cpp:NN⟩` — definition site.
> - `→ X, Y` — *calls / depends on* (downstream).
> - `← X, Y` — *called by / triggered by* (upstream). For slots, the trigger is a Qt signal wired in a `connect(...)`.
> - `⊕` marks a Qt **slot**; `Σ` marks a Qt **signal**; `▣` marks a **file-local** (anonymous-namespace / `static`) helper, not a class method.
> - Line numbers are accurate as of the branch in CLAUDE.md §9 (`claude/loving-mayer-5P4Dw` lineage). They drift as code is appended; treat them as "near here," and the function name as authoritative.

---

## 0. The 60-second model

```
                          ┌──────────────────────────────────────────────┐
   pcap/pcapng file  ─┐   │            MainWindow (the conductor)          │
   live UDP socket   ─┼──▶│  holds ALL session state, owns every dialog,   │
   Word .docx (ICD)  ─┘   │  routes File-mode / Live-mode / ICD-import     │
                          └───────────────┬───────────────┬────────────────┘
                                          │               │
        ┌─────────────────────────────────┘               └───────────────────────┐
        ▼                                                                          ▼
  READ / PARSE layer                                                       DEFINE / CONFIGURE layer (dialogs)
  PcapFileReader  ──▶ RawPacket                                            FieldConfigurationDialog
  UdpPacketParser ──▶ ParsedUdpPacket                                      MessageDefinitionDialog
  LiveUdpReceiver ──Σ datagramReceived                                     MessageLengthFilterDialog
        │                                                                  Bitfield/Conditional decoder dialogs
        ▼                                                                  CompareOptionsDialog
  MATCH layer:  packetMatchesMessage / matchingFilterIndex                 NMEA picker + field dialogs
        │                                                                  IcdImportDialog (+ sub-dialogs)
        ▼                                                                          │
  DECODE layer                                                                     ▼
  ExtractionEngine (HEX) · NmeaDecoder (NMEA)                              DATA MODEL  (headers/AppTypes.h,
  BitfieldDecoder · ConditionalBitfieldDecoder · MathExpressionEvaluator    MessageDefinition.h, FilterTypes.h)
  CompareOptionsEngine (verification)                                      FieldDefinition · MessageDefinition
        │                                                                  BitDecodeRule · CompareOptionsConfig
        ▼                                                                          │
  WRITE layer:  CsvExporter (offline) · CsvStreamWriter (live)                     ▼
        │                                                                  PERSIST: ProjectFile (.pcproj.json),
        ▼                                                                  FieldCsvCodec, BitRuleCsvCodec,
   CSV files on disk  ◀── live preview table / file preview table         IcdDocxImporter mapping profiles
```

**Five processing pipelines** (traced in §9):
1. **File-mode legacy export** — header/standalone fields → one CSV per filter.
2. **File-mode per-message export** — `MessageDefinition`s → one CSV per message (HEX + NMEA + Compare Options). *Primary path.*
3. **Live capture** — socket datagrams → live CSV(s) + rolling preview.
4. **ICD `.docx` import** — Word doc → reviewed `MessageDefinition`s injected into the active mode.
5. **Project save/restore** — full session ⇄ JSON sidecar.

---

## 1. Layer map → files → responsibility

| Layer | Files | One-line responsibility |
|---|---|---|
| **Entry** | `sources/main.cpp` | `QApplication`, set org/app name, show `MainWindow`. |
| **Conductor** | `MainWindow.{h,cpp}` | All session state; mode routing; owns dialogs; export & live loops; project I/O; ICD trigger. |
| **Read/Parse** | `PcapFileReader`, `UdpPacketParser`, `LiveUdpReceiver` | pcap/pcapng → `RawPacket`; → `ParsedUdpPacket`; live socket → datagram signal. |
| **Decode (HEX)** | `ExtractionEngine`, `BitfieldDecoder`, `ConditionalBitfieldDecoder`, `MathExpressionEvaluator` | payload bytes → CSV cell strings; bit/enum & conditional decoders; resolution formulas. |
| **Decode (NMEA)** | `NmeaDecoder`, `NmeaSentenceRegistry`, `NmeaTypes.h` | NMEA 0183 sentence → records; 87-formatter catalogue. |
| **Verify** | `CompareOptionsEngine` | header/terminator/checksum/refresh-rate/endianness/data-length/message-id checks → extra CSV columns. |
| **Write** | `CsvExporter`, `CsvStreamWriter` | RFC-4180 CSV (offline / streaming). |
| **Validate** | `InputValidator` (+ `_filters.cpp`) | central validation of fields/filters/ports/hex/expressions. |
| **Persist** | `ProjectFile`, `FieldCsvCodec`, `BitRuleCsvCodec`, `IcdDocxImporter` (profiles) | project sidecar; CSV/JSON field & bit-rule codecs; ICD mapping profiles. |
| **ICD import** | `IcdDocxImporter`, `IcdReviewDraftBuilder`, `IcdEnumDecoder`, `IcdImportDialog`(+`TableButtons`), `IcdTableSettingsDialog`, `IcdImportTypes.h` | `.docx` → grids → mapped/merged drafts → reviewed messages. |
| **Dialogs** | `FieldConfigurationDialog`, `MessageDefinitionDialog`, `MessageLengthFilterDialog`, `Bitfield*Dialog`, `Conditional*Dialog`, `CompareOptionsDialog`, `Nmea*Dialog` | per-message / per-field editors. |
| **Theming** | `Themes` | dark/light QSS, applied per window. |

---

## 2. `main` + `MainWindow` — the conductor

### 2.1 Entry
- `main(argc,argv)  ⟨sources/main.cpp:12⟩` → sets `QApplication`, `setOrganizationName/setApplicationName = "PcapUdpExtractor"`, constructs & shows `MainWindow`.

### 2.2 MainWindow lifecycle & wiring
- `MainWindow::MainWindow(parent)  ⟨MainWindow.cpp:242⟩` → `ui->setupUi`, `Themes::apply`, sets table headers/ranges, `m_headerFields = defaultFields()`, creates `LiveUdpReceiver` + preview `QTimer`, **wires every connect** (see §8), then `rebuildFilterInputs()`, `onFilterModeChanged()`, `onInputModeChanged()`, `setLiveUiState(false)`, `refreshLiveConfiguredMessagesTable()`, `setStatus(...)`.
- `~MainWindow()  ⟨:347⟩` → `stopLiveCapture()` if running; `delete ui`.
- `closeEvent(e)  ⟨:354⟩  ⊕(override)` → `stopLiveCapture()` if running → `autoSaveProjectOnClose()` → base.
- `dragEnterEvent / dropEvent  ⟨:2615 / :2623⟩  ⊕(override)` → accept a dropped `.pcproj.json` → `firstProjectFile()` → `loadProjectFromPath()`.

### 2.3 MainWindow file-local helpers ▣ `⟨MainWindow.cpp:51–240⟩`
| Helper | Line | Role |
|---|---|---|
| `PORT_COL_* / MESSAGE_COL_*` consts | 53–61 | column indices for the port & configured-message tables |
| `s_livePreviewAppendSeq / s_liveRenderedSeq` | 68–69 | live-preview incremental-render counters |
| `struct OutputPartition` | 71 | per-filter CSV writer bundle (legacy export) |
| `struct MessageOutputPartition` | 81 | per-message CSV writer bundle (+ `MessageDefinition`) |
| `safeName(text)` ▣ | 91 | sanitise text → filesystem-safe token |
| `defaultCsvName(input)` ▣ | 104 | `<base>_<date>_<time>.csv` |
| `defaultLiveCsvName()` ▣ | 113 | `liveCapture_<ts>.csv` |
| `clearVBox(layout)` ▣ | 119 | delete all widgets/items in a vbox (filter-row rebuild) |
| `closePartitions / closeMessagePartitions` ▣ | 132 / 145 | close+delete exporters |
| `fieldBytesFromPayload` ▣ | 158 | bounds-safe payload slice (local copy; ExtractionEngine has its own) |
| `buildNmeaRow(record,fields)` ▣ | 174 | NMEA record → CSV row in `columnHeaders` order; custom sentences re-format by `nmeaValueKind` → `NmeaDecoder::formatValue` / `record.valueAt` |
| `payloadContainsNmeaFormatter(payload,fmt)` ▣ | 194 | scan payload for `$xx<FMT>` |
| `packetMatchesMessage(parsed,msg)` ▣ | 214 | **the match predicate**: port-match; NMEA→formatter scan; else exact length + optional-header prefix |

### 2.4 MainWindow slots & methods by concern

**Mode / filter UI**
| Function (⟨MainWindow.cpp:line⟩) | Trigger / called-by | Calls |
|---|---|---|
| `onInputModeChanged() ⊕ :392` | ← `radFileMode/radLiveMode toggled` | show/hide File vs Live groups, `filterGroup` hidden in Live |
| `onFilterModeChanged() ⊕ :379` | ← `radPortFilter/radHeaderFilter toggled` | `rebuildFilterInputs()` |
| `onFilterCountChanged(n) ⊕ :373` | ← `spinFilterCount valueChanged` | `rebuildFilterInputs()` |
| `rebuildFilterInputs() :465` | ← ctor, the three slots above, `applyProjectState` | builds port-table rows **or** `FilterRowWidget.ui` header rows; wires per-row `onPortValueChanged`, `onManageLengthFiltersClicked`, `onManageHeaderLengthFiltersClicked`; `refreshPortFilterTable`, `refreshHeaderLengthFilterStatus` |
| `clearPortFilterBoxes / clearHeaderFilterBoxes :454/459` | ← `rebuildFilterInputs` | tear down old row widgets |
| `onPortValueChanged(v) ⊕ :411` | ← per-row port spinbox | stamp port onto that row's messages; `refreshPortFilterTable` |
| `refreshPortFilterTable() :545` | ← several | repaint port table + message counts |
| `refreshConfiguredMessagesTable() :567` | ← port-row selection / edits | fill `tblConfiguredMessages`; per-row **Configure Fields** btn → `onConfigureMessageFieldsClicked` |

**Length-filter dialogs (define messages)**
| Function | Trigger | Calls |
|---|---|---|
| `onManageLengthFiltersClicked() ⊕ :428` | ← port-row "Manage Length Filters" | `openLengthFilterDialogForPortRow(row)` |
| `openLengthFilterDialogForPortRow(row) :597` | ↑ | `MessageLengthFilterDialog` (setPort/setMessages → messages); store into `m_portMessagesByRow[row]`; refresh |
| `onManageHeaderLengthFiltersClicked() ⊕ :2144` | ← header-row button | `openHeaderLengthFilterDialogForRow(row)` |
| `openHeaderLengthFilterDialogForRow(row) :2156` | ↑ | dialog → `m_headerMessagesByRow[row]`; `refreshHeaderLengthFilterStatus` |
| `onManageLiveLengthFiltersClicked() ⊕ :2151` | ← live "Manage Length Filters" | `openLiveLengthFilterDialog()` |
| `openLiveLengthFilterDialog() :2184` | ↑ | dialog → `m_liveMessages`; `refreshLiveConfiguredMessagesTable` |
| `refreshHeaderLengthFilterStatus :2208`, `refreshLiveLengthFilterStatus :2231`, `anyHeaderRowHasMessages :2239` | ← many | status labels / predicate |

**Field configuration entry points**
| Function | Trigger | Calls |
|---|---|---|
| `onConfigureMessageFieldsClicked() ⊕ :435` | ← `tblConfiguredMessages` per-row btn | `openFieldConfigurationForMessage(idx)` |
| `openFieldConfigurationForMessage(idx) :624` | ↑ | HEX→`FieldConfigurationDialog`; NMEA→`NmeaFieldConfigurationDialog`; writes back into the message's `fields` |
| `onConfigureHeaderFieldsClicked() ⊕ :442` | ← `btnConfigureHeaderFields` | `configureFieldList(m_headerFields,…)` |
| `onConfigureLiveFieldsClicked() ⊕ :448` | (legacy; unwired — back-compat only) | `configureFieldList(m_liveFields,…)` |
| `onConfigureLiveMessageFieldsClicked() ⊕ :2560` | ← live configured-messages per-row btn | HEX/NMEA field dialog → `m_liveMessages[i].fields` |
| `configureFieldList(fields,len,title) :671` | ↑ several | opens `FieldConfigurationDialog`, returns edited list |
| `defaultFields() :921`, `fieldStatusText() :936`, `refreshStandaloneFieldStatus() :1908` | ← ctor / refresh | seed + summarise field lists |

**File-mode export** — see full trace in §9.1–9.2
| Function | Role |
|---|---|
| `onBrowseClicked() ⊕ :362` | ← `btnBrowse`; file dialog → `txtFilePath`; `tryRestoreProjectForPcap()` |
| `onStartClicked() ⊕ :687` | ← `btnStart`; **the file-mode brain** (validates, branches port/header/legacy) |
| `collectFilterConfiguration(cfg,err) :959` | gather `FilterConfiguration` from UI → `InputValidator::validateFilterConfiguration` |
| `collectMessageDefinitions() :983` | flatten `m_portMessagesByRow` → list |
| `collectHeaderModeMessageDefinitions(port) :2249` | flatten `m_headerMessagesByRow`, stamp common port |
| `validateMessageDefinitions(msgs,err) :1002` | per-message validation (name/port/len/header/fields; NMEA-light path) |
| `validateMessagesExistInCapture(msgs,err) :1145` | opt-in pre-scan of the pcap for each message |
| `exportByMessageDefinitions(msgs,err) :1213` | **per-message export loop** (HEX/NMEA/Compare) |
| `matchingFilterIndex(parsed,cfg) :1786` | legacy: which filter a packet belongs to |
| `buildOutputHeaders :1732`, `buildLiveFieldHeaders :1740`, `buildPreviewHeaders :1745`, `buildPortMessagePreviewHeaders :1753` | CSV / preview header builders → `ExtractionEngine::columnHeaders` |
| `prepareOutputTable :1768`, `appendPreviewRow :1776` | preview table population |
| `buildPartitionCsvPath :1840`, `buildMessageCsvPath :1846` | output path builders → `safeName` |
| `setBusy :1858`, `setLiveUiState :1888`, `setStatus :1915` | UI state |

**Live capture** — see trace §9.3
| Function | Role |
|---|---|
| `startLiveCapture() ⊕ :1459` | ← `btnStartLive`; sets multicast, requires `m_liveMessages`, delegates to `startLiveCaptureWithMessages` |
| `startLiveCaptureWithMessages(port,err) :2265` | one `CsvStreamWriter` per message + `RefreshRateTracker`s; `m_liveReceiver->start` |
| `stopLiveCapture() ⊕ :1595` | ← `btnStopLive`; stop receiver, `closeLiveMessageWriters` |
| `onLiveDatagramReceived(payload,addr,port,ts) ⊕ :1635` | ← `LiveUdpReceiver::datagramReceived`; `tryRouteLivePacketByMessage` |
| `tryRouteLivePacketByMessage(payload,addr,port,ts) :2386` | match each live message; HEX/NMEA decode; `CompareOptionsEngine::compareRow`; `CsvStreamWriter::writeRow`; queue preview |
| `onLiveSocketError(msg) ⊕ :1682` | ← `LiveUdpReceiver::socketError` |
| `refreshLivePreview() ⊕ :1693` | ← preview `QTimer::timeout`; incremental append using seq counters |
| `closeLiveMessageWriters :2498`, `refreshLiveConfiguredMessagesTable :2523` | teardown / table repaint |
| `liveHeaderMatches :1810`, `extractLiveRowValues :1824` | dead legacy code, retained per additive rule |

**Project save / restore** — trace §9.5
| Function | Role |
|---|---|
| `captureProjectState(state) :1920` | session → `ProjectState` |
| `applyProjectState(state) :1940` | `ProjectState` → session (+ rebuild UI) |
| `tryRestoreProjectForPcap(pcap) :2000` | on browse, prompt-restore sidecar |
| `autoSaveProjectOnClose() :2038` | silent save on close |
| `onOpenProject ⊕ :2057`, `onSaveProject ⊕ :2078`, `onSaveProjectAs ⊕ :2104` | ← File-menu actions (Ctrl+O/S/Shift+S) |
| `loadProjectFromPath(path) :2635` | ← drop / open; `ProjectFile::load` → `applyProjectState` |

**Theme + ICD trigger**
| Function | Role |
|---|---|
| `onToggleThemeClicked() ⊕ :2135` | ← `btnToggleTheme`; flip `Themes::setMode`, `applyToAllTopLevels`, relabel button |
| `onImportIcdClicked() ⊕ :2659` | ← `actImportIcd` (Ctrl+I) **and** `btnImportIcd`; pick `.docx` → `IcdDocxImporter::extract` → `IcdImportDialog` → `applyImportedMessages` |
| `applyImportedMessages(msgs) :2691` | route imported messages into Live / header / selected port row (see CLAUDE.md §10.13 Routing) |

---

## 3. Read / parse / receive layer

### 3.1 `PcapFileReader` ⟨headers/PcapFileReader.h, sources/PcapFileReader.cpp⟩ — pcap + pcapng
| Function | Line | Role | ← called by |
|---|---|---|---|
| ctor/dtor | 17/26 | — | export loops |
| `open(path,err)` | 31 | sniff magic, dispatch `parsePcapGlobalHeader` / `parsePcapNgSectionHeader` | `onStartClicked`, `exportByMessageDefinitions`, `validateMessagesExistInCapture` |
| `readNextPacket(pkt,err)` | 89 | dispatch `readNextPcapPacket` / `readNextPcapNgPacket` | export loops |
| `close` 113 / `isOpen` 128 / `formatName` 133 | — | — | loops |
| `parsePcapGlobalHeader` 148, `parsePcapNgSectionHeader` 191 | parse file headers (endianness) | ← `open` |
| `readNextPcapPacket` 236, `readNextPcapNgPacket` 277 | per-record readers | ← `readNextPacket` |
| `parsePcapNgInterfaceBlock` 378, `parsePcapNgEnhancedPacketBlock` 431, `parsePcapNgSimplePacketBlock` 483 | pcapng block parsers | ← `readNextPcapNgPacket` |
| `readU16 521, readU32 534, combineU32ToU64 549, padded32Length 554` ▣util | endian reads | ← all parsers |

### 3.2 `UdpPacketParser` ⟨UdpPacketParser.{h,cpp}⟩
- `parsePacket(RawPacket) → ParsedUdpPacket  ⟨:44⟩` — strip Ethernet/IP/UDP, fill IPs/ports/payload/timestamp/`valid`. **← every export loop & live route.** Helpers ▣ `r16` :9, `ipText` :16, `timeText` :25.

### 3.3 `LiveUdpReceiver` ⟨LiveUdpReceiver.{h,cpp}⟩ (QObject)
| Function | Line | Role |
|---|---|---|
| ctor/dtor 6/11 | — |
| `setMulticastGroup(group) :16` | stores group; ← `startLiveCapture` |
| `start(port,err) :21` | bind (`ShareAddress|ReuseAddressHint` + `joinMulticastGroup` when group set), connect `readyRead` | ← `startLiveCaptureWithMessages` |
| `stop() :82`, `isRunning() :96` | — |
| `onReadyRead() ⊕ :101` | drain datagrams → **Σ `datagramReceived(QByteArray,QHostAddress,quint16,QDateTime)`** | → `MainWindow::onLiveDatagramReceived` |
| Σ `socketError(QString)` | → `MainWindow::onLiveSocketError` |

---

## 4. Decode layer

### 4.1 `ExtractionEngine` ⟨ExtractionEngine.{h,cpp}⟩ — HEX decode core (static class)
**Public**
- `valueFromPayload(payload,field) → QString  ⟨:192⟩` — single field: String special-case → `extractStringValue`; else bounds+length≤8 check → `readUnsignedBigEndianRawValue` → switch on `dataType` → `formatRawValue`-equivalent. **← (kept for reuse; main path is below).**
- `valuesFromPayload(payload,fields) → QStringList  ⟨:263⟩` — **the row builder.** Phase 1 reads each field's raw `quint64` once into `QVarLengthArray` + `nameToIndex`; Phase 2 emits main value, then `BitfieldDecoder::decodeRule` per bit rule, then `ConditionalBitfieldDecoder::decode`. **← `onStartClicked`, `exportByMessageDefinitions`, `tryRouteLivePacketByMessage`.**
- `columnHeaders(fields) → QStringList  ⟨:352⟩` — header names + `_<bitlabel>` + `ConditionalBitfieldDecoder::columnHeaders`. **← all `build*Headers`.**

**File-local ▣** `readUnsignedBigEndianRawValue :15`, `formatCalculatedValue :28`, `shouldApplyResolution :38`, `formatUnsignedValue :43`, `signExtendRawValue :51`, `formatSignedValue :71`, `extractStringValue :84`, `fieldBytesFromPayload :95`, `formatRawValue :105`, `computeExpectedColumnCount :165` (debug column-count assert).

### 4.2 `BitfieldDecoder` ⟨BitfieldDecoder.{h,cpp}⟩ — static
| Function | Line | Role | ← |
|---|---|---|---|
| `rulesToJson(rules) :28` | serialize bit rules (canonical) | ProjectFile, FieldCsvCodec callers, BitRuleCsvCodec, IcdEnumDecoder consumers, decoder dialogs |
| `rulesFromJson(json,…) :69` | parse | dialogs, ProjectFile, import |
| `parseBitPositions(text,…) :139` | "0;1;2"/"0-2" → list | rule dialog, BitRuleCsvCodec |
| `validateRules(rules,err) :245` | structural validation | every importer |
| `decodeRule(fieldBytes,rule) → QString :346` | **the per-row bit decode** | `ExtractionEngine::valuesFromPayload` |
| `binaryString :377, binaryToValue :388, sanitizeColumnLabel :405, bitsText :417, mappingSummary :425, ruleTypeText :441` | formatting/util | dialogs, `columnHeaders` |

### 4.3 `ConditionalBitfieldDecoder` ⟨…{h,cpp}⟩ — static
- `toJson :9` / `fromJson :46` — round-trip `ConditionalBitfieldDecoderConfig`. ← ProjectFile, dialogs.
- `validate :117` — controller + profile checks. ← dialogs.
- `columnHeaders(depName,cfg) :241` — `<dep>_Profile` + per-profile rule/exclusion columns. ← `ExtractionEngine::columnHeaders`.
- `decode(depBytes,ctrlVal,ctrlFound,cfg) :268` — pick profile by controller value, decode. ← `ExtractionEngine::valuesFromPayload`.

### 4.4 `MathExpressionEvaluator` ⟨…{h,cpp}⟩ — recursive-descent `resolutionExpression`
- `MathExpressionEvaluator(expr) :13`, `evaluate(expr,vars,out,err) :20` (public) ← `InputValidator::solveResolutionExpression`.
- Grammar: `parseExpression :97 → parseTerm :120 → parseUnary :151 → parsePower :168 → parsePrimary :187`; lexer `skipSpaces :66`, `peek :74`, `match :84`, `parseIdentifier :227`, `parseNumber :259`; `setError :57`; ▣ `isValidNumber :7`.

### 4.5 NMEA decode
- `NmeaDecoder` ⟨NmeaDecoder.{h,cpp}⟩
  - `NmeaDecodedRecord::valueAt(i) :4`, `rawValueAt(i) :14` — token access by 1-based comma index.
  - `formatValue(kind,raw) → QString :69` — lat/lon/time/date/number formatting. ← `buildNmeaRow`.
  - `decodePacket(expectedFormatter,payload) → Result :125` — split sentences, `xorChecksum` ▣:59 validate, comma-parse → records. **← `exportByMessageDefinitions`, `tryRouteLivePacketByMessage`.**
- `NmeaSentenceRegistry` ⟨…{h,cpp}, generated⟩ — `lookup :1553`, `supportedFormatters :1562`, `displayName :1571` over the 87-formatter table. ← pickers, `buildNmeaRow`, field dialogs.

### 4.6 `CompareOptionsEngine` ⟨CompareOptionsEngine.{h,cpp}⟩ — verification
- `RefreshRateTracker::RefreshRateTracker :8 / observe(tsMs) :12 / reset :21` — rolling 1-s Hz (QQueue). ← export & live loops (one per partition).
- `compareColumnNames(msg) → QStringList :221` — names of the verification columns (lockstep with `compareRow`). ← header builders.
- `compareRow(payload,msg,tracker,tsMs) → QStringList :272` — observed/computed/OK cells: header, terminator, checksum (XOR/SUM), refresh-rate, endianness, **data-length (P3b)**, **message-id (P3b)**, reason. ← `exportByMessageDefinitions`, `tryRouteLivePacketByMessage`. (File-local ▣ helpers `readStoredChecksum`, `hexFixedWidth` near top.)

---

## 5. Write + validate layer

### 5.1 `CsvExporter` ⟨…{h,cpp}⟩ (offline, buffered)
- `open(path,headers,err) :78`, `writeRow(row,err) :98`, `close :134`, `isOpen :144`; ▣/static `safeCell :149`, `escapeCell :163` (RFC-4180; reusable `QByteArray` buffer in `appendEscapedCellUtf8`). ← `onStartClicked`, `exportByMessageDefinitions`.

### 5.2 `CsvStreamWriter` ⟨…{h,cpp}⟩ (live, one per message)
- `open :16`, `writeRow(tsUtc,row,err) :69`, `flush :121`, `close :139`, `isOpen :149`, `filePath :154`, `rowsWritten :159`; private `writeLine :166`, `escapeCsv :182`, `protectFormula :198`. ← `startLiveCaptureWithMessages`, `tryRouteLivePacketByMessage`, `closeLiveMessageWriters`.

### 5.3 `InputValidator` ⟨InputValidator.cpp + InputValidator_filters.cpp⟩ — static
| Function | File:line | ← |
|---|---|---|
| `validateFilePath :47` | InputValidator.cpp | `onStartClicked` |
| `validatePortText :95`, `validatePortValue :115` | " | filters, ICD commit |
| `solveResolutionExpression :126` | " (→ `MathExpressionEvaluator`) | field collect/validate |
| `validateField :143`, `validateFields :196` | " | field dialogs, message validation, ICD commit |
| `minMessageFilterCount :24`, `maxMessageFilterCount :29`, `validateMessageFilterCount :34` | _filters.cpp | ctor spin range |
| `hexStringToBytes :47`, `validateHeaderHexText :77` | _filters.cpp | header/optional-header inputs, ICD |
| `validatePortFilters :127`, `validateHeaderFilters :167`, `validateFilterConfiguration :246` | _filters.cpp | `collectFilterConfiguration` |
| ▣ `fieldDataTypeValidationName` (InputValidator.cpp:12), ▣ `headerStartsWith` (_filters.cpp:13) | helpers | — |

---

## 6. Persistence + codecs

### 6.1 `ProjectFile` ⟨ProjectFile.{h,cpp}⟩ — `.pcproj.json` sidecar (static)
- `save(state,path,err) :291` — atomic `.tmp`→rename + `.bak`. ← `onSaveProject(As)`, `autoSaveProjectOnClose`.
- `load(path,state,err) :386` ← `loadProjectFromPath`, `tryRestoreProjectForPcap`.
- `sidecarPathFor(pcap) :459` (AppData fallback by MD5), `exists(path) :484`.
- `fieldListToJson :518` / `fieldListFromJson :557` — per-field-list JSON with nested decoder **objects**. ← `FieldConfigurationDialog` JSON dropdown, drag-drop.
- ▣ `dataTypeToJsonString :21` (+ inverse), `jsonValueToString :508`; internal `fieldToJson/fieldFromJson` mirror the data model (extend here when adding a `FieldDefinition` field — CLAUDE.md §11). Decoder configs round-trip via `BitfieldDecoder::rulesToJson` / `ConditionalBitfieldDecoder::toJson`. Compare-options via `compareOptionsToJson/FromJson`.

### 6.2 `FieldCsvCodec` ⟨FieldCsvCodec.{h,cpp}⟩ — field-table CSV + type-label authority (static)
- `supportedDataTypeLabels :169`, `dataTypeToLabel :178`, `dataTypeFromLabel :199` — the canonical label↔enum map. **← FieldConfigurationDialog, ICD type combos, IcdReviewDraftBuilder.**
- `dataTypeFromLabelAndSize(label,size,out) :213` — **size-aware (ICD P1):** keyword+Size disambiguation (`"Unsigned Integer"` etc.). ← `IcdReviewDraftBuilder::normalisedTypeText`.
- `importFromCsv :281`, `exportToCsv :456`, `writeTemplate :489` — ← FieldConfigurationDialog CSV dropdown, drag-drop.

### 6.3 `BitRuleCsvCodec` ⟨BitRuleCsvCodec.{h,cpp}⟩ — bit-rule bulk CSV (static)
- `importFromCsv :144` (rows merged by `Label`, validated via `BitfieldDecoder::validateRules`), `exportToCsv :411`, `writeTemplate :469`. ← `BitfieldDecoderDialog` import/export/template.
- ▣ `escapeCsvCell :60, readAllLogicalLines :73, normalizeUnknownBehavior :100, isUnknownBehaviorValid :107, parseBoolCell :112, normalizeBitsCell :122, bitsToCsvCell :129, cellAt :137`.

---

## 7. ICD `.docx` import subsystem

### 7.1 `IcdDocxImporter` ⟨IcdDocxImporter.{h,cpp}⟩ — extract + heuristics + profiles (static)
| Function | Line | Role | ← |
|---|---|---|---|
| `extract(docxPath,doc,err) :201` | unzip with **`QZipReader`**, walk `word/document.xml` via `QXmlStreamReader` → `IcdDocument` grids + headings | `MainWindow::onImportIcdClicked` |
| `buildDrafts :235` | original one-table-per-message builder | **retained, not called** |
| `buildGroupedDrafts :594` | parent+children → typed `IcdMessageDraft` w/ per-group offset auto-detect | (superseded by `IcdReviewDraftBuilder`; retained) |
| `suggestContinuationGroups :705` | structural pre-merge of adjacent same-width tables | `IcdImportDialog::onTableSelectionChanged` |
| `suggestRepeatCount(table) :791` | "Target N" → repeat count | `IcdTableSettingsDialog` prefill |
| `suggestMapping(table,profile) :813` | **content-aware column auto-detect** (header row, Name/Offset/Length/Type/Description, offset base) | table tick + Auto-detect button |
| `profilesDirectory :1020, availableProfiles :1028, saveProfile :1046, loadProfile :1071, profileToJson :1086, profileFromJson :1113` | named mapping-profile persistence | settings dialog Save/Load |

### 7.2 `IcdReviewDraftBuilder` ⟨IcdReviewDraftBuilder.{h,cpp}⟩ — the tolerant builder actually used
- `buildGroupedDrafts(doc,groups,drafts,warnings) :320` — **the path `onBuildClicked` calls.** Per group: apply parent mapping to each member table, append rows; missing cells → empty strings + warnings (never dropped); per-group offset auto-detect; offsets-from-size (P1); repeat-block replication (P1); enum→`bitRulesJson` via `IcdEnumDecoder` (P2). Produces `IcdMessageDraft.fieldRows` (string cells). File-local ▣: `appendRowsFromTable`, `normalisedTypeText` (→ `FieldCsvCodec::dataTypeFromLabelAndSize`), `looksLikeHeaderRow`, `minCorrectedOffset`, `applyRepeatPattern`.

### 7.3 `IcdEnumDecoder` ⟨IcdEnumDecoder.{h,cpp}⟩ (P2)
- `rulesFromDescription(description,label) → QList<BitDecodeRule> :6` — parse `value [-=:)/dash] meaning` anchors (≥2 values; `0xNN`>decimal; ≤255; meaning starts with a letter) → one `BitDecodeRule`. ← `IcdReviewDraftBuilder`, `IcdImportDialog::onEditFieldDecoderClicked` seed.

### 7.4 `IcdImportDialog` ⟨IcdImportDialog.{h,cpp} + IcdImportDialogTableButtons.cpp, forms/IcdImportDialog.ui⟩
3-box flow (CLAUDE.md §10.15). Widgets via `ui->*`.
| Function | Line | Role | ← |
|---|---|---|---|
| ctor/dtor 187/230 | bind ui, wire connects | `onImportIcdClicked` |
| `setDocument(doc) :235` | seed table list | ↑ |
| `tableLabel :252`, `populateTableList :264` | box-1 list | setDocument |
| `childrenOf :286`, `candidateChildrenFor :298` | merge bookkeeping | settings dialog |
| `onTableSelectionChanged() ⊕ :315` | ← `lstTables itemChanged`; auto-map ticked table (`suggestMapping`), seed `suggestContinuationGroups` once | |
| `refreshSelectedTablesTable :369` | box-2 (Standalone/Parent/Merged + Settings btn) | |
| `onTableSettingsClicked() ⊕ :406` → `openSettingsForTable(t) :417` | open `IcdTableSettingsDialog`, apply `mapping()`+`mergedChildren()` | per-row Settings btn |
| `buildGroups() :449` | parent→`IcdTableGroup` list | `onBuildClicked` |
| `onBuildClicked() ⊕ :468` | ← `btnBuild`; `buildGroups` → `IcdReviewDraftBuilder::buildGroupedDrafts` → `populateReviewTree` | |
| `populateReviewTree() :489` | box-3 tree; editable rows; per-row **DataType combo** (`configureTypeCombo` ▣:71); Decoder column → `onEditFieldDecoderClicked`; Preview btn → `onPreviewClicked` | onBuildClicked |
| `onPreviewClicked ⊕ :570 → previewGroup(p) :581` | merged-rows preview (`IcdTablePreviewDialog.ui`) | |
| `onEditFieldDecoderClicked() ⊕ :631` | open `BitfieldDecoderDialog` seeded from row JSON; store back | Decoder btn |
| `onCheckAll ⊕ :676 / onUncheckAll ⊕ :687`; `on_btnCheckAllTables_clicked :25 / on_btnUncheckAllTables_clicked :31` (auto-connect, TableButtons.cpp; ▣ `setAllTableChecks :9`) | bulk tick | btnAll/None, btnCheckAllTables/btnUncheckAllTables |
| `onAccept() ⊕ :698` | ← `buttonBox accepted`; tolerant validate ticked rows (`collectFieldFromItem`, skip-with-note via `setDetailedText`), `InputValidator::validateFields` gate → `m_result` | |
| `selectedMessages() → QList<MessageDefinition> :848` | → `MainWindow::applyImportedMessages` | onImportIcdClicked |
| ▣ `elide :35, decoderButtonLabel :58, resolutionText :63, configureTypeCombo :71` | — | populateReviewTree |

### 7.5 `IcdTableSettingsDialog` ⟨…{h,cpp}, forms/IcdTableSettingsDialog.ui⟩
- `setContext(doc,t,mapping,children) :97` ← `openSettingsForTable`.
- `headerCells :150`, `fillCombosForTable() ⊕ :165` (← `spnHeaderRow valueChanged`), `applyMappingToUi :188`, `collectMappingFromUi :221`, `mapping() :245`, `mergedChildren() :250`.
- Slots: `onAutoDetectClicked ⊕ :262` (→ `IcdDocxImporter::suggestMapping`), `onNameSourceChanged ⊕ :271`, `onUnmergeAllClicked ⊕ :277`, `onSaveProfileClicked ⊕ :287`, `onLoadProfileClicked ⊕ :305`.
- ▣ `elide :16, matchColumn :24, fillRoleCombo :40, setComboData :66`.

---

## 8. Signal → slot wiring map (the slot call graph)

> Every slot's *upstream trigger*. (`a → b` = signal `a` invokes slot `b`.) Built in each class's ctor.

**MainWindow** ⟨MainWindow.cpp:303–338 + rebuildFilterInputs⟩
```
btnBrowse.clicked            → onBrowseClicked
btnStart.clicked             → onStartClicked
spinFilterCount.valueChanged → onFilterCountChanged
radPortFilter/radHeaderFilter.toggled → onFilterModeChanged
btnConfigureHeaderFields.clicked → onConfigureHeaderFieldsClicked
actOpenProject/actSaveProject/actSaveProjectAs.triggered → onOpenProject/onSaveProject/onSaveProjectAs
actImportIcd.triggered  &  btnImportIcd.clicked → onImportIcdClicked
radFileMode/radLiveMode.toggled → onInputModeChanged
btnStartLive.clicked → startLiveCapture ;  btnStopLive.clicked → stopLiveCapture
m_livePreviewTimer.timeout → refreshLivePreview
m_liveReceiver.socketError → onLiveSocketError
m_liveReceiver.datagramReceived → onLiveDatagramReceived
btnToggleTheme.clicked → onToggleThemeClicked
btnManageLiveLengthFilters.clicked → onManageLiveLengthFiltersClicked
# per-row (rebuildFilterInputs): port spin.valueChanged→onPortValueChanged;
#   manage btn→onManageLengthFiltersClicked; header len btn→onManageHeaderLengthFiltersClicked
# per-table-row: Configure Fields btn→onConfigureMessageFieldsClicked / onConfigureLiveMessageFieldsClicked
```
**Dialogs** (all `buttonBox.accepted→onSaveClicked/onAccept`, `rejected→reject`):
```
FieldConfigurationDialog : btnAddField/EditField/RemoveField, btnBitfieldDecoder,
  btnConditionalDecoder, CSV menu(import/export/template), JSON menu(import/export),
  type combo currentIndexChanged(lambda)→applyLengthStateForType, per-row edit btns→onBitfield/ConditionalEditRowClicked
MessageLengthFilterDialog: btnAddLengthFilter/EditFilter/RemoveFilter, btnConfigureFields,
  per-row field btn→onConfigureFieldButtonClicked, per-row compare btn→onCompareOptionsButtonClicked
MessageDefinitionDialog : cmbDataFormat.currentIndexChanged→onDataFormatChanged
BitfieldDecoderDialog   : btnAddRule/EditRule/RemoveRule, btnImportCsv/ImportJson/Export/Template
BitfieldRuleDialog      : btnGenerateMappings/AddMapping/RemoveMapping
ConditionalBitfieldDecoderDialog: btnAddProfile/EditProfile/RemoveProfile
ConditionalProfileDialog: configureRulesBtn, btnAddExclusion/RemoveExclusion
NmeaFieldConfigurationDialog: btnAddRow/RemoveRow
NmeaSentencePickerDialog: buttonBox.accepted→onAccept
IcdImportDialog         : lstTables.itemChanged→onTableSelectionChanged, btnBuild→onBuildClicked,
  btnAll/None→onCheckAll/onUncheckAll, per-table Settings→onTableSettingsClicked,
  per-field Decoder→onEditFieldDecoderClicked, per-msg Preview→onPreviewClicked,
  btnCheckAllTables/btnUncheckAllTables (auto-connect)
IcdTableSettingsDialog  : btnAutoDetect, cmbNameSource.currentIndexChanged, spnHeaderRow.valueChanged→fillCombosForTable,
  btnUnmergeAll, btnSaveMapping/LoadMapping
LiveUdpReceiver         : m_socket.readyRead→onReadyRead, errorOccurred→(emit socketError)
```

---

## 9. End-to-end call traces

### 9.1 File-mode **legacy** export (header/standalone fields)
```
btnStart.clicked → onStartClicked  (MainWindow.cpp:687)
  ├ InputValidator::validateFilePath
  ├ collectFilterConfiguration → InputValidator::validateFilterConfiguration
  ├ [PORT mode w/ messages] → §9.2 ;  [HEADER mode w/ messages] → §9.2
  └ [legacy] InputValidator::validateFields(m_headerFields)
       ├ buildOutputHeaders → ExtractionEngine::columnHeaders
       ├ prepareOutputTable(buildPreviewHeaders)
       ├ per filter: new CsvExporter → buildPartitionCsvPath → exporter.open
       ├ PcapFileReader.open / formatName
       └ loop: readNextPacket → UdpPacketParser::parsePacket → matchingFilterIndex
              → ExtractionEngine::valuesFromPayload → CsvExporter::writeRow → appendPreviewRow
         then closePartitions, reader.close, summary QMessageBox
```

### 9.2 File-mode **per-message** export (primary)  `exportByMessageDefinitions` (:1213)
```
collect messages (collectMessageDefinitions / collectHeaderModeMessageDefinitions)
  → validateMessageDefinitions  (+ optional validateMessagesExistInCapture)
  → exportByMessageDefinitions:
       ├ getExistingDirectory (output folder)
       ├ per message: new CsvExporter, buildMessageCsvPath,
       │     headers = buildLiveFieldHeaders(fields) + CompareOptionsEngine::compareColumnNames(msg)
       │     + new RefreshRateTracker
       ├ PcapFileReader.open
       └ loop: readNextPacket → UdpPacketParser::parsePacket
              for each message partition:
                packetMatchesMessage(parsed,def)  ──┐ (port + length/header  OR  NMEA formatter scan)
                ├ NMEA: NmeaDecoder::decodePacket → per record buildNmeaRow → writeRow (+preview)
                └ HEX : ExtractionEngine::valuesFromPayload
                        (+ if hasCompareOptions: CompareOptionsEngine::compareRow with tsMs)
                        → CsvExporter::writeRow → appendPreviewRow
         then closeMessagePartitions, reader.close, summary
```

### 9.3 Live capture
```
btnStartLive.clicked → startLiveCapture (:1459)
  ├ require m_liveMessages non-empty
  ├ m_liveReceiver->setMulticastGroup(txtLiveMulticast)
  └ startLiveCaptureWithMessages(bindPort) (:2265)
        ├ per message: CsvStreamWriter::open + RefreshRateTracker
        ├ m_liveReceiver->start(port)  → (binds, joinMulticastGroup if set)
        └ m_livePreviewTimer->start
LiveUdpReceiver::onReadyRead → Σ datagramReceived
  → onLiveDatagramReceived (:1635) → tryRouteLivePacketByMessage (:2386)
       for each live message: packetMatchesMessage
         ├ NMEA: NmeaDecoder::decodePacket → buildNmeaRow
         └ HEX : ExtractionEngine::valuesFromPayload (+ CompareOptionsEngine::compareRow)
         → CsvStreamWriter::writeRow ; queue preview row (s_livePreviewAppendSeq++)
QTimer::timeout → refreshLivePreview (:1693)  # appends only new rows by seq delta
btnStopLive.clicked → stopLiveCapture (:1595) → receiver.stop + closeLiveMessageWriters
```

### 9.4 ICD `.docx` import
```
actImportIcd / btnImportIcd → onImportIcdClicked (:2659)
  ├ getOpenFileName(*.docx)
  ├ IcdDocxImporter::extract → IcdDocument (QZipReader + QXmlStreamReader)
  └ IcdImportDialog(doc).exec():
       setDocument → populateTableList
       lstTables.itemChanged → onTableSelectionChanged
            → IcdDocxImporter::suggestMapping (per ticked table)
            → IcdDocxImporter::suggestContinuationGroups (once) ; refreshSelectedTablesTable
       Settings → onTableSettingsClicked → IcdTableSettingsDialog
            (fillCombosForTable, onAutoDetectClicked→suggestMapping, save/load profile)
       btnBuild → onBuildClicked → buildGroups → IcdReviewDraftBuilder::buildGroupedDrafts
            (normalisedTypeText→FieldCsvCodec::dataTypeFromLabelAndSize; IcdEnumDecoder for enums;
             offsets-from-size; repeat replication) → populateReviewTree (DataType combos, Decoder btns)
       Preview → previewGroup ; Decoder → onEditFieldDecoderClicked → BitfieldDecoderDialog
       OK → onAccept → validate + collectFieldFromItem → InputValidator::validateFields → m_result
  → MainWindow::applyImportedMessages(dlg.selectedMessages())
       → routes to m_liveMessages / m_headerMessagesByRow[0] / m_portMessagesByRow[row] (+refresh)
```

### 9.5 Project save / restore
```
onSaveProject(As) → captureProjectState → ProjectFile::save (atomic .tmp→rename, .bak)
closeEvent → autoSaveProjectOnClose → captureProjectState → ProjectFile::save (silent)
onBrowseClicked → tryRestoreProjectForPcap → ProjectFile::sidecarPathFor/exists/load → applyProjectState
onOpenProject / drop(.pcproj.json) → loadProjectFromPath → ProjectFile::load → applyProjectState
applyProjectState → rebuildFilterInputs + refresh* (rebuilds entire UI from ProjectState)
```

### 9.6 Decoder editing (field → bit/conditional)
```
FieldConfigurationDialog: btnBitfieldDecoder → onBitfieldDecoderClicked → BitfieldDecoderDialog
   (rows: BitfieldRuleDialog ; bulk: BitRuleCsvCodec / BitfieldDecoder::rulesToJson|FromJson)
   → setDecoderCell (stores rulesToJson in name item UserRole)
btnConditionalDecoder → onConditionalDecoderClicked → ConditionalBitfieldDecoderDialog
   (profiles: ConditionalProfileDialog → reuses BitfieldDecoderDialog for that profile's rules)
   → setConditionalDecoderCell (UserRole+1)
collectFields() reads both UserRoles → BitfieldDecoder::rulesFromJson / ConditionalBitfieldDecoder::fromJson
```

---

## 10. Data model quick-reference (full detail in CLAUDE.md §5)

- `FieldDefinition` ⟨headers/AppTypes.h⟩ — name, `byteOffset`(1-based) / `byteOffsetcorrect`(0-based), length, `dataType`(`FieldDataType`), resolution(+expression), bit/conditional decoder flags+configs, `nmeaFieldIndex`, `nmeaValueKind`. Helpers: `fieldDataTypeNaturalLength()`, `fieldDataTypeHasFixedLength()`.
- `FieldDataType` enum — 13 values; `RawUnsignedBE`/`String` length user-provided.
- `BitDecodeRule`, `ConditionalBitfieldDecoderConfig`(+`ConditionalBitDecodeProfile`) ⟨AppTypes.h⟩.
- `CompareOptionsConfig` ⟨AppTypes.h⟩ — header/terminator/checksum/refresh/endianness + P3b data-length/message-id.
- `MessageDefinition` ⟨headers/MessageDefinition.h⟩ — name, port, payloadLengthBytes, fields, optionalHeader, compareOptions, `dataFormat`(HEX/NMEA), `nmeaSentenceType`.
- `FilterConfiguration` ⟨headers/FilterTypes.h⟩ — mode(PORT/HEADER), commonPort, `MessageFilter`s.
- `RawPacket` / `ParsedUdpPacket` ⟨AppTypes.h⟩.
- `ProjectState` ⟨headers/ProjectFile.h⟩ — the full serialised session.
- ICD: `IcdRawTable`/`IcdDocument`, `IcdMappingProfile`, `IcdTableGroup`, `IcdMessageDraft`/`IcdFieldDraftRow` ⟨headers/IcdImportTypes.h⟩.
- NMEA: `NmeaValueKind`, `NmeaSentenceDef`, `NmeaDecodedRecord` ⟨headers/NmeaTypes.h, NmeaDecoder.h⟩.

---

## 11. "If you change X, touch Y" (maintenance crib — see CLAUDE.md §11)

| Change | Files to touch |
|---|---|
| Add `FieldDefinition` property | AppTypes.h → FieldConfigurationDialog (table/collect/refresh) → ProjectFile fieldToJson/From + fieldListToJson/From → maybe FieldCsvCodec + ICD importer |
| Add a data type | AppTypes.h (`FieldDataType` + natural length) → FieldConfigurationDialog::setTypeCell → FieldCsvCodec labels → ProjectFile JSON strings → ExtractionEngine decode |
| Add a menu action | forms/MainWindow.ui (`<action>`+`<addaction>`) → MainWindow.h slot → ctor connect → slot body appended at EOF |
| Add an importer/exporter | mirror collect-errors-into-one-dialog + Replace/Append/Cancel + leave-state-on-failure; reuse `FieldCsvCodec::dataTypeFromLabel` + `InputValidator` |
| Add a Compare-Options check | AppTypes.h `CompareOptionsConfig` → CompareOptionsDialog UI/setConfig/config → CompareOptionsEngine `compareColumnNames`+`compareRow` (keep lockstep, before `CompareReason`) → ProjectFile compareOptionsToJson/From |

---

### Cross-references
- Architecture, constraints, feature catalogue, build steps → **[CLAUDE.md](../CLAUDE.md)**.
- Per-feature design notes → **[docs/](.)** (`ICD_DOCX_IMPORT.md`, `ICD_IMPORT_TOLERANT_REVIEW.md`, `EDITING_JSON.md`, …).
- User-facing walkthrough with screenshots → **[docs/USER_MANUAL.md](USER_MANUAL.md)**.
