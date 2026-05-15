# Changes and Features - Multi-Filter Workflow Upgrade

This document explains the latest workflow changes, new features, validation rules, output behavior, and manual testing process added to the PCAP UDP Extractor project.

## 1. Previous Workflow

Earlier, the application followed this simple workflow:

```text
Select PCAP/PCAPNG file
  -> enter one UDP port
  -> define payload fields
  -> export one CSV file
```

This worked for a single UDP port only. The output was one CSV file containing all packets matching that one port.

## 2. New Workflow

The application now supports multiple message filters in one run.

New workflow:

```text
Select PCAP/PCAPNG file
  -> enter number of message filters
  -> select one filter mode
       -> Port Filter
       -> Header Filter
  -> enter filter values
  -> define payload fields
  -> export partitioned CSV files
```

Only one filter mode can be active at a time.

## 3. New Feature: Number of Message Filters

A new input has been added:

```text
Number of Message Filters
```

This controls how many filter input boxes are shown.

Example:

```text
Number of Message Filters = 3
```

If Port Filter mode is selected, the UI shows 3 port input boxes.

If Header Filter mode is selected, the UI shows 3 header input boxes.

Allowed range:

```text
Minimum: 1
Maximum: 20
```

## 4. New Feature: Filter Mode Selection

Two filter modes are available:

```text
Port Filter
Header Filter
```

The user can select only one mode at a time.

Port Filter mode is the default mode.

## 5. Port Filter Mode

In Port Filter mode, the user enters one UDP port per filter box.

Example:

```text
Number of Message Filters = 3
Port Filter 1 = 4001
Port Filter 2 = 4002
Port Filter 3 = 5000
```

A UDP packet matches a port filter if:

```text
source UDP port == filter port
OR
destination UDP port == filter port
```

Each port filter creates a separate CSV output file.

Example output files:

```text
capture_20260515_143022_port_4001.csv
capture_20260515_143022_port_4002.csv
capture_20260515_143022_port_5000.csv
```

### Port Filter Validation Rules

```text
Port must be an integer.
Port must be between 0 and 65535.
Duplicate ports are not allowed.
```

If duplicate ports are entered, the application rejects the input before processing starts.

## 6. Header Filter Mode

In Header Filter mode, the user enters:

```text
one common UDP port
one header value per message filter
```

Example:

```text
Number of Message Filters = 2
Common UDP Port = 5000
Header Filter 1 = A1B2
Header Filter 2 = FF10
```

The packet must first match the common UDP port:

```text
source UDP port == common UDP port
OR
destination UDP port == common UDP port
```

After that, the application checks whether the UDP payload starts with the configured header bytes.

A header match means:

```text
UDP payload starts with the header bytes entered by the user
```

Example:

```text
UDP Payload: A1 B2 10 20 30
Header:      A1 B2
Result:      Match
```

Each header filter creates a separate CSV output file.

Example output files:

```text
capture_20260515_143022_header_A1B2.csv
capture_20260515_143022_header_FF10.csv
```

## 7. Header Input Rules

Header values are entered in hex format.

Allowed header length:

```text
0 to 4 bytes
```

Allowed text lengths:

```text
empty      = 0 bytes
2 hex chars = 1 byte
4 hex chars = 2 bytes
6 hex chars = 3 bytes
8 hex chars = 4 bytes
```

Valid examples:

```text
A1
A1B2
a1b2
A1B2C3
A1B2C3D4
```

Header input is case-insensitive.

The application normalizes accepted header labels to uppercase.

Invalid examples:

```text
A
A1B
GG11
A1B2C3D4E5
```

Reasons:

```text
A           -> odd number of hex characters
A1B         -> odd number of hex characters
GG11        -> G is not a valid hex character
A1B2C3D4E5  -> more than 4 bytes
```

## 8. Empty Header Rule

An empty header is allowed.

It means:

```text
match all UDP payloads on the selected common UDP port
```

The output file label for an empty header is:

```text
EMPTY
```

Example output file:

```text
capture_20260515_143022_header_EMPTY.csv
```

Important rule:

```text
EMPTY header can only be used when there is exactly one header filter.
```

If EMPTY is used with another header, the application rejects it because EMPTY would match every packet and cause duplicate/unclean partitioning.

## 9. Overlapping Header Protection

Overlapping headers are not allowed.

Example of invalid combination:

```text
Header 1 = A1
Header 2 = A1B2
```

Reason:

```text
A payload starting with A1B2 also starts with A1.
```

This would cause duplicate or unclear partitioning, so the application rejects it.

The CSV output should remain cleanly partitioned.

## 10. CSV Partitioning Behavior

CSV files do not support sheets or tabs.

Therefore, partitioning is implemented by creating multiple CSV files.

The user chooses one base CSV name, and the application creates one CSV file per filter.

Example base name:

```text
capture_20260515_143022.csv
```

Port Filter output:

```text
capture_20260515_143022_port_4001.csv
capture_20260515_143022_port_4002.csv
```

Header Filter output:

```text
capture_20260515_143022_header_A1B2.csv
capture_20260515_143022_header_FF10.csv
```

