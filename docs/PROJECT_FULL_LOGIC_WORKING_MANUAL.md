# PCAP UDP Extractor - Full Project Logic and Working Manual

Generated for the `pcapfile` Qt/C++ project.

This document explains the complete working, architecture, data flow, module responsibilities, packet parsing logic, field extraction logic, CSV export logic, validation rules, current limitations, and debugging points of the project.

The project is a lightweight offline Qt Widgets application that reads `.pcap` and `.pcapng` packet capture files, extracts UDP packets for a selected port, reads custom user-defined fields from the UDP payload, displays a preview in the UI, and exports the result to a CSV file that can be opened in Microsoft Excel.

---

## 1. Project Identity

### Project name

```text
PcapUdpExtractor
```

### Repository name

```text
pcapfile
```

### Project type

```text
Qt Widgets desktop application
```

### Build system

```text
qmake
```

The project uses a `.pro` file, not CMake.

### Language standard

```text
C++11
```

### Main goal

The main goal is to make a simple offline tool where a user can:

1. Select a `.pcap` or `.pcapng` file.
2. Enter a UDP port number.
3. Define custom fields from the UDP payload.
4. Extract field values from matching UDP packets.
5. Preview extracted values in the Qt table.
6. Export the complete result into a `.csv` file.

---

## 2. Important Design Constraints

The project is intentionally simple and offline.

It does not use:

```text
libpcap
Npcap
WinPcap
QXlsx
Boost
Python
Database
Internet API
External packet parsing libraries
```

The code manually reads packet capture bytes using Qt classes like:

```cpp
QFile
QByteArray
QString
QStringList
QTableWidget
QTextStream
```

This design keeps the project easy to explain during internship review because the logic is visible in the source code instead of hidden inside third-party libraries.

---

## 3. Active Project File List

The active build configuration is controlled by `PcapUdpExtractor.pro`.

Current source files included in the build:

```text
sources/main.cpp
sources/MainWindow.cpp
sources/FieldDefinition.cpp
sources/InputValidator.cpp
sources/PcapFileReader.cpp
sources/UdpPacketParser.cpp
sources/ExtractionEngine.cpp
sources/CsvExporter.cpp
sources/MathExpressionEvaluator.cpp
```

Current header files included in the build:

```text
headers/MainWindow.h
headers/ui_MainWindow.h
headers/AppTypes.h
headers/FieldDefinition.h
headers/InputValidator.h
headers/PcapFileReader.h
headers/UdpPacketParser.h
headers/ExtractionEngine.h
headers/CsvExporter.h
headers/MathExpressionEvaluator.h
```

Important note: the current UI used by the application is the manually written `headers/ui_MainWindow.h`. The `.pro` file does not currently compile a `.ui` form file through the usual Qt Designer `FORMS +=` mechanism.

---

## 4. High-Level Architecture

The project is divided into small modules. Each module has one main responsibility.

```text
main.cpp
  -> creates QApplication
  -> creates MainWindow
  -> starts Qt event loop

MainWindow
  -> handles UI
  -> collects user input
  -> validates file, port, and field definitions
  -> starts extraction workflow
  -> updates preview table
  -> shows status and errors

InputValidator
  -> validates file path
  -> validates UDP port
  -> validates field rows
  -> solves resolution expressions through MathExpressionEvaluator

MathExpressionEvaluator
  -> evaluates expressions like 180/2^15
  -> supports +, -, *, /, ^, brackets, scientific notation, pi, e

PcapFileReader
  -> opens PCAP or PCAPNG file
  -> detects file format
  -> reads one packet at a time
  -> does not load full file into memory

UdpPacketParser
  -> parses Ethernet II frame
  -> handles basic VLAN tag
  -> parses IPv4 header
  -> rejects fragmented IPv4 packets
  -> parses UDP header and payload

ExtractionEngine
  -> reads user-defined byte ranges from UDP payload
  -> converts selected bytes into unsigned big-endian integer
  -> applies resolution multiplier
  -> returns display/export value

CsvExporter
  -> opens CSV file
  -> writes headers
  -> writes rows
  -> escapes CSV cells
  -> protects against Excel formula injection
```

---

## 5. Complete Data Flow

The complete runtime flow is:

```text
User opens application
  -> User selects .pcap/.pcapng file
  -> User enters UDP port
  -> User adds payload field definitions
  -> User clicks Start
  -> MainWindow validates all input
  -> MainWindow asks user where to save CSV
  -> CsvExporter opens output file and writes header row
  -> PcapFileReader opens capture file
  -> Capture format is detected as PCAP or PCAPNG
  -> Reader reads one raw packet
  -> UdpPacketParser parses packet
  -> Invalid/non-UDP/non-matching packets are skipped
  -> Matching UDP packet payload is passed to ExtractionEngine
  -> ExtractionEngine extracts all custom fields
  -> MainWindow builds one output row
  -> CsvExporter writes row immediately
  -> MainWindow adds row to preview table until preview limit
  -> Loop continues until end of file or error
  -> Files are closed
  -> Summary is shown to user
```

