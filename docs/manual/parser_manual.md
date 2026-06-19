# Universal Wireshark Log Reader — User Manual

Universal Wireshark Log Reader is an offline desktop tool that **reads and decodes** UDP and TCP
data — from a captured **`.pcap` / `.pcapng`** file or a **live UDP / TCP** stream — into engineering
values, and exports the result to **CSV** or **Excel**. It is the receiver/parser companion to
*Universal Data Simulator* (the sender).

> Tip: press **F1** any time to open this manual, and use the search box at the top to jump straight
> to a **Common function** or a **Troubleshooting** entry.

---

## Using this manual

This manual is built into the app (**Help → User Manual**, or **F1**). The list on the left is a
clickable table of contents; the box at the top searches the whole manual — type a word like
`troubleshooting`, `live mode` or `bitfield` and press **Enter** (or **Next**/**Prev**) to step
through every match. The same content is also provided as `parser_manual.docx`.

![The searchable in-app manual viewer](parser/help-viewer.png)

---

## Overview

![Universal Wireshark Log Reader — main window (Modern Light theme)](parser/main-light.png)

The window flows top to bottom:

1. **Input Mode** — *File Mode* (a saved capture) or *Live Mode* (listen to UDP/TCP now). Plus **Import ICD…**.
2. **Input** — choose the capture file (File Mode) or the listen/connect settings (Live Mode).
3. **Message Filters** — by **Port** and/or **Header**, with per-message **Length filters**.
4. **Configured Messages** — the message/field definitions that drive decoding.
5. **Start** — parse/decode and write the output.
6. **Output Preview** — a preview of the decoded rows.

The same window in the **Slate Dark** theme (toggle with **Ctrl+T**):

![Main window (Slate Dark theme)](parser/main-dark.png)

---

## Getting started

1. Choose **File Mode** (Ctrl+1) and **Browse** to a `.pcap`/`.pcapng` file — or **Live Mode**
   (Ctrl+2), pick **UDP** or **TCP** transport, and set the listen port (or TCP host/port).
2. Set a **Message Filter** (e.g. Port 5000) so only the traffic you care about is decoded.
3. Define one or more **messages** and their **fields** (name, byte offset, type, length, resolution),
   or **Import ICD…** to generate them from a Word document.
4. Press **Start** (F5). The **Output Preview** fills in and a CSV/Excel file is written.

---

## The main window, panel by panel

### Input Mode
- **File Mode** (Ctrl+1) — decode a saved `.pcap`/`.pcapng` capture.
- **Live Mode** (Ctrl+2) — receive data live over the network and decode it as it arrives, writing
  per-message CSV files to a chosen folder.
- **Import ICD…** — build message/field definitions from a Word `.docx` ICD.

### Input
*File Mode:* **Browse** to the capture file. *Live Mode:* pick a **Transport** (UDP or TCP), set the
listen port, and optionally configure TCP-specific settings (role: Server/Client; host; frame length)
before **Start Live**.

### Message Filters
Select **Port** or **Header** filtering and set the number of filters. Each filter row has a port and a
**Manage Length Filters** button (route messages by payload length) with a live message count. Header
filtering lets you tell apart same-length messages by a leading signature.

### Configured Messages
The list of messages to decode. Each has a name, payload length, port and fields. **Configure Fields**
opens the field editor (name, 1-based byte offset, data type, length, resolution / resolution
expression). An **Offsets in** selector lets you switch between **Bytes** and **Words** (1 word = 2
bytes) for display; field offsets are always stored in bytes internally. Fields can carry **bitfield
decoders** and **conditional bitfield decoders** that expand a byte/word into named bit meanings. Tick
**Verify all configured messages before export** to validate everything first.

There is no length cap on numeric fields — fields wider than 8 bytes are decoded to exact decimal
values (or resolution-scaled doubles).

### Start & Output
**Start** (F5) parses the input, applies the filters, decodes each message's fields and writes the
output (CSV, or Excel via the export options). The **Output Preview** shows a sample of the decoded
rows. In Live Mode, **Stop** (Shift+F5) ends the capture.

---

## Common functions

### Decode a capture file to CSV
1. **File Mode** (Ctrl+1) → **Browse** to the `.pcap`/`.pcapng`.
2. Add a **Port** filter (e.g. 5000) and define the message + fields (or **Import ICD…**).
3. **Start** (F5). The decoded rows appear in the preview and a CSV is written.

### Decode a live UDP stream
1. **Live Mode** (Ctrl+2) → transport **UDP** → set the listen **port**.
2. Define messages/fields and (optionally) per-message **length filters**.
3. **Start Live**, pick an output folder; one CSV per message is written as datagrams arrive. **Stop** when done.

### Decode a live TCP stream
1. **Live Mode** (Ctrl+2) → transport **TCP**.
2. Choose a **Role**: *Server* (listen for incoming connections) or *Client* (connect to a remote host).
   Set the port (and host for Client mode). Set the **Frame length** so the reader knows where one
   message ends and the next begins.
3. Define messages/fields → **Start Live**. TCP data is framed and decoded the same way as UDP.

### Import message/field definitions from a Word ICD
**Import ICD…** → pick the `.docx` → tick the tables that hold field definitions → **Build / Preview**
→ review (the column mapping and offset base are auto-detected and editable) → **OK**. The drafted
messages appear in *Configured Messages*.

### Add a bitfield decoder to a field
In **Configure Fields**, open the field's decoder and define bit ranges → names/meanings. On export,
the field expands into readable per-bit columns. **Conditional** decoders switch the bit meanings based
on a controller field's value.

### Export to Excel
Use the export options to write `.xlsx` instead of CSV (one sheet per message). See the Excel export
notes for carrying the bundled library to another PC.

### Import/export fields as Excel
In **Configure Fields**, use the **Excel** menu → **Import** to load fields from a `.xlsx` file (same
column layout as CSV: Name, ByteOffset, DataType, Length, Resolution, etc.), or **Export** to write the
current fields to Excel. Bit decoders are not carried in the spreadsheet — use JSON for those.

### Import/export fields as JSON (lossless)
In **Configure Fields**, use the **JSON** menu → **Import** / **Export**. The JSON format carries the
full field definition including bitfield decoders, conditional decoders, and resolution expressions.

### Import/export whole messages as JSON
**File → Export Messages (JSON)** writes all configured messages (both modes) — fields, compare
options, payload length, port — to a single JSON file. **File → Import Messages (JSON)** loads them
back, appending to the current list (clashing names are auto-renamed). The format is shared with the
simulator, so you can round-trip definitions between the two apps without data loss.

### Switch offset display to Words
In **Configure Fields**, change the **Offsets in** dropdown from **Bytes** to **Words (2 bytes)**.
The offset column will display and accept word-based offsets (1 word = 2 bytes). The underlying byte
offsets are unchanged; this is purely a display convenience.

### Use the simulator as a test source
Run *Universal Data Simulator*, point it at this app's **Live Mode** port (UDP or TCP), and stream a
message — a quick way to validate your field definitions end to end.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| "Please select a PCAP or PCAPNG file" | No/invalid capture chosen in File Mode | **Browse** to a real `.pcap`/`.pcapng` file. |
| No rows decoded | Filters exclude everything, or the wrong port | Check the **Port**/length filters match the traffic; widen or remove a filter to confirm. |
| Live Mode shows 0 messages (UDP) | Nothing arriving on that port, or firewall | Confirm a sender is transmitting to this PC's IP and the listen port; allow the app through the firewall. |
| Live Mode TCP won't connect | Wrong role, host, or port | Server mode listens; Client mode connects. Confirm the remote end is running and the host/port are correct. |
| Values look wrong / scaled oddly | Resolution or data type mismatch | Re-check each field's **Type**, **Length** and **Resolution**; confirm the byte offset (1-based). |
| Fields shifted by a byte | Wrong byte offset base or payload length | Byte Offset is **1-based**; verify the message **Payload Length** and each field's offset. |
| ICD import offsets look shifted by one | Offset base (0- vs 1-based) mis-detected | In the import's **Table Settings**, flip the **Offset base**; review offsets are editable. |
| Export fails / no file written | Output path not writable, or verify failed | Pick a writable folder; fix any issues from **Verify all configured messages**. |
| Two messages of equal length collide | Same length on the same port | Add a **Header** filter / optional-header signature to tell them apart. |
| App opened with mixed light/dark panels | One-time startup repaint (now auto-fixed) | If seen on older builds, toggle the theme (Ctrl+T) once; current builds re-apply on launch. |

---

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| F1 | Open this user manual |
| Shift+F1 | Keyboard shortcuts box |
| Ctrl+1 / Ctrl+2 | File Mode / Live Mode |
| F5 / Shift+F5 | Start / Stop |
| Ctrl+T | Toggle Light/Dark theme |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / Save / Save-As project |
| Ctrl+I | Import ICD (.docx) |

---

## Glossary

- **PCAP / PCAPNG** — packet-capture file formats (e.g. from Wireshark/tcpdump).
- **UDP payload** — the application bytes of a UDP datagram, after the IP/UDP headers.
- **TCP frame** — a fixed-length chunk of a TCP byte stream; set the frame length to match one message.
- **Message filter** — a rule (by port, header signature and/or payload length) selecting which
  datagrams map to which message definition.
- **Byte offset** — 1-based position of a field's first byte within the payload.
- **Word offset** — 1-based position in 2-byte words; displayed when **Offsets in** is set to Words.
- **Resolution** — scale factor; the decoded engineering value is `raw × resolution`.
- **Bitfield decoder** — expands a numeric field into named per-bit meanings.
- **ICD** — Interface Control Document; here, a Word `.docx` describing message/field layouts.