## 11. CSV Columns

Each generated CSV file has the same output columns.

Fixed columns:

```text
Packet No
Timestamp
Source IP
Destination IP
Source UDP Port
Destination UDP Port
Payload Size
```

After these fixed columns, user-defined fields are added.

Example:

```text
Packet No, Timestamp, Source IP, Destination IP, Source UDP Port, Destination UDP Port, Payload Size, Heading, Speed, Range
```

## 12. Output Preview Change

The preview table now includes one extra column at the beginning:

```text
Filter
```

This shows which filter partition the preview row belongs to.

Example:

```text
Filter | Packet No | Timestamp | Source IP | Destination IP | ...
4001   | 12        | ...       | ...       | ...            | ...
A1B2   | 18        | ...       | ...       | ...            | ...
```

The CSV files themselves do not need this column because each CSV filename already identifies the filter partition.

## 13. Field Extraction Logic Is Unchanged

The existing payload field extraction logic remains unchanged.

Fields are still defined using:

```text
Field Name
Byte Offset
Length
Resolution
```

The byte offset is still:

```text
zero-based from the start of the UDP payload
```

It is not counted from:

```text
PCAP record start
Ethernet frame start
IPv4 header start
UDP header start
```

## 14. Resolution Logic Is Unchanged

Resolution expressions still work as before.

Supported examples:

```text
1
0.1
180/2^15
360/2^16
1e-3
```

Formula remains:

```text
Final value = unsigned big-endian raw decimal value × solved resolution expression
```

Example:

```text
Hex bytes: 16 05
Raw decimal: 0x1605 = 5637
Resolution: 180/2^15 = 0.0054931640625
Final value: 5637 × 0.0054931640625 = 30.9649658203125
Displayed value: 30.964966
```

## 15. Files Changed

The following files were changed or added for this upgrade:

```text
PcapUdpExtractor.pro
headers/FilterTypes.h
headers/InputValidator.h
headers/MainWindow.h
headers/ui_MainWindow.h
sources/InputValidator_filters.cpp
sources/MainWindow.cpp
docs/MULTI_FILTER_UPGRADE.md
docs/CHANGES_AND_FEATURES_MULTI_FILTER.md
```

## 16. Files Intentionally Not Rewritten

The core parsing and extraction files were not rewritten:

```text
sources/PcapFileReader.cpp
sources/UdpPacketParser.cpp
sources/ExtractionEngine.cpp
sources/CsvExporter.cpp
sources/MathExpressionEvaluator.cpp
```

This was done deliberately to avoid changing proven packet-reading, UDP-parsing, CSV-writing, and resolution-calculation behavior.

## 17. Why Manual UI Header Was Preserved

The project already used a hand-written UI header:

```text
headers/ui_MainWindow.h
```

The upgrade keeps this structure.

It does not switch the project to a generated `.ui` form workflow.

Reason:

```text
avoids uic/generated-header conflicts
keeps current qmake project structure stable
reduces compile risk
```

## 18. Manual Build Steps

From the project root:

```bash
qmake PcapUdpExtractor.pro
mingw32-make
```

Or open `PcapUdpExtractor.pro` in Qt Creator and build normally.

## 19. Manual Test Checklist

### Test 1: Port Filter Export

```text
1. Open application.
2. Select a .pcap or .pcapng file.
3. Set Number of Message Filters = 2.
4. Select Port Filter.
5. Enter two different UDP ports.
6. Define at least one field.
7. Click Start Export.
8. Choose base CSV name.
9. Confirm two CSV files are created.
10. Confirm each CSV contains packets matching the corresponding port.
```

### Test 2: Header Filter Export

```text
1. Open application.
2. Select a .pcap or .pcapng file.
3. Set Number of Message Filters = 2.
4. Select Header Filter.
5. Enter common UDP port.
6. Enter two non-overlapping headers, for example A1B2 and FF10.
7. Define at least one field.
8. Click Start Export.
9. Confirm two CSV files are created.
10. Confirm each CSV contains packets from the common port and matching header.
```

### Test 3: Invalid Port Inputs

These should be rejected:

```text
duplicate ports
ports below 0
ports above 65535
```

### Test 4: Invalid Header Inputs

These should be rejected:

```text
A
A1B
GG11
A1B2C3D4E5
A1 and A1B2 together
EMPTY plus another header
```

### Test 5: Regression Test for Resolution

Use known bytes:

```text
Hex bytes: 16 05
Resolution: 180/2^15
Expected output: 30.964966
```

This verifies that the original extraction and resolution logic still works.

## 20. Current Scope

Supported now:

```text
multiple port filters
multiple header filters
one common port for header mode
separate CSV output per filter
case-insensitive header input
0 to 4 byte header matching
filter validation
preview filter column
old field extraction retained
old resolution logic retained
```

Not included in this upgrade:

```text
TCP parsing
IPv6 parsing
live capture
native XLSX sheets
bit-level field extraction
little-endian field extraction
signed/float field types
```

These can be added later as separate upgrades.
