# Universal Wireshark Log Reader — User Manual

Universal Wireshark Log Reader is an offline desktop tool that **reads and decodes** UDP data — from
a captured **`.pcap` / `.pcapng`** file or a **live UDP** stream — into engineering values, and
exports the result to **CSV** or **Excel**. It is the receiver/parser companion to *Universal Data
Simulator* (the sender).

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

1. **Input Mode** — *File Mode* (a saved capture) or *Live Mode* (listen to UDP now). Plus **Import ICD…**.
2. **Input** — choose the capture file (File Mode) or the listen settings (Live Mode).
3. **Message Filters** — by **Port** and/or **Header**, with per-message **Length filters**.
4. **Configured Messages** — the message/field definitions that drive decoding.
5. **Start** — parse/decode and write the output.
6. **Output Preview** — a preview of the decoded rows.

The same window in the **Slate Dark** theme (toggle with **Ctrl+T**):

![Main window (Slate Dark theme)](parser/main-dark.png)

---

## Getting started

1. Choose **File Mode** (Ctrl+1) and **Browse** to a `.pcap`/`.pcapng` file — or **Live Mode**
   (Ctrl+2) and set the listen port.
2. Set a **Message Filter** (e.g. Port 5000) so only the traffic you care about is decoded.
3. Define one or more **messages** and their **fields** (name, byte offset, type, length, resolution),
   or **Import ICD…** to generate them from a Word document.
4. Press **Start** (F5). The **Output Preview** fills in and a CSV/Excel file is written.

---

## The main window, panel by panel

### Input Mode
- **File Mode** (Ctrl+1) — decode a saved `.pcap`/`.pcapng` capture.
- **Live Mode** (Ctrl+2) — bind a UDP port and decode datagrams as they arrive, writing per-message
  CSV files to a chosen folder.
- **Import ICD…** — build message/field definitions from a Word `.docx` ICD.

### Input
*File Mode:* **Browse** to the capture file. *Live Mode:* set the listen port (and per-message length
filters) before **Start Live**.

### Message Filters
Select **Port** or **Header** filtering and set the number of filters. Each filter row has a port and a
**Manage Length Filters** button (route messages by payload length) with a live message count. Header
filtering lets you tell apart same-length messages by a leading signature.

### Configured Messages
The list of messages to decode. Each has a name, payload length, port and fields. **Configure Fields**
opens the field editor (name, 1-based byte offset, data type, length, resolution / resolution
expression). Fields can carry **bitfield decoders** and **conditional bitfield decoders** that expand a
byte/word into named bit meanings. Tick **Verify all configured messages before export** to validate
everything first.

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
1. **Live Mode** (Ctrl+2) → set the listen **port**.
2. Define messages/fields and (optionally) per-message **length filters**.
3. **Start Live**, pick an output folder; one CSV per message is written as datagrams arrive. **Stop** when done.

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

### Use the simulator as a test source
Run *Universal Data Simulator*, point it at this app's **Live Mode** port, and stream a message — a
quick way to validate your field definitions end to end.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| "Please select a PCAP or PCAPNG file" | No/invalid capture chosen in File Mode | **Browse** to a real `.pcap`/`.pcapng` file. |
| No rows decoded | Filters exclude everything, or the wrong port | Check the **Port**/length filters match the traffic; widen or remove a filter to confirm. |
| Live Mode shows 0 messages | Nothing arriving on that port, or firewall | Confirm a sender is transmitting to this PC's IP and the listen port; allow the app through the firewall. |
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
- **Message filter** — a rule (by port, header signature and/or payload length) selecting which
  datagrams map to which message definition.
- **Byte offset** — 1-based position of a field's first byte within the payload.
- **Resolution** — scale factor; the decoded engineering value is `raw × resolution`.
- **Bitfield decoder** — expands a numeric field into named per-bit meanings.
- **ICD** — Interface Control Document; here, a Word `.docx` describing message/field layouts.