The packet flow inside the capture data is:

```text
Capture file
  -> raw packet record
  -> Ethernet II frame
  -> optional VLAN tag
  -> IPv4 packet
  -> UDP datagram
  -> UDP payload
  -> custom field bytes
  -> raw integer
  -> scaled engineering value
  -> preview table + CSV row
```

---

## 6. Entry Point: `main.cpp`

The application starts from `sources/main.cpp`.

Code:

```cpp
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
```

Explanation:

1. `QApplication app(argc, argv);` creates the Qt application object.
2. `MainWindow window;` creates the main UI window.
3. `window.show();` displays the window.
4. `app.exec();` starts the Qt event loop.

The event loop keeps the UI alive and responds to button clicks.

---

## 7. Shared Data Structures: `AppTypes.h`

The file `headers/AppTypes.h` contains common structures used across modules.

### 7.1 `FieldDefinition`

```cpp
struct FieldDefinition
{
    QString name;
    int byteOffset;
    int length;
    double resolution;
};
```

Meaning:

```text
name       = output column name
byteOffset = zero-based byte offset inside UDP payload
length     = number of bytes to read from UDP payload
resolution = multiplier applied to raw decimal value
```

Example:

```text
Field name  : Heading
Byte offset : 15
Length      : 2
Resolution  : 180/2^15
```

After validation, the resolution expression is solved and stored as a `double`, not as the raw string.

For the above example:

```text
resolution = 0.0054931640625
```

### 7.2 `RawPacket`

```cpp
struct RawPacket
{
    quint64 packetNumber;
    quint64 tsSec;
    quint32 tsUsec;
    quint32 linkType;
    QByteArray data;
};
```

Meaning:

```text
packetNumber = sequential packet number assigned by the reader
tsSec        = packet timestamp seconds
tsUsec       = packet timestamp microseconds
linkType     = capture link type, currently Ethernet is accepted
data         = raw packet bytes from capture record
```

This is the output of `PcapFileReader`.

### 7.3 `ParsedUdpPacket`

```cpp
struct ParsedUdpPacket
{
    bool valid;
    QString timestamp;
    QString sourceIp;
    QString destinationIp;
    int sourcePort;
    int destinationPort;
    int payloadSize;
    QByteArray udpPayload;
    QString error;
};
```

Meaning:

```text
valid           = true only if Ethernet + IPv4 + UDP parsing succeeds
timestamp       = formatted timestamp text
sourceIp        = IPv4 source address
destinationIp   = IPv4 destination address
sourcePort      = UDP source port
destinationPort = UDP destination port
payloadSize     = UDP payload size in bytes
udpPayload      = actual bytes after UDP header
error           = reason for invalid packet, used internally
```

This is the output of `UdpPacketParser`.

---

## 8. UI Layer: `MainWindow`

`MainWindow` is the controller of the application.

It handles:

```text
file browsing
adding/removing field rows
reading field table data
validating input
opening save dialog
starting packet processing
updating preview table
showing status messages
showing error dialogs
```

### 8.1 UI elements

The current UI is built manually in `headers/ui_MainWindow.h`.

Main controls:

```text
txtFilePath      -> selected capture file path
btnBrowse        -> opens file picker
spinPort         -> UDP port selector, range 0 to 65535
btnStart         -> starts extraction
btnAddField      -> adds field definition row
btnRemoveField   -> removes selected field rows
tblFields        -> field definition input table
tblOutput        -> extracted output preview table
lblStatus        -> current status text
progressBar      -> busy indicator while processing
```

The window title is:

```text
PCAP UDP Extractor
```

The initial window size is:

```text
1100 x 760
```

### 8.2 Field input table

The field input table has four columns:

```text
Field | Byte | Length | Resolution
```

Default row added by `onAddFieldClicked()`:

```text
Field name : Field1
Byte       : 0
Length     : 2
Resolution : 1
```

### 8.3 Output preview table

The output table is read-only.

Preview is limited by this constant:

```cpp
static const int PREVIEW_ROW_LIMIT = 5000;
```

Reason: the application writes all rows to CSV but only previews the first 5000 rows to avoid freezing low-spec systems.

---

## 9. Export File Naming Logic

The export filename is automatically generated using this format:

```text
<uploaded_file_name>_<today_date>_<system_time>.csv
```

The code responsible is inside `sources/MainWindow.cpp`.

### 9.1 Unsafe filename cleanup

