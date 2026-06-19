# Universal Data Simulator — User Manual

Universal Data Simulator is an offline desktop tool that **transmits** user-defined binary (HEX)
and **NMEA 0183** messages over **UDP (Ethernet)** or a **serial COM port**. It is the sender-side
companion to *Universal Wireshark Log Reader* (the receiver/parser). Use it to feed a system under
test with exactly the bytes you choose, at the rate you choose.

> Tip: press **F1** any time to open this manual, and use the search box at the top to jump
> straight to a **Common function** or a **Troubleshooting** entry.

---

## Using this manual

This manual is built into the app (**Help → User Manual**, or **F1**). The list on the left is a
clickable table of contents; the box at the top searches the whole manual — type a word like
`troubleshooting`, `endian` or `checksum` and press **Enter** (or **Next**/**Prev**) to step through
every match. The same content is also provided as `simulator_manual.docx`.

![The searchable in-app manual viewer](sim/help-viewer.png)

---

## Overview

![Universal Data Simulator — main window (Modern Light theme)](sim/main-light.png)

The window is a single top-to-bottom flow:

1. **Destination** — choose Ethernet (UDP) or a serial COM port and press **Connect**.
2. **Messages** — define one or more messages, each with its own fields, values and send rate.
3. **Send** — verify everything and stream every ticked message at its own rate until Stop.
4. **Outgoing Data Preview** — the last 5 payloads handed to the link, in hex.

The same window in the **Slate Dark** theme (toggle with **Ctrl+T**):

![Main window (Slate Dark theme)](sim/main-dark.png)

---

## Getting started

1. **Pick a destination** at the top — *Ethernet (UDP)* (type the destination IP + port) or
   *Serial Port* (pick the COM port, baud rate, data bits, parity, stop bits).
2. Press **Connect**. The dot turns **green** when the link opened and a health-check message was
   transmitted, **red** (with a reason + solution) if it failed.
3. Press **Add Message**, give it a name, payload length and send rate, then **Configure Fields**.
4. Type a **Value** for each field — the **Hex (auto)** column shows the exact bytes in real time.
5. Tick the message's **Send?** box and press **Send** (F5). Watch the preview at the bottom.

---

## The main window, panel by panel

### Destination
Pick **Ethernet (UDP)** for an IP + port, or **Serial Port** for a COM port with baud / data bits /
parity / stop bits. Press **Refresh** to re-scan the serial ports on this PC. **Connect** opens the
link and sends a short health-check message; for UDP a green dot means "handed to the network"
(UDP cannot confirm a listener). **Disconnect** closes the link.

### Messages
Each row is a message: **Send?** tick, **Name**, **Format** (HEX or NMEA), payload **Length**,
**Rate (Hz)**, field count and a **Configure Fields** button.
- **Add Message / Edit / Remove** manage the list.
- **Import ICD…** reads a Word `.docx` ICD and turns its tables into ready-to-send messages.

### Configure Fields (HEX messages)
Columns: **Field Name · Byte Offset · Type · Length · Endian · Resolution · Value · Hex (auto) · Bits**.
- The **Value** is typed in the field's own type; the read-only **Hex (auto)** cell shows the exact
  bytes that will be sent and updates as you type (it turns red with a reason if the value does not fit).
- **Endian** sets Big-endian (default) or Little-endian per field (numeric fields only).
- **Bits…** opens a two-way bit editor — type a value or toggle individual bits.
- Select several rows (Ctrl/Shift-click) to **delete them at once**; **drag a row** to reorder it
  (Alt+Up / Alt+Down also work).

### Send
**Send** (F5) verifies everything first — connection, ticked messages, and that every field encodes
and fits — and reports all problems in one dialog, each with a reason and a solution. It then streams
every ticked message at its own rate until **Stop** (Shift+F5). You can **edit values, rates or the
Send? tick while streaming** — the affected stream updates in place without a gap.

### Outgoing Data Preview
The bottom box shows the last **5** payloads handed to the link, newest at the bottom, in hex.

---

## Common functions

### Send a fixed UDP packet to a receiver
1. Destination → **Ethernet (UDP)**; type the receiver IP and port; **Connect** (dot green).
2. **Add Message** (e.g. name `TEST`, length 4, rate 1 Hz) → **Configure Fields**.
3. Add a field (type `uint`, length 4), type a **Value**, **Save**.
4. Tick **Send?**, press **Send**. The preview shows the bytes leaving.

### Send a value in Little-endian
In Configure Fields, set that field's **Endian** column to **Little-endian**. A `uint32` value of `1`
becomes `01 00 00 00` on the wire instead of `00 00 00 01`. String fields are never reversed.

### Change a value while it is already streaming
With Send running, open **Configure Fields**, change the Value, **Save**. The live stream picks up the
new bytes immediately — no Stop/Start needed. Changing the **Rate** retunes the cadence live; toggling
**Send?** starts or stops just that one message.

### Build an NMEA 0183 sentence
Add a message and set its **Format** to **NMEA**; pick the sentence (e.g. GGA) and talker (e.g. GP).
In Configure Fields, type each field's token value; the simulator builds
`$GPGGA,...*HH<CR><LF>` with a correct XOR checksum.

### Import messages from a Word ICD
**File → Import ICD…** (Ctrl+I) → pick the `.docx` → tick the tables that hold field definitions →
**Build / Preview** → review → **OK**. Each table becomes a message with its byte offsets, lengths and
types filled in; type the values and send. Names that clash with existing ones are auto-renamed
(`ttd` → `ttd_1`) with a warning.

### Save / reload your work
**File → Save Setup** writes the whole setup (destination + messages + fields + values + rates) to a
JSON file; **Open Setup** loads one. The setup is also auto-saved on close and restored on next launch.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Red dot after Connect (UDP) | Bad IP, or no route to the destination | Re-check the IP/port; confirm the network route. The warning box states the exact reason. |
| Red dot after Connect (Serial) | COM port busy, missing, or wrong settings | Close other apps using the port; press **Refresh**; check baud/data/parity/stop bits. |
| "Cannot Send" dialog lists problems | Not connected, no message ticked, or a field value doesn't fit | Fix each listed item (each has a solution). Connect first; tick at least one message; correct out-of-range values. |
| Hex (auto) cell shows `—` in red | The Value can't be encoded in that type/length | Hover the cell for the reason; widen the Length, change the Type, or enter a value in range. |
| Value too big for the field | Value exceeds the type/length range | Use a wider type or larger Length, raise the Resolution, or send a smaller value. |
| Bytes arrive byte-swapped at the receiver | Endianness mismatch | Set the field's **Endian** to match the receiver (Big vs Little). |
| Nothing received over UDP | Green dot only means "sent to the OS" | Confirm the receiver is listening on that exact IP/port; check firewalls; try the parser app's Live mode as a receiver. |
| NMEA sentence rejected by the receiver | Bad talker/formatter or a token contains `, * $ !` | Use a valid 2-char talker + 3-char formatter; remove delimiter characters from token values. |
| ICD import offsets look shifted by one | The ICD's offset base (0- vs 1-based) was mis-detected | In the import's **Table Settings**, flip the **Offset base**; every review offset is also editable. |
| Send rate seems capped | The timer fires at most once per millisecond | Rates above ~1000 Hz are clamped; use 0.001–1000 Hz. |

---

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| F1 | Open this user manual |
| Shift+F1 | Keyboard shortcuts box |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / Save / Save-As setup |
| Ctrl+I | Import ICD (.docx) |
| F5 / Shift+F5 | Send / Stop |
| Ctrl+T | Toggle Light/Dark theme |
| Ctrl+Q | Quit |
| Insert / Ctrl+E / Ctrl+Delete | (Field dialog) Add / Edit / Remove field(s) |
| Alt+Up / Alt+Down | (Field dialog) Move the selected field |

---

## Glossary

- **Payload** — the bytes of one message, excluding any transport headers.
- **Byte offset** — 1-based position of a field's first byte within the payload.
- **Resolution** — scale factor; the transmitted raw value is `round(value ÷ resolution)`.
- **Endianness** — byte order on the wire: Big-endian (most-significant byte first) or Little-endian.
- **NMEA 0183** — an ASCII sentence format (`$TALKER+FORMATTER,fields*CHECKSUM`).
- **ICD** — Interface Control Document; here, a Word `.docx` describing message/field layouts.
