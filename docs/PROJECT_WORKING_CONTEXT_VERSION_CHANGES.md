PcapUdpExtractor — Working, Context, Version Info, and Change Documentation

Date: 20 May 2026
Project: PcapUdpExtractor
Current major workflow version: V6 — Conditional Bitfield Decoder + Dark Military UI
Technology: C++, Qt Widgets, qmake
Primary purpose: Read .pcap / .pcapng files, parse UDP payloads, apply user-defined filters, extract protocol fields, decode bitfields conditionally, preview data, and export CSV files.

⸻

1. Project Overview

PcapUdpExtractor is a Qt C++ desktop application built to process packet capture files and extract meaningful UDP payload data into CSV format.

The application is designed for logs where UDP packets contain structured protocol messages. The user can load a .pcap or .pcapng file, define filters, configure fields inside the UDP payload, apply optional resolution formulas, decode bit-level status fields, and preview data, and export the extracted result into CSV files.

The current committed project uses Qt Widgets with qmake. The .pro file confirms that the project uses core, gui, widgets, and network Qt modules. It also confirms that Qt 6 builds use C++17 while Qt 5 builds remain on C++11.  ￼

⸻

2. Current Version Information

Project Name:        PcapUdpExtractor
Current Version:     V6 / Conditional Bitfield Decoder + Dark Military UI
Date:                20 May 2026
Language:            C++
Framework:           Qt Widgets
Build System:        qmake
Supported Standard:  C++11 for Qt 5, C++17 for Qt 6
Supported Input:     .pcap, .pcapng
Supported Output:    CSV
Main Filter Modes:   Port Filter, Header Filter
Additional Mode:     Live UDP Capture

V6 adds the Conditional Bitfield Decoder feature, a dark military-themed UI across all dialogs, and a refactor of the live extraction path to use ExtractionEngine::valuesFromPayload() so all modes share a single code path that is always synchronized with the CSV column headers.

⸻

3. High-Level Purpose of the Software

The software solves this problem:

A packet capture file may contain thousands of UDP packets. Those packets may come from different ports, and a single port can carry multiple different message types. Each message type can have a different payload length and a different internal field layout.

The application allows the user to:

1. Select a PCAP/PCAPNG file.
2. Choose filter mode.
3. Add port filters or header filters.
4. For port filters, define message types by payload length.
5. Configure fields separately for every message type.
6. Optionally configure bitfield decoding rules.
7. Extract values from UDP payloads.
8. Export clean CSV files.

⸻

4. Original Workflow Before V5

Earlier, the application followed this simpler workflow:

Select file
→ Select filter type
→ Add filters
→ Configure one global field table
→ Export CSV

This worked only when all selected messages shared the same field layout.

The old model was:

Port/Header filters
  → one shared global field table

That design was not correct for real-world packet logs because one UDP port can carry multiple messages with different payload structures.

Example:

Port 5001
  → Msg_A, payload length 20 bytes
  → Msg_B, payload length 30 bytes
  → Msg_C, payload length 40 bytes

In this case, Msg_A, Msg_B, and Msg_C may all have different fields. A single global field table cannot correctly decode all three.

⸻

5. Main Problem Solved in V5

The user requirement was:

When Port Filter mode is selected, every port should allow multiple message definitions based on UDP payload length.

Each message should have:

Message Name
Payload Length
Port Number
Its own field configuration
Its own bitfield decoder rules if required
Its own CSV export

This means field definitions must move from the global level to the message level.

The new model is:

Port
  → Length Filter / Message Definition
      → Field Configuration
          → Bitfield Decoder

This is now implemented through the new MessageDefinition structure, which stores messageName, port, payloadLengthBytes, and its own list of FieldDefinition objects.  ￼

⸻

6. Current Correct Workflow

The current V5 workflow is:

1. Open application.
2. Select File Mode.
3. Browse and import .pcap or .pcapng file.
4. Select filter type:
   - Port
   - Header
5. Enter number of filters.
6. If Port mode is selected:
   - Add port numbers.
   - Click Manage Length Filters for each port.
   - Add message name and UDP payload length.
   - Configure fields for each message.
7. If Header mode is selected:
   - Add common UDP port.
   - Add header filters.
   - Configure header-mode fields.
8. Start export.
9. Application validates filters, fields, and message existence.
10. Application exports CSV files.

For Port Filter mode, the actual hierarchy is now:

File
  → Port Filter
      → Port Number
          → Message Name + Payload Length
              → Field List
                  → Optional Bitfield Decoder Rules