```cpp
QString safeExportBaseName(QString name)
{
    name = name.trimmed();

    if (name.isEmpty())
    {
        name = "export";
    }

    name.replace(QRegExp("[\\\\/:*?\"<>|]"), "_");
    name.replace(QRegExp("\\s+"), "_");

    return name;
}
```

This function:

1. Trims spaces.
2. Uses `export` if name is empty.
3. Replaces Windows-unsafe filename characters with `_`.
4. Replaces repeated spaces with `_`.

### 9.2 Final CSV filename builder

```cpp
QString buildDefaultCsvFileName(const QString& inputFilePath)
{
    const QFileInfo inputInfo(inputFilePath.trimmed());
    const QString uploadedName = safeExportBaseName(inputInfo.completeBaseName());
    const QString today = QDate::currentDate().toString("yyyyMMdd");
    const QString systemTime = QTime::currentTime().toString("HHmmss");

    return QString("%1_%2_%3.csv")
        .arg(uploadedName)
        .arg(today)
        .arg(systemTime);
}
```

Example:

```text
Input file: radar sample.pcapng
Date      : 2026-05-15
Time      : 14:30:22
Output    : radar_sample_20260515_143022.csv
```

The save dialog opens in the same folder as the selected input file:

```cpp
const QFileInfo inputInfo(ui->txtFilePath->text().trimmed());
const QString defaultCsvName = buildDefaultCsvFileName(ui->txtFilePath->text());
const QString defaultCsvPath = inputInfo.absoluteDir().filePath(defaultCsvName);
```

The user can still manually change the name or location in the save dialog.

---

## 10. Input Validation Logic

Validation is handled by `InputValidator`.

### 10.1 File validation

Function:

```cpp
InputValidator::validateFilePath(const QString& filePath, QString& errorMessage)
```

Checks performed:

```text
file path is not empty
file exists
path points to a file, not folder
file size is greater than 0
file size is not greater than 500 MB
extension is .pcap or .pcapng
file can be opened for reading
```

Maximum file size:

```cpp
const qint64 MAX_CAPTURE_FILE_SIZE = 500LL * 1024LL * 1024LL;
```

Reason: this is a lightweight version intended for low-spec machines.

### 10.2 UDP port validation

Valid UDP port range:

```text
0 to 65535
```

Function:

```cpp
InputValidator::validatePortValue(int port, QString& errorMessage)
```

Rule:

```cpp
if (port < 0 || port > 65535)
{
    errorMessage = "UDP port must be between 0 and 65535.";
    return false;
}
```

### 10.3 Field validation

Each field row is checked by:

```cpp
InputValidator::validateField(name, byteText, lengthText, resolutionText, errorMessage)
```

Rules:

```text
field name cannot be empty
byte offset must be integer >= 0
length must be integer > 0
length must be <= 8
resolution expression must evaluate successfully
resolution result must be > 0
byte offset must not be extremely large
```

Maximum supported field length:

```text
8 bytes
```

Reason: the extraction engine stores raw extracted integer value in `quint64`, which can safely hold up to 8 bytes.

### 10.4 Duplicate field name validation

After collecting all fields, `validateFields()` checks duplicate names case-insensitively.

Example duplicates rejected:

```text
Heading
heading
HEADING
```

All are treated as same field name.

---

## 11. Resolution Expression Logic

The project supports mathematical resolution expressions through `MathExpressionEvaluator`.

Supported examples:

```text
1
0.1
180/2^15
360/2^16
(180 + 20) / 2^15
1e-3
pi/180
e
```

Supported operators:

```text
+   addition
-   subtraction
*   multiplication
/   division
^   power/exponent
()  grouping
```

Supported constants:

```text
pi
e
```

### 11.1 Why this evaluator is required

Qt's `QString::toDouble()` only works for plain decimal strings.

It can read:

```text
0.005493
```

It cannot solve:

```text
180/2^15
```

So the project uses a custom parser. Without this parser, the resolution text may be converted incorrectly or rejected.

### 11.2 Parser structure

The parser uses recursive descent parsing.

Function levels:

```text
parseExpression -> handles + and -
parseTerm       -> handles * and /
parseUnary      -> handles unary + and unary -
parsePower      -> handles ^
parsePrimary    -> handles brackets, constants, numbers
parseNumber     -> handles decimal/scientific notation
```

### 11.3 Power operator handling

Critical point:

```text
^ means exponent in this project.
```

C++ normally treats `^` as bitwise XOR, not exponentiation. This project avoids that problem by explicitly parsing `^` and calling:

```cpp
std::pow(left, right);
```

### 11.4 Division by zero protection

Inside division logic:

```cpp
if (right == 0.0)
{
    setError("Division by zero.");
    return 0.0;
}
```

So expressions like this are rejected:

```text
180/0
```

### 11.5 Final resolution validation

After evaluation, `InputValidator` checks:

```cpp
if (value <= 0.0)
{
    errorMessage = "Resolution must produce a value greater than 0.";
    return false;
}
```

