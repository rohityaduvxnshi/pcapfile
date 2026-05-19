# Port Length Message Filter Implementation Summary

Date: 2026-05-19
Project: PcapUdpExtractor

## What Was Changed

The application was updated from a single global UDP payload field table to a message-definition workflow for Port Filter mode.

Old model:

```text
Port/Header filters
  -> one shared global field table
```

New Port Filter model:

```text
Port
  -> Message Definition / Length Filter
      -> Field Configuration
          -> Bitfield Decoder
```

Each message definition now owns its own field list, so different messages on the same UDP port can use different payload layouts.

## New Data Model

Added `MessageDefinition`:

```cpp
struct MessageDefinition
{
    QString messageName;
    quint16 port;
    int payloadLengthBytes;
    QList<FieldDefinition> fields;
};
```

Also added `resolutionExpression` to `FieldDefinition` so expressions such as `180/2^15`, `1/256`, and `360/2^16` remain available when editing fields later.

## UI Changes

Updated `forms/MainWindow.ui`:

- Removed the old global field definition table from the main window.
- Added a Port Filter table:
  - `Port`
  - `Manage Length Filters`
  - `Message Count`
- Added a combined `Configured Messages` table:
  - `Message Name`
  - `Payload Length`
  - `Port`
  - `Fields`
  - `Configure Fields`
- Added field configuration buttons for Header mode and Live mode so those paths can keep working without a main-window global field table.

Added new Qt Designer dialogs:

- `MessageLengthFilterDialog`
- `MessageDefinitionDialog`
- `FieldConfigurationDialog`

## New Dialogs

### MessageLengthFilterDialog

Manages all message definitions for one selected UDP port.

It supports:

- Add Length Filter
- Edit Selected Filter
- Remove Selected Filter
- Configure Fields
- Save
- Cancel

It blocks duplicate message names and duplicate payload lengths under the same port.

### MessageDefinitionDialog

Adds or edits one message definition.

Fields:

- Message Name
- Payload Length (bytes)

Validation:

- Message name cannot be empty.
- Payload length must be greater than 0.

### FieldConfigurationDialog

Contains the old field table behavior, now scoped to one message definition.

It supports:

- Add Field
- Edit Field
- Remove Field
- Bitfield Decoder
- Save
- Cancel

Field validation includes:

- Field name cannot be empty.
- Field names must be unique inside one message.
- Byte offset must be greater than or equal to 0.
- Field length must be greater than 0.
- Field offset plus field length must not exceed the message payload length.
- Resolution expression must be valid.
- Existing BitfieldDecoderDialog is reused.

## Filtering Logic

Port mode now matches a UDP packet to a message definition only when:

```text
(source UDP port == message port OR destination UDP port == message port)
AND
UDP payload size == Payload Length (bytes)
```

The implementation uses UDP payload size only. It does not use total Ethernet/IP/UDP packet length.

Header filter mode was kept on the existing behavior path.

## Export Logic

Port mode now exports one CSV file per message definition.

Filename format:

```text
messageName_payloadLength_port_yyyyMMdd_HHmmss.csv
```

Example:

```text
Msg_A_20_5001_20260519_214522.csv
```

Each CSV uses only that message definition's fields. Different messages do not share columns.

Bitfield decoder output columns are still generated from the field's configured decoder rules.

## Pre-Export Validation

Before exporting in Port Filter mode, the app scans the imported capture and verifies that every configured message appears at least once.

If no packet is found, export is blocked with:

```text
No packet found for message '<name>' with port <port> and payload length <length> bytes.
```

## Files Added

- `headers/MessageDefinition.h`
- `sources/MessageDefinition.cpp`
- `headers/MessageLengthFilterDialog.h`
- `sources/MessageLengthFilterDialog.cpp`
- `forms/MessageLengthFilterDialog.ui`
- `headers/MessageDefinitionDialog.h`
- `sources/MessageDefinitionDialog.cpp`
- `forms/MessageDefinitionDialog.ui`
- `headers/FieldConfigurationDialog.h`
- `sources/FieldConfigurationDialog.cpp`
- `forms/FieldConfigurationDialog.ui`
- `docs/PORT_LENGTH_MESSAGE_FILTER_WORKFLOW.md`
- `docs/PORT_LENGTH_MESSAGE_FILTER_IMPLEMENTATION_SUMMARY.md`

## Files Updated

- `PcapUdpExtractor.pro`
- `README.md`
- `forms/MainWindow.ui`
- `headers/AppTypes.h`
- `headers/MainWindow.h`
- `sources/MainWindow.cpp`
- `sources/BitfieldDecoder.cpp`
- `sources/InputValidator_filters.cpp`

## qmake Changes

The project file was updated to include all new headers, sources, and forms.

The C++ standard selection was made conditional:

```qmake
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}
```

This keeps Qt 5 on C++11 while allowing Qt 6.11 to build, because Qt 6.11 requires C++17.

## Compatibility Fixes

Two Qt 6 compatibility fixes were made:

- Replaced `QRegularExpression::exactMatch()` with `match(...).hasMatch()`.
- Used Qt 6 split-behavior enums for `QString::split()` when building with Qt 6.

## Build Check

qmake was run successfully.

The project built successfully with the Qt 6.11 MinGW kit:

```text
build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug/debug/PcapUdpExtractor.exe
```

## Known Limitation

If two messages share the same UDP port and the same UDP payload length, this implementation cannot distinguish them.

For now, duplicate payload length under the same port is blocked.

Future support can add a discriminator such as:

- header bytes
- message ID byte
- port + length + header
- port + length + message ID

## Manual Test Checklist

- Add ports `5001` and `6001`.
- Under port `5001`, add `Msg_A` length `20` and `Msg_B` length `30`.
- Under port `6001`, add `Msg_C` length `40`.
- Confirm the combined table shows all three messages.
- Configure fields for `Msg_A`.
- Configure different fields for `Msg_B`.
- Confirm fields are not shared between messages.
- Add a bitfield decoder to one field in `Msg_B`.
- Confirm duplicate payload length under the same port is blocked.
- Confirm export blocks messages with no fields.
- Confirm field offset `19` and length `2` is blocked for payload length `20`.
- Confirm export blocks a message whose port and payload length do not appear in the capture.
- Confirm valid export creates one CSV per message definition.
- Confirm Header Filter mode still exports.