⸻

7. Current Architecture

The project still follows the original modular architecture:

MainWindow
  → InputValidator
  → PcapFileReader
  → UdpPacketParser
  → ExtractionEngine
  → CsvExporter

After V5, additional workflow classes were added:

MessageDefinition
MessageLengthFilterDialog
MessageDefinitionDialog
FieldConfigurationDialog

The committed .pro file confirms that the following new source files, header files, and UI forms are now registered in the build:

sources/MessageDefinition.cpp
sources/MessageLengthFilterDialog.cpp
sources/MessageDefinitionDialog.cpp
sources/FieldConfigurationDialog.cpp
headers/MessageDefinition.h
headers/MessageLengthFilterDialog.h
headers/MessageDefinitionDialog.h
headers/FieldConfigurationDialog.h
forms/MessageLengthFilterDialog.ui
forms/MessageDefinitionDialog.ui
forms/FieldConfigurationDialog.ui

￼

⸻

8. Important File Structure

PcapUdpExtractor.pro
forms/
  MainWindow.ui
  BitfieldDecoderDialog.ui
  BitfieldRuleDialog.ui
  MessageLengthFilterDialog.ui
  MessageDefinitionDialog.ui
  FieldConfigurationDialog.ui
  ConditionalBitfieldDecoderDialog.ui       ← V6
  ConditionalProfileDialog.ui               ← V6
headers/
  AppTypes.h
  MainWindow.h
  FieldDefinition.h
  FilterTypes.h
  InputValidator.h
  PcapFileReader.h
  UdpPacketParser.h
  ExtractionEngine.h
  CsvExporter.h
  MathExpressionEvaluator.h
  BitfieldDecoder.h
  BitfieldDecoderDialog.h
  BitfieldRuleDialog.h
  LiveUdpReceiver.h
  CsvStreamWriter.h
  MessageDefinition.h
  MessageLengthFilterDialog.h
  MessageDefinitionDialog.h
  FieldConfigurationDialog.h
  ConditionalBitfieldDecoder.h              ← V6
  ConditionalBitfieldDecoderDialog.h        ← V6
  ConditionalProfileDialog.h                ← V6
sources/
  main.cpp
  MainWindow.cpp
  FieldDefinition.cpp
  InputValidator.cpp
  InputValidator_filters.cpp
  PcapFileReader.cpp
  UdpPacketParser.cpp
  ExtractionEngine.cpp
  CsvExporter.cpp
  MathExpressionEvaluator.cpp
  BitfieldDecoder.cpp
  BitfieldDecoderDialog.cpp
  BitfieldRuleDialog.cpp
  LiveUdpReceiver.cpp
  CsvStreamWriter.cpp
  MessageDefinition.cpp
  MessageLengthFilterDialog.cpp
  MessageDefinitionDialog.cpp
  FieldConfigurationDialog.cpp
  ConditionalBitfieldDecoder.cpp            ← V6
  ConditionalBitfieldDecoderDialog.cpp      ← V6
  ConditionalProfileDialog.cpp              ← V6
docs/
  BITFIELD_DECODER_FEATURE.md
  PORT_LENGTH_MESSAGE_FILTER_WORKFLOW.md
  CONDITIONAL_BITFIELD_DECODER.md           ← V6 user guide
  CONDITIONAL_BITFIELD_DECODER_IMPLEMENTATION.md ← V6 implementation record

⸻

9. Build Configuration

The project uses qmake.

The .pro file uses:

QT += core gui widgets network

The C++ standard is conditional:

greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}

This means:

Qt 5 build → C++11
Qt 6 build → C++17

This was added because newer Qt 6 kits may require a newer C++ standard.  ￼

Recommended build steps:

1. Open project in Qt Creator.
2. Select correct Qt kit.
3. Clean Project.
4. Run qmake.
5. Rebuild Project.
6. Run.

Terminal build:

qmake PcapUdpExtractor.pro
mingw32-make clean
mingw32-make

On Linux/macOS:

qmake PcapUdpExtractor.pro
make clean
make

⸻

10. MainWindow Responsibilities

MainWindow controls the main workflow:

File selection
Input mode selection
Filter mode selection
Port filter table
Header filter controls
Configured messages table
Preview table
Start/export workflow
Live capture controls

The current MainWindow.h confirms functions for collecting message definitions, validating them, checking if configured messages exist in the capture file, exporting by message definition, opening length-filter dialogs, and opening field-configuration dialogs.  ￼

Important functions include:

