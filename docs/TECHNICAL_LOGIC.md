# Technical Logic

## Architecture

The application is divided into small modules so that each file has one clear responsibility.

```text
MainWindow
  -> InputValidator
  -> PcapFileReader
  -> UdpPacketParser
  -> ExtractionEngine
  -> CsvExporter
```

## Packet Flow

```text
Capture file
  -> raw packet record
  -> Ethernet II frame
  -> IPv4 packet
  -> UDP datagram
  -> UDP payload
  -> custom field extraction
  -> preview table + CSV export
```

## PCAP Reader

`PcapFileReader` reads one packet at a time using `QFile`.

It supports:

- classic PCAP global header
- classic PCAP packet records
- PCAPNG section header blocks
- PCAPNG interface description blocks
- PCAPNG enhanced packet blocks
- PCAPNG simple packet blocks

It does not load the complete file into memory.

## Ethernet Logic

Ethernet II header size is 14 bytes.

Important bytes:

```text
Bytes 12-13 = EtherType
0x0800      = IPv4
```

Basic VLAN tags `0x8100` and `0x88A8` are also handled by moving the IPv4 offset forward by 4 bytes.

## IPv4 Logic

IPv4 starts after the Ethernet header.

Important fields:

```text
Version/IHL       = byte 0 of IPv4 header
Total Length      = bytes 2-3
Fragment Info     = bytes 6-7
Protocol          = byte 9
Source IP         = bytes 12-15
Destination IP    = bytes 16-19
```

Only protocol `17` is accepted because that means UDP.

Fragmented IPv4 packets are skipped because the UDP header or payload may not be complete in one packet.

## UDP Logic

UDP header size is 8 bytes.

Important fields:

```text
Source Port       = bytes 0-1
Destination Port  = bytes 2-3
UDP Length        = bytes 4-5
Checksum          = bytes 6-7
Payload           = starts after byte 7
```

The application processes a packet only if:

```text
source UDP port == user selected port
OR
destination UDP port == user selected port
```

## Custom Field Extraction

Fields are applied to the UDP payload only.

For each field:

```text
1. Check byte offset is valid.
2. Check length is between 1 and 8.
3. Check offset + length does not exceed payload size.
4. Read bytes as unsigned big-endian integer.
5. Multiply raw value by resolution.
6. Write final value to table and CSV.
```

If a field cannot be extracted, the result is `N/A`.

## CSV Safety

`CsvExporter` escapes:

- commas
- double quotes
- newlines

It also protects Excel from formula injection. If a cell starts with `=`, `+`, `-`, or `@`, the exporter prefixes it with an apostrophe.

## Memory Strategy

The application is designed for low-spec systems:

- packet-by-packet reading
- no full-file loading
- no packet list stored in memory
- CSV rows written immediately
- UI preview limited to 5000 rows

## Upgrade Path

Recommended future phases:

1. Move processing into `QThread`.
2. Add Stop button.
3. Add endian selector for UDP payload fields.
4. Add signed integer and float field types.
5. Add native XLSX export if an offline XLSX library is approved.