So zero or negative resolution is not allowed.

---

## 12. Correct Heading Calculation Example

Given field:

```text
Field name  : Heading
Byte offset : 15
Length      : 2
Resolution  : 180/2^15
Hex bytes   : 16 05
```

Step 1: read bytes from UDP payload.

```text
16 05
```

Step 2: combine bytes as unsigned big-endian integer.

```text
0x1605 = 5637
```

Step 3: solve resolution expression.

```text
180 / 2^15
= 180 / 32768
= 0.0054931640625
```

Step 4: multiply raw decimal value by resolution.

```text
5637 * 0.0054931640625
= 30.9649658203125
```

Current formatting keeps up to 6 decimal places and removes useless trailing zeros.

Expected displayed/exported result:

```text
30.964966
```

Important: this result comes from `180/2^15`, not `180/2^25`.

If `180/2^25` is used:

```text
180 / 33554432 = 0.000005364418029785156
5637 * 0.000005364418029785156 = 0.030243...
```

So `180/2^25` cannot produce approximately `30.964` for raw decimal `5637`.

### 12.1 Byte offset warning

The project uses zero-based byte offsets.

```text
UI byte offset 0  = first byte of UDP payload
UI byte offset 1  = second byte of UDP payload
UI byte offset 15 = sixteenth byte of UDP payload
```

So if a document says `byte number 15 and 16` using human counting, the UI offset should usually be:

```text
14
```

If the document says `offset 15 and 16`, then the UI offset should be:

```text
15
```

This is one of the most common reasons for a wrong value even when the formula is correct.

---

## 13. PCAP Reading Logic

The class `PcapFileReader` reads capture files.

It supports:

```text
classic PCAP
limited PCAPNG
```

It reads one packet at a time. It does not load the whole file into memory.

### 13.1 Format detection

When a file is opened:

```cpp
const QByteArray firstBytes = m_file.read(12);
```

The first bytes are checked.

If first four bytes are:

```text
0A 0D 0D 0A
```

then the file is treated as PCAPNG.

Otherwise the code reads the 24-byte classic PCAP global header.

### 13.2 Classic PCAP magic numbers

Supported PCAP magic values:

```text
A1 B2 C3 D4 -> big-endian microsecond PCAP
D4 C3 B2 A1 -> little-endian microsecond PCAP
A1 B2 3C 4D -> big-endian nanosecond PCAP
4D 3C B2 A1 -> little-endian nanosecond PCAP
```

The reader sets:

```text
byte order
whether timestamp is nanosecond or microsecond
link type
```

Link type is read from global header offset 20.

### 13.3 Classic PCAP packet records

Each classic PCAP packet record has a 16-byte packet header.

The code reads:

```text
ts_sec          -> offset 0
ts_subsec       -> offset 4
capturedLength  -> offset 8
```

Then it reads `capturedLength` bytes as packet data.

Packet size is rejected if:

```text
capturedLength == 0
capturedLength > 16 MB
```

Limit:

```cpp
const quint32 MAX_SINGLE_PACKET_SIZE = 16U * 1024U * 1024U;
```

### 13.4 Nanosecond PCAP timestamp handling

If a classic PCAP file uses nanosecond timestamp format, the code converts nanoseconds to microseconds:

```cpp
packet.tsUsec = m_nanosecondPcap ? static_cast<quint32>(tsSubSec / 1000U) : tsSubSec;
```

---

## 14. PCAPNG Reading Logic

PCAPNG is block-based. The reader supports the blocks needed for this tool.

Supported PCAPNG blocks:

```text
Section Header Block             0x0A0D0D0A
Interface Description Block      0x00000001
Simple Packet Block              0x00000003
Enhanced Packet Block            0x00000006
```

Unsupported block types are skipped.

### 14.1 PCAPNG block length protection

Each PCAPNG block has a total length.

The project rejects invalid block sizes:

```text
totalLength < 12
totalLength > 64 MB
```

Limit:

```cpp
const quint32 MAX_PCAPNG_BLOCK_SIZE = 64U * 1024U * 1024U;
```

The code also verifies that the trailing block length matches the header block length:

```cpp
if (trailingLength != totalLength)
{
    errorMessage = "PCAPNG block length mismatch.";
    return false;
}
```

### 14.2 Interface Description Block

The Interface Description Block stores link type and timestamp resolution information.

Current accepted link type:

```text
1 = Ethernet
```

The code also checks PCAPNG option code `9`, which is timestamp resolution.

Default timestamp resolution:

```text
1,000,000 units per second
```

That means default timestamps are interpreted as microsecond-based.

### 14.3 Enhanced Packet Block

Enhanced Packet Block contains:

```text
interface ID
timestamp high
timestamp low
captured length
original length
packet data
```

The project:

1. Finds the interface by interface ID.
2. Rejects the packet if interface is not Ethernet.
3. Combines timestamp high and low into a 64-bit timestamp.
4. Converts timestamp into seconds and microseconds.
5. Reads packet bytes from the block.
6. Returns a `RawPacket`.

### 14.4 Simple Packet Block

Simple Packet Block has packet bytes but no timestamp.

So the project returns:

```text
tsSec  = 0
tsUsec = 0
```

The UI displays timestamp as:

```text
N/A
```

for this case.

---

## 15. Ethernet Parsing Logic

Ethernet parsing is handled by `UdpPacketParser`.

Current accepted link type:

```cpp
const quint32 LINK_ETHERNET = 1;
```

If the raw packet link type is not Ethernet:

```text
Unsupported link type.
```

The minimum Ethernet II frame header size is:

```text
14 bytes
```

Ethernet structure used:

```text
Destination MAC  -> bytes 0-5
Source MAC       -> bytes 6-11
EtherType        -> bytes 12-13
Payload          -> starts at byte 14
```

The parser reads EtherType at offset 12.

IPv4 EtherType:

```text
0x0800
```

If EtherType is not IPv4, the packet is skipped.

---

## 16. VLAN Handling

The parser handles basic VLAN tags.

Supported VLAN EtherTypes:

```text
0x8100
0x88A8
```

If a VLAN tag is found, the parser shifts the network offset forward from 14 to 18.

Logic:

```cpp
if (etherType == 0x8100 || etherType == 0x88A8)
{
    etherTypeOffset = 16;
    etherType = r16(data, etherTypeOffset);
    networkOffset = 18;
}
```

This allows the parser to reach IPv4 even when one VLAN tag is present.

Current limitation: only one VLAN tag level is handled.

---

## 17. IPv4 Parsing Logic

After Ethernet parsing, the parser reads IPv4.

Important IPv4 fields:

```text
Version/IHL       -> byte 0
Total Length      -> bytes 2-3
Flags/Fragment    -> bytes 6-7
Protocol          -> byte 9
Source IP         -> bytes 12-15
Destination IP    -> bytes 16-19
```

### 17.1 Version and IHL

Code extracts version and header length:

```cpp
const quint8 versionIhl = static_cast<quint8>(data.at(networkOffset));
const int version = (versionIhl >> 4) & 0x0F;
const int ihl = versionIhl & 0x0F;
const int ipHeaderLength = ihl * 4;
```

Rules:

```text
version must be 4
IPv4 header length must be at least 20 bytes
packet must contain full IPv4 header
```

### 17.2 Total length validation

The parser reads IPv4 total length:

```cpp
const quint16 totalLength = r16(data, networkOffset + 2);
```

Rules:

```text
totalLength must be at least IP header length + UDP header length
the captured frame must contain the full IPv4 packet
```

UDP header length is 8 bytes.

### 17.3 Fragment rejection

Fragmented IPv4 packets are skipped.

Code:

```cpp
const quint16 flagsAndFragment = r16(data, networkOffset + 6);
const int fragmentOffset = flagsAndFragment & 0x1FFF;
const bool moreFragments = (flagsAndFragment & 0x2000) != 0;
if (fragmentOffset != 0 || moreFragments)
{
    parsed.error = "Fragmented IPv4 packet skipped.";
    return parsed;
}
```

Reason: if an IPv4 packet is fragmented, the UDP header or payload may not be complete in that packet. This project does not perform IP reassembly.

### 17.4 Protocol check

UDP protocol number is:

```text
17
```

If protocol is not 17, packet is skipped.

---

## 18. UDP Parsing Logic

After IPv4 validation, the parser finds UDP header offset:

```cpp
const int udpOffset = networkOffset + ipHeaderLength;
```

UDP header fields:

```text
Source Port       -> bytes 0-1
Destination Port  -> bytes 2-3
UDP Length        -> bytes 4-5
Checksum          -> bytes 6-7
Payload           -> starts at byte 8
```

The parser reads:

```cpp
parsed.sourcePort = r16(data, udpOffset);
parsed.destinationPort = r16(data, udpOffset + 2);
const quint16 udpLength = r16(data, udpOffset + 4);
```

Validation rules:

```text
UDP length must be at least 8
UDP end must not exceed IPv4 packet end
UDP end must not exceed captured data size
```

Payload extraction:

```cpp
const int payloadOffset = udpOffset + 8;
const int payloadLength = static_cast<int>(udpLength) - 8;
parsed.udpPayload = data.mid(payloadOffset, payloadLength);
parsed.payloadSize = payloadLength;
parsed.valid = true;
```

At this point the parser has produced a valid `ParsedUdpPacket`.

---

## 19. UDP Port Filtering Logic

Port filtering is done in `MainWindow::onStartClicked()` after UDP parsing.