collectMessageDefinitions()
validateMessageDefinitions()
validateMessagesExistInCapture()
exportByMessageDefinitions()
openLengthFilterDialogForPortRow()
openFieldConfigurationForMessage()
configureFieldList()

These functions are central to the new V5 workflow.

⸻

11. Main UI Layout

The updated MainWindow.ui contains these main sections:

Input Mode
Input
Live UDP Capture
Message Filters
Configured Messages
Output Preview
Status / Progress

The main UI now includes:

File Mode / Live Mode selection
Capture file browser
Start Export button
Live UDP capture section
Filter count selector
Port/Header filter selector
Port filter table
Header filter panel
Configured Messages table
Output preview table

The UI now uses a dark military theme (V6): deep navy background (#0a0e1a), tactical blue accents (#1e88e5 / #4fc3f7), Bahnschrift font (falls back to Arial Narrow) at 11pt bold weight, and thin 6px scrollbars. All 8 UI forms share the same palette.

⸻

12. Port Filter Mode — New V5 Workflow

Port Filter mode now works as a two-level filter:

Level 1: UDP Port
Level 2: UDP Payload Length

Example:

Port 5001
  → Msg_A, length 20 bytes
  → Msg_B, length 30 bytes
Port 6001
  → Msg_C, length 40 bytes

The main port table contains:

Port
Manage Length Filters
Message Count

The user enters a port number, then clicks:

Manage Length Filters

This opens a port-specific message/length filter dialog.

⸻

13. Message Definition Model

The new message model is:

struct MessageDefinition
{
    QString messageName;
    quint16 port;
    int payloadLengthBytes;
    QList<FieldDefinition> fields;
};

Purpose of each property:

messageName:
Human-readable protocol message name. Also used for CSV filename.
port:
UDP port number used for matching source or destination UDP port.
payloadLengthBytes:
UDP payload size in bytes. This is not Ethernet frame length, IP packet length, UDP total length, or captured packet length.
fields:
Independent field list for this exact message only.

This is the core fix of V5.  ￼

⸻

14. MessageLengthFilterDialog

MessageLengthFilterDialog manages all message definitions for one selected UDP port.

It shows:

Message Name
Payload Length (bytes)
Fields
Configure Fields

It supports:

Add Length Filter
Edit Selected Filter
Remove Selected Filter
Configure Fields
Save
Cancel

The implementation validates duplicate names, duplicate payload lengths, and whether fields still fit inside the payload when a message length is edited.  ￼

Example usage:

Port: 5001
Message Name | Payload Length | Fields
Msg_A        | 20             | 3 fields
Msg_B        | 30             | 5 fields, 1 decoder

⸻

15. MessageDefinitionDialog

MessageDefinitionDialog is used to add or edit a single message definition.

It contains:

Message Name
Payload Length (bytes)
Save / Cancel

Validation:

Message name cannot be empty.
Payload length must be greater than 0.

The payload length spinbox accepts positive values and stores the final value as payloadLengthBytes.  ￼

⸻

16. FieldConfigurationDialog

FieldConfigurationDialog replaces the old global field table.

It is now used to configure fields for:

A specific Port + Length message
Header mode fields
Live mode fields

For Port mode, it receives the exact payload length so field boundaries can be validated.

The dialog table now contains 6 columns (V6 extended from 5):

Field Name
Byte Offset
Length
Resolution
Bit Decoder
Cond. Decoder         ← V6 new column

It supports:

Add Field
Edit Field
Remove Field
Bitfield Decoder
Conditional Decoder   ← V6 new button
Save
Cancel

The implementation validates field name, byte offset, field length, resolution expression, payload boundary, bitfield decoder rules, and on Save runs a second-pass validation of all conditional decoders against the final field list.

Storage per row (on the column-0 name item):
Qt::UserRole     → static bitfield decoder JSON (unchanged)
Qt::UserRole + 1 → conditional decoder JSON (V6 new)

⸻

17. Field Definition Model

The field model is stored in AppTypes.h.

Current FieldDefinition contains (V6 extended):

struct FieldDefinition
{
    QString name;
    int byteOffset;
    int length;
    double resolution;
    QString resolutionExpression;
    bool hasBitfieldDecoder;
    QList<BitDecodeRule> bitDecodeRules;
    bool hasConditionalBitfieldDecoder;            ← V6 new (default false)
    ConditionalBitfieldDecoderConfig conditionalDecoder;  ← V6 new
};

V6 also added two new structs to AppTypes.h:

struct ConditionalBitDecodeProfile
{
    QString profileName;
    quint64 controllerValue;
    QList<BitDecodeRule> bitDecodeRules;
};

struct ConditionalBitfieldDecoderConfig
{
    QString controllerFieldName;
    QString unknownBehavior;    // "UNKNOWN_CONTROLLER" or "BLANK"
    QList<ConditionalBitDecodeProfile> profiles;
};

The added resolutionExpression stores the original expression text, such as:

180/2^15
1/256
360/2^16

The calculated resolution stores the solved numeric value.

⸻

18. Field Extraction Logic

For every matched packet, the application extracts values from UDP payload using the configured field list.

V6 extended field extraction works in two passes:

Pass 1 (raw value map for controller lookups):
- For each field whose byte range fits in the payload:
  - Read raw unsigned big-endian integer value.
  - Store in QMap<QString, quint64> rawValues keyed by field name.
  - Store validity flag in QMap<QString, bool> fieldValid.

Pass 2 (output row):
1. For each configured field:
   a. Call valueFromPayload() — raw or resolution-scaled value.
   b. If hasBitfieldDecoder:
      - Extract field bytes.
      - For each enabled rule: call BitfieldDecoder::decodeRule().
      - Append decoded values.
   c. If hasConditionalBitfieldDecoder:
      - Look up controller field value from rawValues map.
      - Call ConditionalBitfieldDecoder::decode().
      - Append profile name column + all profile rule columns (stable count).

Field extraction is handled by ExtractionEngine::valuesFromPayload().
Column headers are generated by ExtractionEngine::columnHeaders() (V6 new method) using the exact same iteration logic, guaranteeing header count == value count always.

Live mode now calls ExtractionEngine::valuesFromPayload() directly (V6 fix), eliminating a separate code path that previously could diverge from the CSV headers.

⸻

19. Port + Length Packet Matching

For Port Filter mode, a UDP packet matches a message definition only if both conditions are true:

(source UDP port == configured message port OR destination UDP port == configured message port)
AND
UDP payload size == configured payload length

The implementation uses parsed.udpPayload.size() for matching, meaning it uses the UDP payload length only. It does not use Ethernet frame length, IP packet length, UDP total length, or capture record length.  ￼

Example:

Configured message:
Name: Msg_A
Port: 5001
Payload Length: 20
Packet:
Source Port: 4500
Destination Port: 5001
UDP Payload Size: 20
Result:
Matched

Example not matched:

Configured message:
Name: Msg_A
Port: 5001
Payload Length: 20
Packet:
Source Port: 4500
Destination Port: 5001
UDP Payload Size: 40
Result:
Not matched

⸻

20. Port Mode Export Logic

In Port Filter mode, the application exports one CSV file per message definition.

Filename format:

messageName_payloadLength_port_yyyyMMdd_HHmmss.csv

Example:

Msg_A_20_5001_20260519_214522.csv
Msg_B_30_5001_20260519_214522.csv
Msg_C_40_6001_20260519_214522.csv

The committed function buildMessageCsvPath() generates this format by combining sanitized message name, payload length, port number, and timestamp.  ￼

Each message CSV uses only that message's own fields.

Important current behavior:

The current implementation writes field-value columns to each message CSV. Packet metadata is shown in preview, but the per-message CSV export currently uses field headers and extracted field values only. If packet metadata is required in the message CSV files, export should be adjusted to include:

Packet No
Timestamp
Source IP
Destination IP
Source UDP Port
Destination UDP Port
Payload Size

⸻

21. Header Filter Mode

Header Filter mode is preserved.

Header mode workflow:

1. Select Header filter.
2. Enter common UDP port.
3. Enter header filters.
4. Configure header fields.
5. Export.

Header filter matching still works by checking:

UDP source/destination port matches common port
AND
UDP payload starts with configured header bytes

Header mode has its own field list through m_headerFields.

⸻

22. Live UDP Capture Mode

The project also includes Live UDP Capture.

Live mode can:

Bind to a UDP port
Receive live UDP datagrams
Apply header matching if Header mode is selected
Extract configured live fields
Write rows into a live CSV file
Show live preview
Track packets received, matched, rows written, and short packets

Live mode uses m_liveFields, separate from port-message fields and header fields.

Important limitation:

Live mode currently receives datagrams from a bound UDP port. It does not have the full source/destination port context in the same way as PCAP parsing. Therefore, the Port + Length message-definition workflow is mainly for File Mode / PCAP export.

⸻

23. Bitfield Decoder Support

23a. Static Bitfield Decoder (unchanged since V4)

A field can have:

Raw field value
Decoded bit columns
Single-bit rules
Grouped-bit rules
Reserved/spare bit rules
Unknown value behavior

Example:

Field: Status
Length: 1 byte
Rules:
Bits 0-2 → BITE
Bits 3-4 → MODE
Bits 5-6 → AHR_VALIDITY
Bit 7    → SPARE

Output headers become:

Status
Status_BITE
Status_MODE
Status_AHR_VALIDITY
Status_SPARE

The Field Configuration dialog reuses the existing BitfieldDecoderDialog unchanged.

23b. Conditional Bitfield Decoder (V6 new)

A field can also carry a Conditional Bitfield Decoder. This allows one field's bits to be decoded differently depending on the raw value of another field (the "controller field") in the same payload.

Example — Sonar Sub State depends on Sonar Mode:

Sonar_Mode = 0x01 (Live)       → decode bits as TX_OFF / TX_ENABLE / TRANSMITTING
Sonar_Mode = 0x02 (Simulation) → decode bits as SIMULATION active
Sonar_Mode = 0x04 (Replay)     → decode bits as REPLAY active
Sonar_Mode = 0x08 (Maintenance)→ decode bits as MAINTENANCE_MODE active

Output headers for Sonar_Sub_State with 4 profiles:

Sonar_Sub_State
Sonar_Sub_State_Profile
Sonar_Sub_State_Live_SSTT_TX_OFF
Sonar_Sub_State_Live_SSTT_TX_ENABLE
Sonar_Sub_State_Live_TRANSMITTING
Sonar_Sub_State_Simulation_SIMULATION
Sonar_Sub_State_Simulation_RESERVED
Sonar_Sub_State_Replay_REPLAY
Sonar_Sub_State_Replay_RESERVED
Sonar_Sub_State_Maintenance_MAINTENANCE_MODE
Sonar_Sub_State_Maintenance_RESERVED

Headers are stable — always the same columns regardless of which profile matches in any given row.
Only the matching profile's rule columns are filled; all other profile columns are blank.
When no profile matches, the _Profile column reads UNKNOWN_CONTROLLER(0xNN) or "" (configurable).

The feature is fully generic. No sonar names, byte offsets, or mode values are hardcoded in the engine.

⸻

24. Resolution Expression Support

Fields support mathematical resolution expressions.

Examples:

1
180/2^15
360/2^16
1/256

Validation happens through InputValidator::solveResolutionExpression() and the math expression evaluator. The validator checks that the expression evaluates successfully and produces a value greater than zero.  ￼

Example:

Payload bytes: 16 05
Raw decimal: 5637
Resolution: 180/2^15 = 0.005493
Final value: 5637 × 0.005493 = 30.964

⸻

25. Validation Rules

Message Validation

Every message definition must satisfy:

Message name cannot be empty.
Port must be valid.
Payload length must be greater than 0.
Duplicate payload length under same port is not allowed.
Duplicate message name under same port is not allowed.
At least one field must be configured before export.

validateMessageDefinitions() enforces these checks.  ￼

Field Validation

Every field must satisfy:

Field name cannot be empty.
Field names must be unique inside the same message.
Byte offset must be >= 0.
Length must be > 0.
Length must be <= 8 bytes.
Resolution must be valid and greater than zero.
Byte offset + length must not exceed message payload length.
Bitfield rules must be valid if bitfield decoder is enabled.

Field validation exists in both InputValidator and FieldConfigurationDialog.

Conditional Decoder Validation (V6 new)

Each configured conditional decoder must satisfy:

Controller field name is non-empty.
Controller field is not the same as the dependent field.
Controller field exists in the message field list.
Controller field length is between 1 and 8 bytes.
At least one profile is configured.
No two profiles share the same controller value.
No two profiles share the same name (case-insensitive).
No two profiles produce the same sanitized CSV column prefix.
Each profile's bit rules pass BitfieldDecoder::validateRules() for the dependent field's length.

Validation runs in three places:
1. ConditionalBitfieldDecoderDialog::onSaveClicked() — on dialog close.
2. FieldConfigurationDialog::onSaveClicked() — second pass using the final complete field list.
3. MainWindow::validateMessageDefinitions() — at export time.

Capture Existence Validation

Before export, the application scans the capture file to confirm every configured message appears at least once.

If a configured message is not found, export is blocked with an error like:

No packet found for message 'Msg_A' with port 5001 and payload length 20 bytes.

The reader is closed after validation, and export opens the file again separately. This avoids the issue where validation consumes the reader before export.  ￼

⸻

26. CSV Output Behavior

Header Mode CSV

Header mode exports partitioned CSVs based on header filters.

Rows include packet metadata plus extracted fields:

Packet No
Timestamp
Source IP
Destination IP
Source UDP Port
Destination UDP Port
Payload Size
Field columns...

Port + Length Message CSV

Port mode exports one CSV per configured message definition.

Current file naming:

messageName_payloadLength_port_yyyyMMdd_HHmmss.csv

Current message CSV row content:

Field columns only

Current preview content:

Message
Packet No
Timestamp
Source IP
Destination IP
Source UDP Port
Destination UDP Port
Payload Size
Extracted Values

Recommended future improvement:

Add packet metadata columns into each message CSV for easier traceability.

⸻

27. Example Full Workflow

Example user setup:

Selected file:
sample_log.pcapng
Filter mode:
Port
Number of filters:
2
Ports:
5001
6001

Port 5001 message definitions:

Msg_A | Payload Length 20
Msg_B | Payload Length 30

Port 6001 message definitions:

Msg_C | Payload Length 40

Field configuration:

Msg_A:
  heading, byte 0, length 2, resolution 180/2^15
  speed, byte 2, length 2, resolution 1/100
Msg_B:
  mode, byte 0, length 1, resolution 1
  status, byte 1, length 1, resolution 1, bitfield decoder enabled
Msg_C:
  temperature, byte 4, length 2, resolution 0.1

Export result:

Msg_A_20_5001_20260519_214522.csv
Msg_B_30_5001_20260519_214522.csv
Msg_C_40_6001_20260519_214522.csv

Each CSV contains only packets that match its exact message definition.

⸻

28. Current Known Limitation

The current system identifies messages by:

UDP port + UDP payload length

Therefore, two different messages cannot share the same port and same payload length.

Example unsupported case:

Port 5001
  Msg_A length 20
  Msg_B length 20

The software blocks duplicate payload length under the same port because it cannot distinguish these messages safely.

Future solution:

Port + Length + Header
Port + Length + Message ID byte
Port + Length + Fixed byte pattern at offset

This limitation is also documented in PORT_LENGTH_MESSAGE_FILTER_WORKFLOW.md.  ￼

⸻

29. Version History

V1 — Basic PCAP UDP Extractor

Initial workflow:

Import PCAP/PCAPNG
Parse UDP packets
Configure fields
Export CSV

Main capability:

Read capture files and extract byte-level fields from UDP payload.

⸻

V2 — Resolution Expression Fix

Added support for mathematical resolution expressions such as:

180/2^15
1/256
360/2^16

Purpose:

Allow real protocol scaling factors instead of only numeric constants.

⸻

V3 — Multi-Filter Workflow

Added filter modes:

Port Filter
Header Filter

Port mode:

Match packets where source or destination UDP port equals configured port.

Header mode:

Match packets where payload starts with configured header bytes.

⸻

V4 — Bitfield Decoder + UI Form Upgrade

Added:

BitfieldDecoder
BitfieldDecoderDialog
BitfieldRuleDialog
BitDecodeRule model
Bitfield decoder CSV expansion
Qt Designer .ui conversion
Soft 12pt light UI theme

Purpose:

Decode protocol status bits and grouped bit ranges into readable labels.

⸻

V5 — Port + Length Message Workflow

Added:

MessageDefinition model
MessageLengthFilterDialog
MessageDefinitionDialog
FieldConfigurationDialog
Port-specific length filters
Per-message field configuration
Per-message CSV export
Message existence validation
Payload-length-based packet matching

Purpose:

Correctly support real logs where one UDP port carries multiple message types with different payload lengths and different field layouts.

⸻

V6 — Conditional Bitfield Decoder + Dark Military UI

Added (new files):

ConditionalBitfieldDecoder utility class (header + source)
ConditionalBitfieldDecoderDialog (header + source + .ui form)
ConditionalProfileDialog (header + source + .ui form)
docs/CONDITIONAL_BITFIELD_DECODER.md
docs/CONDITIONAL_BITFIELD_DECODER_IMPLEMENTATION.md

New data structures (AppTypes.h):

ConditionalBitDecodeProfile
ConditionalBitfieldDecoderConfig
FieldDefinition extended with hasConditionalBitfieldDecoder + conditionalDecoder

Modified:

FieldConfigurationDialog — 6th column "Cond. Decoder", new "Conditional Decoder" button, second-pass save validation, Qt::UserRole+1 JSON storage
ExtractionEngine — two-pass valuesFromPayload(), new columnHeaders() method
MainWindow — buildLiveFieldHeaders() delegates to ExtractionEngine::columnHeaders(), validateMessageDefinitions() validates conditional decoders, extractLiveRowValues() simplified to use ExtractionEngine::valuesFromPayload()
All 8 .ui forms — dark military theme (Bahnschrift/Arial Narrow, 11pt, navy/tactical-blue palette)

Purpose:

Allow one field's bits to be decoded differently depending on another field's value in the same payload. Generic implementation — not sonar-specific. Stable CSV column headers across all modes. Synchronized header/value counts guaranteed by single code path.

This is the current committed version.

⸻

30. Files Added in V5

headers/MessageDefinition.h
sources/MessageDefinition.cpp
headers/MessageLengthFilterDialog.h
sources/MessageLengthFilterDialog.cpp
forms/MessageLengthFilterDialog.ui
headers/MessageDefinitionDialog.h
sources/MessageDefinitionDialog.cpp
forms/MessageDefinitionDialog.ui
headers/FieldConfigurationDialog.h
sources/FieldConfigurationDialog.cpp
forms/FieldConfigurationDialog.ui
docs/PORT_LENGTH_MESSAGE_FILTER_WORKFLOW.md

⸻

30b. Files Added in V6

headers/ConditionalBitfieldDecoder.h
sources/ConditionalBitfieldDecoder.cpp
headers/ConditionalBitfieldDecoderDialog.h
sources/ConditionalBitfieldDecoderDialog.cpp
forms/ConditionalBitfieldDecoderDialog.ui
headers/ConditionalProfileDialog.h
sources/ConditionalProfileDialog.cpp
forms/ConditionalProfileDialog.ui
docs/CONDITIONAL_BITFIELD_DECODER.md
docs/CONDITIONAL_BITFIELD_DECODER_IMPLEMENTATION.md

⸻

30c. Files Modified in V6

headers/AppTypes.h — two new structs, FieldDefinition extended
headers/ExtractionEngine.h — columnHeaders() declaration
sources/ExtractionEngine.cpp — two-pass valuesFromPayload(), columnHeaders() implementation
headers/FieldConfigurationDialog.h — new slot + helper
sources/FieldConfigurationDialog.cpp — 6th column, conditional decoder slot, updated collectFields/onSaveClicked
forms/FieldConfigurationDialog.ui — btnConditionalDecoder added
sources/MainWindow.cpp — simplified extractLiveRowValues(), updated buildLiveFieldHeaders(), validateMessageDefinitions(), fieldStatusText()
All .ui forms — dark military stylesheet applied

⸻

31. Files Updated in V5

PcapUdpExtractor.pro
forms/MainWindow.ui
headers/AppTypes.h
headers/MainWindow.h
sources/MainWindow.cpp
sources/InputValidator.cpp
sources/InputValidator_filters.cpp
README.md

Main updates:

Registered new files in qmake.
Added message-definition workflow.
Moved field configuration out of main UI.
Added configured-message table.
Added per-message export logic.
Added per-message validation.
Preserved header and live field configuration.

⸻

32. Manual Testing Checklist

Build Test

1. Clean project.
2. Run qmake.
3. Rebuild.
4. Confirm no missing ui_*.h errors.
5. Launch application.

Port Workflow Test

1. Select File Mode.
2. Import .pcap or .pcapng.
3. Select Port filter.
4. Set filter count to 2.
5. Add port 5001.
6. Add port 6001.
7. Click Manage Length Filters for port 5001.
8. Add Msg_A length 20.
9. Add Msg_B length 30.
10. Click Manage Length Filters for port 6001.
11. Add Msg_C length 40.
12. Confirm main Configured Messages table shows all three messages.

Field Isolation Test

1. Configure fields for Msg_A.
2. Configure different fields for Msg_B.
3. Reopen Msg_A configuration.
4. Confirm Msg_B fields do not appear.
5. Reopen Msg_B configuration.
6. Confirm Msg_A fields do not appear.

Duplicate Length Test

1. Under port 5001, add Msg_A length 20.
2. Try to add Msg_B length 20.
3. Confirm software blocks it.

Payload Boundary Test

1. Create message length 20.
2. Add field byte offset 19, length 2.
3. Confirm software blocks it because 19 + 2 exceeds 20.

Empty Fields Test

1. Add message definition.
2. Do not configure fields.
3. Start export.
4. Confirm export is blocked.

Missing Message Test

1. Add message with port and length that do not exist in capture.
2. Start export.
3. Confirm export is blocked with message-not-found error.

Bitfield Decoder Test

1. Add a 1-byte status field.
2. Configure bitfield decoder rules.
3. Export.
4. Confirm raw field and decoded columns appear.

Conditional Bitfield Decoder Test (V6)

1. Create a message with Sonar_Mode (1 byte, offset 2) and Sonar_Sub_State (1 byte, offset 3).
2. Select Sonar_Sub_State → click Conditional Decoder.
3. Set Controller Field to Sonar_Mode.
4. Add 4 profiles: Live (0x01), Simulation (0x02), Replay (0x04), Maintenance (0x08).
5. Configure bit rules for each profile.
6. Save. Confirm 6th column shows "Yes (4 profiles)".
7. Export or run live capture.
8. Confirm CSV headers include _Profile column plus all profile rule columns.
9. Confirm only the matching profile's columns are filled per row.
10. Send a packet with Sonar_Mode=0x10 (no matching profile).
    Confirm _Profile column reads UNKNOWN_CONTROLLER(0x10).
11. Delete Sonar_Mode field, try to save → confirm error blocked.
12. Set Behavior to BLANK, resend unmatched packet → confirm _Profile is empty.

Dark Theme Regression Test (V6)

1. Launch application.
2. Confirm dark navy background on main window.
3. Open all dialogs (Field Configuration, Bitfield Decoder, Conditional Decoder, Profile).
4. Confirm consistent dark theme across all dialogs.
5. Confirm Bahnschrift/Arial Narrow font is rendering (not system default).

Header Mode Regression Test

1. Select Header filter.
2. Add common port.
3. Add header values.
4. Configure header fields.
5. Export.
6. Confirm header mode still works.

Live Mode Regression Test

1. Select Live Mode.
2. Configure live fields.
3. Bind UDP port.
4. Send UDP datagrams.
5. Confirm packets received and rows written.

⸻

33. Important Developer Notes

Do not manually create or edit generated Qt headers like:

ui_MainWindow.h
ui_FieldConfigurationDialog.h
ui_MessageLengthFilterDialog.h
ui_MessageDefinitionDialog.h

These are generated by Qt's uic tool from .ui files during qmake/build.

Correct workflow:

Edit .ui files in Qt Designer or XML.
Run qmake.
Build.
Qt generates ui_*.h files automatically.

⸻

34. Recommended Future Improvements

1. Add Metadata Columns to Per-Message CSV

Current message CSVs appear to export only field values. Add packet metadata for traceability:

Packet No
Timestamp
Source IP
Destination IP
Source UDP Port
Destination UDP Port
Payload Size

2. Add Port + Length + Header Message Identification

Current limitation:

Same port + same length cannot be separated.

Future model:

MessageDefinition
  messageName
  port
  payloadLengthBytes
  optionalHeaderBytes
  optionalHeaderOffset
  fields

3. Save and Load Configuration

Add support for saving project settings:

Selected filters
Message definitions
Fields
Resolution expressions
Bitfield decoder rules

Possible format:

JSON

4. Add Import/Export Config

Allow users to reuse protocol layouts across multiple PCAP files.

5. Add Better Preview Per Message

Current preview combines extracted values into one preview cell for port-message export. A future version can allow selecting a message and showing its full decoded table.

⸻

35. Final Current Status

Current status (V6):

The Port + Length + Message Definition workflow is complete (V5).
The Conditional Bitfield Decoder feature is implemented (V6).
The dark military UI theme is applied across all 8 .ui forms (V6).
The live extraction path uses ExtractionEngine::valuesFromPayload() (V6 fix).
CSV header and value column counts are guaranteed synchronized via ExtractionEngine::columnHeaders().
All new files are registered in PcapUdpExtractor.pro.
ConditionalBitfieldDecoder, ConditionalBitfieldDecoderDialog, and ConditionalProfileDialog are all proper Qt Designer .ui form-backed dialogs.

Current known limitations:

One message per port+length combination only (pre-existing constraint).
No mutual-exclusion bit validation within a profile (e.g. cannot enforce that Live mode bits 0 and 1 are exclusive).
Per-message CSV exports field columns only, not packet metadata (pre-existing).

Final technical assessment:

Architecture: Correct
V6 feature completeness: Full (except mutual-exclusion validation — documented limitation)
Manual test needed: Yes
Ready for local build test: Yes
Ready for demo: Yes (after build test)
