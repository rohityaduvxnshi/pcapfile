# Multi-Filter Workflow Upgrade

This document explains the updated workflow added to the PCAP UDP Extractor project.

## Updated Workflow

```text
Select PCAP/PCAPNG file
  -> enter number of message filters
  -> choose exactly one filter mode
       -> Port Filter
       -> Header Filter
  -> enter the required filter values
  -> define UDP payload fields
  -> export partitioned CSV files
```

The old workflow used one UDP port and one CSV output. The new workflow supports multiple output partitions in one extraction run.

## Filter Modes

Only one filter mode can be active at a time.

### Port Filter Mode

In Port Filter mode, the user enters N UDP port values, where N is the value of **Number of Message Filters**.

A packet matches a port filter if:

```text
packet source UDP port == filter port
OR
packet destination UDP port == filter port
```

Each port filter creates one CSV file.

Example:

```text
Number of Message Filters = 3
Ports = 4001, 4002, 5000
```

Generated files:

```text
<base>_port_4001.csv
<base>_port_4002.csv
<base>_port_5000.csv
```

Duplicate ports are rejected.

If a packet matches more than one configured port because source and destination ports are both in the list, the packet is written to the first matching filter in UI order. This keeps CSV partitions from duplicating rows.

### Header Filter Mode

In Header Filter mode, the user enters:

```text
one common UDP port
N header filters
```

A packet matches only if:

```text
source UDP port == common port
OR
destination UDP port == common port
```

Then the UDP payload is checked against the header filters.

A header match means:

```text
UDP payload starts with the configured header bytes
```

Header input rules:

```text
Allowed length: 0 to 4 bytes
Allowed hex characters: 0-9, A-F, a-f
Allowed text lengths: empty, 2, 4, 6, or 8 hex characters
Case-insensitive input
Spaces are ignored
Stored label is uppercase
```

Examples:

```text
A1
A1B2
A1B2C3
A1B2C3D4
```

Invalid examples:

```text
A
A1B
GG11
A1B2C3D4E5
```

An empty header is allowed only when there is exactly one header filter. It means:

```text
match every UDP payload on the common UDP port
```

If more than one header filter is configured, EMPTY is rejected because it would overlap every other header and duplicate rows.

Overlapping headers are also rejected. For example:

```text
A1
A1B2
```

These cannot be used together because a payload starting with `A1B2` also starts with `A1`.

This rule keeps CSV files cleanly partitioned.

Generated files:

```text
<base>_header_A1B2.csv
<base>_header_FF10.csv
<base>_header_EMPTY.csv
```

## Field Extraction Logic

Field extraction is unchanged.

Fields are still defined as:

```text
Field Name
Byte Offset
Length
Resolution
```

The byte offset is still zero-based from the UDP payload.

The extraction formula is still:

```text
Final value = unsigned big-endian raw decimal value × solved resolution expression
```

Example:

```text
Payload bytes at offset 15: 16 05
Raw value: 0x1605 = 5637
Resolution: 180/2^15 = 0.0054931640625
Final value: 5637 × 0.0054931640625 = 30.9649658203125
Displayed value: 30.964966
```

## CSV Output

CSV files do not have sheets or tabs, so partitioning is implemented as separate CSV files.

When the user chooses a base CSV path, the application automatically creates one CSV per filter.

All partition CSV files use the same columns:

```text
Packet No, Timestamp, Source IP, Destination IP, Source UDP Port, Destination UDP Port, Payload Size, custom fields...
```

The preview table includes one extra first column:

```text
Filter
```

This helps identify which filter partition the preview row belongs to.

## Files Changed

```text
PcapUdpExtractor.pro
headers/FilterTypes.h
headers/InputValidator.h
headers/MainWindow.h
headers/ui_MainWindow.h
sources/InputValidator_filters.cpp
sources/MainWindow.cpp
docs/MULTI_FILTER_UPGRADE.md
```

## Files Intentionally Preserved

The following core files are not rewritten by this upgrade:

```text
sources/PcapFileReader.cpp
sources/UdpPacketParser.cpp
sources/ExtractionEngine.cpp
sources/CsvExporter.cpp
sources/MathExpressionEvaluator.cpp
```

The core packet reading, UDP parsing, resolution expression, and extraction formula remain unchanged.

## Manual Test Checklist

### Port Filter Test

```text
1. Open the app.
2. Select a .pcap or .pcapng file.
3. Set Number of Message Filters = 2.
4. Select Port Filter.
5. Enter two different UDP ports.
6. Define at least one field.
7. Click Start Export.
8. Choose a base CSV name.
9. Confirm two CSV files are created.
10. Confirm each file contains only packets matching that port.
```

### Header Filter Test

```text
1. Open the app.
2. Select a .pcap or .pcapng file.
3. Set Number of Message Filters = 2.
4. Select Header Filter.
5. Enter the common UDP port.
6. Enter two non-overlapping headers, such as A1B2 and FF10.
7. Define at least one field.
8. Click Start Export.
9. Confirm two CSV files are created.
10. Confirm each file contains packets from the common port whose UDP payload starts with the matching header.
```

### Validation Tests

These must be rejected:

```text
Duplicate ports
Duplicate headers
A
A1B
GG11
A1B2C3D4E5
A1 and A1B2 together
EMPTY plus another header
```

### Regression Test

Use a known payload value:

```text
Hex bytes: 16 05
Resolution: 180/2^15
Expected: 30.964966
```

This verifies that the old field extraction and resolution logic still works.