Code:

```cpp
if (parsed.sourcePort != port && parsed.destinationPort != port)
{
    continue;
}
```

Meaning:

A packet is accepted if either:

```text
source UDP port == selected port
```

or:

```text
destination UDP port == selected port
```

This makes the tool useful for both directions of communication.

---

## 20. Field Extraction Logic

Field extraction is handled by `ExtractionEngine`.

Function:

```cpp
QString ExtractionEngine::valueFromPayload(const QByteArray& payload, const FieldDefinition& field)
```

### 20.1 Boundary checks

Before extraction, the engine checks:

```text
byte offset must be >= 0
length must be > 0
length must be <= 8
offset + length must not exceed payload size
```

If a field cannot be extracted, it returns:

```text
N/A
```

### 20.2 Raw byte reading

The selected bytes are read as unsigned big-endian.

Code:

```cpp
quint64 readUnsignedBigEndianRawValue(const QByteArray& payload, int byteOffset, int length)
{
    quint64 rawValue = 0;

    for (int i = 0; i < length; ++i)
    {
        rawValue <<= 8;
        rawValue |= static_cast<quint8>(payload.at(byteOffset + i));
    }

    return rawValue;
}
```

Example:

```text
Bytes: 16 05

Start rawValue = 0
After 16       = 0x16
After 05       = 0x1605
Decimal        = 5637
```

### 20.3 Resolution multiplication

After raw decimal conversion:

```cpp
const double calculatedValue = static_cast<double>(rawDecimalValue) * field.resolution;
```

Formula:

```text
Final value = raw unsigned big-endian decimal value × solved resolution
```

### 20.4 Formatting

If resolution is exactly `1.0`, the raw integer is returned directly:

```cpp
if (field.resolution == 1.0)
{
    return QString::number(static_cast<qulonglong>(rawDecimalValue));
}
```

If resolution is not `1.0`, the calculated value is formatted with up to six decimal places:

```cpp
QString::number(value, 'f', 6)
    .remove(QRegExp("0+$"))
    .remove(QRegExp("\\.$"));
```

Examples:

```text
30.964966 -> 30.964966
12.500000 -> 12.5
10.000000 -> 10
```

---

## 21. Output Row Structure

Every exported row contains fixed packet metadata columns plus custom field columns.

Fixed headers:

```text
Packet No
Timestamp
Source IP
Destination IP
Source UDP Port
Destination UDP Port
Payload Size
```

Then each user-defined field is added as a column.

Example headers:

```text
Packet No, Timestamp, Source IP, Destination IP, Source UDP Port, Destination UDP Port, Payload Size, Heading, Range, Speed
```

Row building code:

```cpp
QStringList row;
row << QString::number(static_cast<qulonglong>(rawPacket.packetNumber));
row << parsed.timestamp;
row << parsed.sourceIp;
row << parsed.destinationIp;
row << QString::number(parsed.sourcePort);
row << QString::number(parsed.destinationPort);
row << QString::number(parsed.payloadSize);
row << ExtractionEngine::valuesFromPayload(parsed.udpPayload, fields);
```

---

## 22. CSV Export Logic

CSV export is handled by `CsvExporter`.

### 22.1 Opening output file

```cpp
m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)
```

Meaning:

```text
WriteOnly = file is opened for writing
Text      = text mode
Truncate  = old content is removed if file already exists
```

For Qt versions below 6, UTF-8 codec is set:

```cpp
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_stream.setCodec("UTF-8");
#endif
```

### 22.2 Header row

When the CSV file opens, the first row written is the header row.

```cpp
return writeRow(headers, errorMessage);
```

### 22.3 Cell escaping

Cells are quoted if they contain:

```text
comma
double quote
newline
carriage return
```

Double quotes inside cells are doubled.

Example:

```text
Input cell : hello "radar", test
CSV cell   : "hello ""radar"", test"
```

### 22.4 Excel formula injection protection

If a cell starts with any of these characters:

```text
=
+
-
@
```

then the exporter prefixes an apostrophe.

Code:

```cpp
if (first == QChar('=') || first == QChar('+') || first == QChar('-') || first == QChar(64))
{
    cell.prepend(QChar(39));
}
```

Reason: Excel can treat such cells as formulas. This protection prevents accidental formula execution when opening the CSV.

---

## 23. Processing Loop Logic

The main processing loop is inside `MainWindow::onStartClicked()`.

Simplified logic:

```cpp
while (true)
{
    RawPacket rawPacket;
    QString readError;
    const bool hasPacket = reader.readNextPacket(rawPacket, readError);

    if (!hasPacket)
    {
        if (!readError.isEmpty())
        {
            failed = true;
            errorMessage = readError;
        }
        break;
    }

    ++totalPackets;

    ParsedUdpPacket parsed = UdpPacketParser::parsePacket(rawPacket);
    if (!parsed.valid)
    {
        continue;
    }

    ++validUdpPackets;

    if (parsed.sourcePort != port && parsed.destinationPort != port)
    {
        continue;
    }

    ++matchedPackets;

    // build row
    // write CSV row
    // append preview row if under limit
}
```

