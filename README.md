# Universal Data Suite

One offline Qt 5.10 project that builds **two desktop apps** from a shared core:

| App | Executable | What it does |
|---|---|---|
| **Universal Wireshark Log Reader** (`app_parser/`) | `UniversalWiresharkLogReader.exe` | Reads/decodes UDP data from a `.pcap`/`.pcapng` file or a live UDP stream into engineering values; exports CSV / Excel. |
| **Universal Data Simulator** (`app_simulator/`) | `UniversalDataSimulator.exe` | Transmits user-defined HEX and NMEA-0183 messages over UDP or a serial COM port. |

The two apps are each other's test rig — the simulator sends what the reader receives. They share a
single core (`shared/`): the data model, NMEA stack, themes, ICD `.docx` import and the searchable
in-app manual.

- Qt Widgets, C++11, Qt 5.10.1 (mingw53_32). No external libraries except the vendored **QXlsx** (Excel,
  reader only). Clean **Modern Light** default look with a one-click **Dark** theme (Ctrl+T).
- Every app has a built-in, **searchable user manual** — *Help → User Manual* (F1) — covering common
  functions and troubleshooting, plus a matching `.docx` under `docs/manual/`.

## Build

```powershell
$env:PATH = 'D:\qt\5.10.1\mingw53_32\bin;D:\qt\Tools\mingw530_32\bin;' + $env:PATH
New-Item -ItemType Directory -Force build-suite | Out-Null
Set-Location build-suite
qmake ..\universal-data-suite.pro     # builds both apps
mingw32-make -j4
```

Build just one app with `qmake ..\app_parser\parser.pro` or `qmake ..\app_simulator\simulator.pro`.

## Repository layout

```
universal-data-suite.pro   SUBDIRS project (both apps)
shared/                    shared core (shared.pri, included by each app)
app_parser/                Universal Wireshark Log Reader
app_simulator/             Universal Data Simulator
third_party/QXlsx/         vendored Excel library (reader only)
docs/manual/               manual sources (.md) + generated .html/.docx + screenshots + make_manuals.py
CLAUDE.md                  developer/project memory
```

See `CLAUDE.md` for the architecture, the shared-vs-app-specific file map, and the encode/decode contract.
