# PcapUdpExtractor

A lightweight offline Qt Widgets application for reading packet capture files and exporting selected UDP payload fields to a CSV file that opens directly in Microsoft Excel.

This project is designed for an offline, low-spec development system:

- qmake project format, not CMake
- Qt Widgets UI
- C++11 compatible code
- No external libraries
- No libpcap, Npcap, QXlsx, Boost, Python, database, or internet dependency
- Manual `.pcap` and limited `.pcapng` reading
- Ethernet II + IPv4 + UDP parsing
- CSV export for Excel compatibility

## Project Purpose

The user selects a `.pcap` or `.pcapng` file, enters a UDP port number, defines custom payload fields, and clicks **Start**. The application reads packets from the capture file, extracts Ethernet/IPv4/UDP packets, filters packets by source or destination UDP port, extracts values from the UDP payload using user-defined field definitions, displays a preview in the UI table, and writes all extracted rows to a CSV file.

Current port-filter exports use message definitions instead of one global field table. A port owns length filters, each length filter defines one message, and each message owns its own fields. This keeps different UDP payload layouts on the same port from being mixed during export.

## Supported

- `.pcap` files
- `.pcapng` files with Ethernet interfaces and Enhanced Packet Blocks / Simple Packet Blocks
- Ethernet II frames
- IPv4 packets
- UDP datagrams
- UDP source/destination port filtering
- User-defined UDP payload field extraction (byte-offset / HEX mode)
- **NMEA 0183 decoding** — decode ASCII marine sentences (GGA, RMC, GLL, …) by sentence formatter (see below)
- Live UDP capture mode (listen on a socket and stream extraction to CSV in real time)
- Per-message length filters with optional header disambiguation
- CSV export
- UI preview limit to avoid freezing low-spec PCs

## Not Supported Yet

- TCP parsing
- IPv6 parsing
- ARP / ICMP decoding
- NMEA `!` encapsulation sentences (AIS six-bit payloads) — only `$` parametric sentences are decoded
- Native `.xlsx` export
- Multi-threaded parsing

## Folder Layout

```text
pcapfile/
├── PcapUdpExtractor.pro
├── README.md
├── PROJECT_CONTEXT_FOR_OLLAMA.txt
├── .gitignore
├── headers/
│   ├── MainWindow.h
│   ├── AppTypes.h
│   ├── FieldDefinition.h
│   ├── InputValidator.h
│   ├── PcapFileReader.h
│   ├── UdpPacketParser.h
│   ├── ExtractionEngine.h
│   └── CsvExporter.h
├── sources/
│   ├── main.cpp
│   ├── MainWindow.cpp
│   ├── FieldDefinition.cpp
│   ├── InputValidator.cpp
│   ├── PcapFileReader.cpp
│   ├── UdpPacketParser.cpp
│   ├── ExtractionEngine.cpp
│   └── CsvExporter.cpp
├── forms/
│   └── MainWindow.ui
├── docs/
│   ├── WORKING_MANUAL.md
│   └── TECHNICAL_LOGIC.md
├── test_files/
└── output/
```

## Build Instructions

Open the project in Qt Creator:

```text
File → Open File or Project → PcapUdpExtractor.pro
```

Then select your Qt kit and build normally.

Command-line qmake build example:

```bash
qmake PcapUdpExtractor.pro
make
```

On Windows with MinGW:

```bash
qmake PcapUdpExtractor.pro
mingw32-make
```

## How to Use

1. Open the application.
2. Click **Browse**.
3. Select a `.pcap` or `.pcapng` file.
4. Select **Port** or **Header** filter mode.
5. For Port mode, enter ports and click **Manage Length Filters**.
6. Add each message name and **Payload Length (bytes)**.
7. Configure fields for each message definition:
   - Field Name
   - Byte Offset
   - Length
   - Resolution
8. Click **Start Export**.
9. Choose a CSV output folder.
10. The application creates one CSV per configured message definition.

## Field Definition Logic

Fields are extracted from the UDP payload only, not from the full Ethernet frame.

Example:

| Field Name | Byte Offset | Length | Resolution |
|---|---:|---:|---:|
| Heading | 0 | 2 | 0.01 |
| Range | 2 | 4 | 1 |
| Speed | 6 | 2 | 0.1 |

Formula:

```text
Final Value = Raw Unsigned Big-Endian Integer × Resolution
```

If a field range is outside the UDP payload, the application writes `N/A`.

## NMEA 0183 Decoding

In addition to byte-offset (HEX) extraction, a message can be set to **Data Format = NMEA** to decode ASCII NMEA 0183 marine sentences instead of raw bytes.

NMEA sentences look like:

```text
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47<CR><LF>
```

- The 2-character **talker** (`GP`) plus 3-character **sentence formatter** (`GGA`) identify the sentence.
- Data fields are **comma-delimited and positional** (no byte offsets) and may be empty (null) fields.
- `*47` is an XOR **checksum** of the characters between `$` and `*`; it is validated during decode.

How it works in the app:

1. Add a length filter and set **Data Format** to **NMEA**, then pick a sentence formatter.
2. Click **Configure Fields** — the field list is driven by a built-in registry for that sentence. Enable the fields you want and optionally rename their CSV columns.
3. NMEA messages are matched to packets **by sentence formatter** found in the payload (not by exact byte length), so variable-length sentences match correctly.
4. On export, each decoded sentence becomes one CSV row; a datagram carrying multiple sentences produces multiple rows. Latitude/longitude, time and date fields are formatted for readability.

All **87 approved parametric sentence formatters** from NMEA 0183 v3.01 (AAM through ZTG) are available. The common GNSS / navigation sentences (GGA, RMC, GLL, VTG, GSA, GSV, ZDA, GNS, GST, HDG, HDT, VHW, VLW, MWV, MWD, DBT, DPT, RMA, RMB, RSA, ROT, …) ship with descriptive field names; the remaining sentences use type-correct names derived from the standard's field templates. The catalogue is extensible.

## Output Columns

Header filter CSV output keeps the previous packet metadata columns:

```text
Packet No, Timestamp, Source IP, Destination IP, Source UDP Port, Destination UDP Port, Payload Size, Custom Fields...
```

Port message CSV output is split per message definition and uses that message's fields only. See `docs/PORT_LENGTH_MESSAGE_FILTER_WORKFLOW.md` for the full workflow and validation rules.

## Security and Stability

The application includes:

- File extension validation
- File size limit
- UDP port validation
- Field validation
- Duplicate field-name detection
- Packet boundary checks
- Safe malformed-packet rejection
- CSV formula-injection protection
- Sequential packet reading to avoid high RAM usage
- UI preview limit of 5000 rows

## Recommended First Test

Use a small Ethernet IPv4 UDP `.pcap` file first. Enter a UDP port known to exist in the capture and define a simple field such as:

```text
Field Name: FirstTwoBytes
Byte Offset: 0
Length: 2
Resolution: 1
```

## Mentor Explanation

This is an offline qmake-based Qt Widgets application. It manually reads PCAP/PCAPNG capture files using QFile and QByteArray, parses Ethernet II, IPv4, and UDP headers, filters by UDP port, extracts user-defined fields from UDP payload data, previews the result in a Qt table, and exports the complete result to CSV for Excel.