Counters maintained:

```text
totalPackets    = all raw packets read from capture
validUdpPackets = packets that successfully parsed as Ethernet + IPv4 + UDP
matchedPackets  = valid UDP packets where source or destination port matched
exportedRows    = rows successfully written to CSV
preview rows    = rows shown in UI table, max 5000
```

The status label is updated every 500 packets:

```cpp
if ((totalPackets % 500) == 0)
{
    setStatus(QString("Processing... total=%1, UDP=%2, matched=%3")...);
    QApplication::processEvents();
}
```

`QApplication::processEvents()` keeps the UI from looking completely frozen during processing.

---

## 24. Busy State Logic

When processing starts, UI controls are disabled:

```cpp
ui->btnStart->setEnabled(!busy);
ui->btnBrowse->setEnabled(!busy);
ui->btnAddField->setEnabled(!busy);
ui->btnRemoveField->setEnabled(!busy);
ui->spinPort->setEnabled(!busy);
ui->tblFields->setEnabled(!busy);
```

When busy, progress bar becomes indeterminate:

```cpp
ui->progressBar->setRange(0, 0);
```

When finished, progress bar returns to normal and shows 100:

```cpp
ui->progressBar->setRange(0, 100);
ui->progressBar->setValue(100);
```

---

## 25. Error Handling Strategy

The project does not crash on malformed packets. It rejects them safely.

Examples of parser errors:

```text
Packet is smaller than Ethernet header.
Truncated VLAN Ethernet frame.
Not an IPv4 packet.
Packet is smaller than minimum IPv4 header.
Invalid IPv4 header length.
Truncated IPv4 header.
Invalid IPv4 total length.
Truncated IPv4 packet.
Fragmented IPv4 packet skipped.
Not a UDP packet.
Truncated UDP header.
Invalid UDP length.
Truncated UDP payload.
```

Most packet-level parse errors only skip that packet.

File-level errors stop processing, for example:

```text
Truncated PCAP packet header.
Invalid PCAPNG block length.
PCAPNG block length mismatch.
Cannot open capture file.
```

---

## 26. Current Supported Features

The current project supports:

```text
Qt Widgets UI
qmake build system
classic PCAP reading
limited PCAPNG reading
Ethernet link type
Ethernet II frame parsing
basic VLAN tag handling
IPv4 parsing
UDP parsing
UDP port filtering by source or destination port
custom payload field extraction
unsigned big-endian integer extraction
resolution expression evaluation
CSV export
Excel-compatible output
CSV formula-injection protection
preview table
5000-row preview limit
safe default export filename
```

---

## 27. Current Limitations

The project currently does not support:

```text
live packet capture
TCP parsing
IPv6 parsing
ARP/ICMP decoding
IP fragment reassembly
full PCAPNG feature coverage
multiple nested VLAN tags
non-Ethernet link types
little-endian payload field extraction
signed integer field extraction
floating-point payload field extraction
bit-level field extraction
native .xlsx export
multi-threaded processing
cancel/stop button during extraction
checksum validation
protocol-specific radar/message decoding beyond raw payload fields
```

These are not bugs. They are current scope limits.

---

## 28. Build Instructions

### 28.1 Build using Qt Creator

```text
1. Open Qt Creator.
2. File -> Open File or Project.
3. Select PcapUdpExtractor.pro.
4. Select a Qt kit.
5. Build the project.
6. Run the application.
```

### 28.2 Build using command line

Linux/macOS style:

```bash
qmake PcapUdpExtractor.pro
make
```

Windows MinGW style:

```bash
qmake PcapUdpExtractor.pro
mingw32-make
```

If new files were added and Qt Creator does not detect them, clean and rebuild.

---

## 29. Normal User Workflow

```text
1. Start the application.
2. Click Browse.
3. Select a .pcap or .pcapng file.
4. Enter the UDP port to extract.
5. Click Add Field for each payload field.
6. Fill Field, Byte, Length, Resolution.
7. Click Start.
8. Confirm or change the CSV save path.
9. Wait for processing to finish.
10. Open the CSV in Excel.
```

Example field table:

```text
Field      Byte    Length    Resolution
Heading    15      2         180/2^15
Range      17      4         1
Speed      21      2         0.1
```

---

## 30. Debugging Wrong Extracted Values

If the output value is wrong, check in this exact order.

### 30.1 Confirm byte offset base

The UI uses zero-based offset from UDP payload.

If your protocol document uses human byte numbering, subtract 1.

