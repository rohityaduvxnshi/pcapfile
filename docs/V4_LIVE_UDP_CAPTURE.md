# V4 — Live UDP Capture and Recording

## 1. Purpose

V4 adds a second input mode to PcapUdpExtractor.

```
File Mode  -> existing V1/V2/V3 behavior. Reads .pcap / .pcapng files. Unchanged.
Live Mode  -> binds a UDP socket to one port, receives live UDP payloads,
              applies the same field extraction and bitfield decoding,
              and streams matched rows into a CSV file while running.
```

Both modes share the same field table, filters, and Bitfield Decoder
configuration. V4 only adds a new data **source** (a UDP socket) and a new
streaming **writer**.

## 2. File mode vs live mode — the key difference

In file mode:

```
PcapFileReader  -> raw packet records
UdpPacketParser -> strips Ethernet + IPv4 + UDP headers -> UDP payload
ExtractionEngine + BitfieldDecoder -> work on the UDP payload
```

In live mode:

```
QUdpSocket::readDatagram() -> UDP payload DIRECTLY
ExtractionEngine + BitfieldDecoder -> work on the UDP payload
```

The operating system already removes the Ethernet, IPv4, and UDP headers
before `readDatagram()` returns. What you receive is the UDP payload only.

## 3. Why UdpPacketParser is NOT used in live mode

`UdpPacketParser` exists to strip Ethernet/IPv4/UDP headers from raw frames
read out of a `.pcap` file. A live datagram from `QUdpSocket` has **already
been stripped by the OS**. Passing a live datagram into `UdpPacketParser`
would make it parse payload bytes as if they were Ethernet/IP headers and
produce garbage or fail.

Because both paths end at the same thing — a UDP payload byte buffer —
`ExtractionEngine` and `BitfieldDecoder` are reused unchanged. Field byte
offsets stay payload-relative in both modes. No offset reinterpretation.

## 4. Single bind port limitation (V4)

A `QUdpSocket` binds to exactly one port. V4 supports **one** live capture
port at a time. Multi-port live capture (one socket per port, like file-mode
multi-port filtering) is intentionally out of scope for V4.

The bound port is the destination port. The sender's source port is read from
`readDatagram()` and stored in the CSV `SourcePort` column, but it is not
treated as a filter input.

## 5. Header filter behavior in live mode

The header filter works the same as file mode: it compares the first 0–4
bytes of the UDP payload against configured hex prefixes.

```
- Hex prefixes must be even-length (0, 2, 4, 6, or 8 hex characters).
- Invalid / odd-length / non-hex header config is rejected before Start.
- If the payload is shorter than the prefix, it does not match.
```

Port-filter mode in live capture collapses to "bind to that one port",
because the destination port is fixed by the socket bind.

## 6. CSV output columns

Live mode writes one CSV file per capture session:

```
liveCapture_yyyyMMdd_HHmmss.csv
```

Column order:

```
TimestampUtc, SourceIP, SourcePort, <field columns identical to file mode>
```

`<field columns>` includes Bitfield Decoder expansion columns, exactly as in
file mode. Example with a Msg2 bitfield field:

```
TimestampUtc, SourceIP, SourcePort, Msg2, Msg2_BITE, Msg2_MODE, Msg2_AHR_VALIDITY, Msg2_SPARE
```

Timestamp format: ISO-8601 UTC with milliseconds
(`QDateTime::toUTC().toString(Qt::ISODateWithMs)`).

CSV escaping and formula-injection protection are the same rules used in file
mode (quote-wrap on comma/quote/newline, double internal quotes, prefix a
single quote when a value starts with `=`, `+`, `-`, or `@`). The live writer
opens the file in binary write mode and writes `\r\n` manually. Do not use
`QIODevice::Text` with manual CRLF because Windows text translation can produce
bad line endings.

## 7. Short packet handling

A live datagram may be shorter than a configured field needs
(`offset + length > payload.size()`). This is checked per field, per packet.
When it happens:

```
- The affected field cell is written as SHORT_PACKET.
- The Short Packets counter increments.
- The application does NOT crash and does NOT write garbage.
```

## 8. UDP packet loss limitation

UDP is lossy by design. If datagrams arrive faster than the application
processes them, the OS socket receive buffer fills and the OS **silently
drops datagrams**. There is no exception and no error for this.

V4 reduces the risk by:

```
- Requesting a 1 MB socket receive buffer.
- Draining all pending datagrams on each readyRead.
- Keeping per-datagram work fast (no table repaint inside the datagram slot).
- Refreshing the preview table on a 250-500 ms timer, not per packet.
```

V4 cannot guarantee zero packet loss. The goal is to minimize loss, surface
every detectable error, and never crash or write corrupt rows.

## 9. Testing with the Python UDP sender

A test sender is provided at `tools/udp_test_sender.py`.

```
1. Build and run PcapUdpExtractor.
2. Select Live Mode.
3. Set the live port (example: 5005).
4. Define at least one field (offsets are payload-relative).
5. Optionally configure a header filter (example: AA55).
6. Click Start Live Capture.
7. In a terminal, run:  python3 tools/udp_test_sender.py 127.0.0.1 5005
8. Confirm: packet counter increases, preview rows appear,
   liveCapture_*.csv is created and grows.
9. Click Stop Live Capture and confirm the file path and row count.
```

The sender includes one deliberately short payload (`AA55`) to exercise the
SHORT_PACKET path.

## 10. Regression checklist

V4 must not break existing behavior. After implementation, verify:

```
File mode:
 1. .pcap load still works.
 2. .pcapng load still works.
 3. UdpPacketParser (Ethernet/IPv4/UDP) behavior unchanged.
 4. Resolution expressions like 180/2^15 still work.
 5. Export file naming uploadedFileName_yyyyMMdd_HHmmss.csv unchanged.
 6. File-mode port filter still works.
 7. File-mode header filter still works.
 8. CsvExporter batch output unchanged.
 8a. Live CSV rows have exactly the same field-column count as the header;
     mismatch must stop capture with a clear error instead of writing malformed CSV.
 9. Bitfield Decoder dialog opens and saves config.
10. Bitfield single-bit decode works.
11. Bitfield grouped-bit decode works.
12. Reserved/spare and unknown-value behavior unchanged.

Live mode:
13. Binds to a valid port.
14. Packet counter increments on incoming traffic.
15. Receives UDP payload directly (no UdpPacketParser).
16. Header filter applied to payload start.
17. CSV has TimestampUtc, SourceIP, SourcePort columns.
18. Extracted field values written correctly.
19. Short datagram handled without crash (SHORT_PACKET).
20. Stop button flushes and closes the CSV.
21. Closing the app during capture flushes and closes the CSV.

Build:
22. qmake re-run and full rebuild succeeds.
```

## 11. Known build-sensitive points

```
- QT += network must be added to the .pro file (required for QUdpSocket).
- After adding a Qt module, qmake must be re-run before rebuilding.
- The socket error signal differs by Qt version: errorOccurred (Qt 5.15+/Qt6)
  vs the older error() overload. LiveUdpReceiver.cpp handles both with a
  QT_VERSION guard.
- Ports below 1024 may require admin/root privileges to bind. V4 warns but
  still allows the attempt.
```
