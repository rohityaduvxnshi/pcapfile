# Port + Length Message Filter Workflow

## Why the Global Field Table Changed

The old workflow used one global UDP payload field table for every selected port or header filter. That does not match real logs where one UDP port can carry multiple message types with different payload lengths and different field layouts.

Example:

```text
Port 5001
  Msg_A, payload length 20 bytes, fields for Msg_A
  Msg_B, payload length 30 bytes, fields for Msg_B
```

With one global field table, Msg_A fields could accidentally be applied to Msg_B packets. The new workflow makes fields belong to a message definition instead.

## New Hierarchy

```text
File
  -> Filter Type
      -> Port Filter
          -> Port Number
              -> Length Filter / Message Definition
                  -> Field Configuration
                      -> Bitfield Decoder
```

A message definition is identified by:

- message name
- UDP port number
- UDP payload length in bytes
- its own field list

The payload length is the UDP payload size only. It is not the Ethernet, IP, UDP, or captured frame length.

## UI Workflow

1. Select File Mode.
2. Import a `.pcap` or `.pcapng` file.
3. Select the Port filter type.
4. Enter the number of port filters.
5. Enter each UDP port in the port table.
6. Click `Manage Length Filters` for a port.
7. Add message definitions with `Message Name` and `Payload Length (bytes)`.
8. Configure fields from the message row or from the combined `Configured Messages` table.
9. Export.

The main configured-message table shows:

```text
Message Name | Payload Length | Port | Fields | Configure Fields
```

## Validation Rules

- Message name cannot be empty.
- Port must be 1 to 65535.
- Payload Length (bytes) must be greater than 0.
- Field names must be unique inside one message definition.
- Field byte offset must be greater than or equal to 0.
- Field length must be greater than 0.
- Field byte offset + field length must not exceed the message payload length.
- Resolution expressions are validated by the existing expression evaluator.
- A message definition must have at least one configured field before export.
- Duplicate message names are blocked under the same port.
- Duplicate payload lengths are blocked under the same port.
- Before export, every configured message definition must appear at least once in the imported capture.

## Export Naming

Port message exports create one CSV file per message definition.

Filename format:

```text
messageName_payloadLength_port_yyyyMMdd_HHmmss.csv
```

Example:

```text
Msg_A_20_5001_20260519_214522.csv
```

Message names are sanitized for filenames. Spaces become underscores and filename-breaking characters such as `\ / : * ? " < > |` are replaced.

## CSV Splitting

Each message definition gets its own CSV and only receives packets that match:

```text
(source UDP port == message port OR destination UDP port == message port)
AND
UDP payload size == Payload Length (bytes)
```

Each CSV uses only that message definition's fields. Bitfield decoder columns are expanded from the fields for that message only.

## Current Limitation

If two different messages share the same UDP port and the same UDP payload length, this implementation cannot distinguish them. For now, duplicate payload length under the same port is blocked.

## Future Extension

A future workflow can add a discriminator such as:

- header bytes
- message ID byte
- port + header + length
- port + length + message ID

That would allow two messages with the same port and payload length to have separate field definitions.
