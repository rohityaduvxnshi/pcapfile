# Serial Mode

A third input mode next to **File** and **Live** (Ctrl+3). It reads data from a
serial COM port — or replays a **text-file dump** of one — and runs the exact
same machinery as the other modes: message definitions ("length filters"),
field configuration, NMEA sentences, bit/conditional decoders, Compare Options
verification, **ICD `.docx` import**, project save/restore, and **Excel
output** (one `.xlsx` per message).

## Data framing — one line = one record

A serial link is a byte stream with no packet boundaries, so Serial Mode frames
records by **newline** (`\n`, `\r` stripped; implemented in
[`SerialPortReceiver`](../headers/SerialPortReceiver.h)). Each non-empty line is
converted to a payload (`serialLineToPayload` in
[`MainWindow.cpp`](../sources/MainWindow.cpp)):

| Line looks like | Treated as |
|---|---|
| `$GPGGA,123519,4807.038,...*47` (starts `$` or `!`) | ASCII NMEA sentence — matched by sentence formatter, decoded by `NmeaDecoder` |
| `AA 55 01 02 03FF` (hex pairs; spaces/commas/dashes/colons allowed) | Binary payload — matched like a UDP payload (exact length + optional header bytes) |
| anything else | Raw ASCII bytes |

A binary stream that never contains `\n` overflows a 1 MB guard buffer and
stops with a clear error — Serial Mode expects newline-terminated records.

## Message matching

Serial links have no ports, so the UDP port is **ignored**; everything else is
identical to Live mode:

* **HEX messages** match on exact payload length, plus the optional header
  bytes when configured (lets two same-length messages coexist).
* **NMEA messages** match on the 3-char sentence formatter.

`Manage Length Filters` opens the same dialog as Live mode (a placeholder port
of 1 is stamped in; it plays no role). **Import ICD (Ctrl+I) while Serial Mode
is active** routes the imported messages into the serial list.

## Live capture

1. Pick the **Port** (Refresh re-scans; you can also type a name), **Baud**,
   data bits / parity / stop bits (8-N-1 is the default).
2. Define at least one message in *Manage Length Filters*.
3. **Start Serial Capture** (F5) → choose an output folder. One
   `serialCapture_<message>_<timestamp>.xlsx` is opened per message.
4. Watch lines received / matched / rows written and the rolling preview.
5. **Stop Serial Capture** (Shift+F5) — the workbooks are **saved on stop**
   (see docs/EXCEL_EXPORT.md for why .xlsx cannot be appended row-by-row).

## Text-file replay ("file mode for serial")

`Or replay a text file` → Browse to a `.txt`/`.log`/`.nmea` dump (one record
per line, same formats as above) → **Process File**. The whole file is read,
matched, and exported per message — the serial equivalent of File-mode's Start
Export, including a final summary with per-file row counts.

## Persistence

Serial messages round-trip through the project sidecar (`.pcproj.json`) under
the new `serial.messages` key; `inputMode` gains the value `"serial"`. Old
project files load unchanged (the key is simply absent).

## Build requirement

`PcapUdpExtractor.pro` now has `QT += serialport`. QtSerialPort ships with
every Qt 5.10 kit (mingw and msvc) — no extra install.