```text
Human byte 1  -> UI offset 0
Human byte 15 -> UI offset 14
```

### 30.2 Confirm bytes are from UDP payload, not full frame

The field byte offset is not from:

```text
PCAP record start
Ethernet frame start
IPv4 header start
UDP header start
```

It is from:

```text
first byte after UDP header
```

### 30.3 Confirm extracted hex bytes

For a 2-byte field, verify the two payload bytes manually.

Example:

```text
Expected bytes: 16 05
Expected decimal: 5637
```

### 30.4 Confirm endian rule

Current extraction is unsigned big-endian.

```text
16 05 -> 0x1605 -> 5637
05 16 -> 0x0516 -> 1302
```

If protocol uses little-endian, current output will not match. That is a future enhancement.

### 30.5 Confirm resolution expression

For heading:

```text
180/2^15 = 0.0054931640625
```

Not:

```text
180/2^25
```

### 30.6 Confirm latest executable is running

If code was fixed but output still looks old:

```text
Clean project
Run qmake again
Rebuild
Make sure you launched the newly built .exe
```

---

## 31. Why the Earlier Resolution Error Happened

The earlier wrong result came from treating resolution as a plain number instead of a mathematical expression.

Incorrect approach:

```cpp
field.resolution = resolutionText.toDouble();
```

Problem:

```text
QString::toDouble() cannot calculate 180/2^15.
```

Correct approach now:

```cpp
double solvedResolution = 0.0;
InputValidator::solveResolutionExpression(resolutionText, solvedResolution, errorMessage);
field.resolution = solvedResolution;
```

Then `ExtractionEngine` multiplies using the solved numeric value.

Correct pipeline:

```text
resolution text: 180/2^15
  -> MathExpressionEvaluator
  -> 0.0054931640625
  -> stored in field.resolution
  -> raw value 5637 * 0.0054931640625
  -> 30.964966
```

The extraction engine itself was not the main problem. The problem was the resolution value being stored incorrectly before extraction.

---

## 32. Security and Stability Measures

The project includes these protections:

```text
file extension validation
file existence validation
empty file rejection
500 MB capture file limit
packet size limit
PCAPNG block size limit
PCAPNG trailing length verification
port range validation
field length limit of 8 bytes
field boundary checks
invalid packet skipping
fragmented IPv4 skipping
duplicate field name detection
CSV escaping
CSV formula-injection protection
no full-file memory loading
preview row limit
```

These measures make the application safer for malformed files and low-spec systems.

---

## 33. What to Say in Project Explanation/Viva

Use this explanation:

```text
This project is an offline Qt Widgets application written in C++ using qmake. It reads PCAP and limited PCAPNG files manually using QFile. It does not depend on libpcap or external libraries. The user selects a capture file, enters a UDP port, and defines custom fields inside the UDP payload. The application reads packets one by one, parses Ethernet II, IPv4, and UDP headers, filters packets by source or destination UDP port, extracts user-defined payload bytes as unsigned big-endian integers, applies resolution expressions such as 180/2^15, previews the results in a table, and exports the complete output to a CSV file compatible with Excel. The design is modular: MainWindow handles UI, InputValidator handles validation, PcapFileReader reads capture files, UdpPacketParser parses network headers, ExtractionEngine extracts and scales payload values, CsvExporter writes safe CSV output, and MathExpressionEvaluator solves resolution formulas.
```

---

## 34. Future Upgrade Plan

Recommended clean upgrades:

```text
1. Move packet processing to QThread so UI remains responsive.
2. Add Stop/Cancel button.
3. Add endian selector per field: big-endian/little-endian.
4. Add field type selector: unsigned, signed, float, double.
5. Add bit-level extraction for fields not aligned to full bytes.
6. Add protocol template import/export.
7. Add native XLSX export if an approved offline library is allowed.
8. Add IPv6 support.
9. Add TCP parsing if needed.
10. Add detailed packet error report export.
11. Add checksum verification as optional validation.
12. Add support for deeper PCAPNG options.
```

---

## 35. Final Summary

The project is a clean, modular, offline PCAP/PCAPNG UDP payload extraction tool.

The strongest parts of the current design are:

```text
small module separation
no external dependency
manual packet parsing logic
safe input validation
low-memory packet-by-packet reading
custom user-defined payload extraction
resolution expression support
Excel-compatible CSV export
clear scope suitable for internship explanation
```

The most important technical rule is:

```text
Field byte offsets are zero-based and start from the UDP payload, not from the Ethernet frame or PCAP file.
```

The most important calculation rule is:

```text
Final value = unsigned big-endian raw decimal value × solved resolution expression
```

For the heading example:

```text
0x1605 = 5637
180/2^15 = 0.0054931640625
5637 × 0.0054931640625 = 30.9649658203125
Displayed/exported value = 30.964966
```
