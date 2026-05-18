# V4 Review and Fix Report

Scope reviewed: all standalone V4 files from the supplied package.

Files reviewed:
- `LiveUdpReceiver.h`
- `LiveUdpReceiver.cpp`
- `CsvStreamWriter.h`
- `CsvStreamWriter.cpp`
- `V4_LIVE_UDP_CAPTURE.md`
- `V4_PRO_AND_MAINWINDOW_INTEGRATION.md`
- `udp_test_sender.py`

## Verdict

The V4 design is usable. It correctly keeps live UDP as a parallel payload source and does not route live datagrams through `UdpPacketParser`. The main architecture is safe for V1/V2/V3 because the new receiver and streaming writer are isolated.

## Weaknesses found and fixed

### 1. CSV text mode + manual CRLF line endings

Problem:
`CsvStreamWriter` opened the file with `QIODevice::Text` while also manually writing `\r\n`.
On Windows, text mode can translate `\n` again, producing bad line endings such as `\r\r\n`.

Fix:
Changed the file open mode to binary write/truncate mode:

```cpp
m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)
```

The writer still writes `\r\n` manually, so CSV line endings remain consistent.

### 2. Header row was written but not immediately flushed

Problem:
If capture was started and stopped/crashed early, the header could remain buffered.

Fix:
After writing the header, `CsvStreamWriter::open()` now calls `flush()` immediately. This leaves a readable CSV header even for an empty or early-stopped capture.

### 3. CSV row/header mismatch could silently corrupt CSV structure

Problem:
`writeRow()` accepted any number of field values. If integration returned fewer/more values than the header count, the CSV would become malformed without an obvious error.

Fix:
Added `m_expectedValueCount` to `CsvStreamWriter`. It is set from `fieldHeaders.size()` in `open()`. `writeRow()` now rejects rows where `values.size()` does not match the expected count.

Failure message example:

```text
CSV row has X values, but header expects Y values.
```

### 4. Short-write error could report an empty error string

Problem:
If `QFile::write()` returned a short write but `errorString()` was empty, the UI could show a blank error.

Fix:
Added a fallback error message:

```text
Short write to CSV file. Disk may be full or unavailable.
```

### 5. Datagram size guard was incomplete

Problem:
`pendingDatagramSize()` returns `qint64`, but `QByteArray::resize()` takes `int`. Normal UDP payloads are small, but the code should still guard against impossible size values before casting.

Fix:
Added an upper-bound check:

```cpp
pending > std::numeric_limits<int>::max()
```

### 6. Final flush error was ignored in the integration guide

Problem:
`stopLiveCapture()` called `flush(err)` but ignored the return value. This could hide a final disk/write failure.

Fix:
Updated the integration guide to check the return value and show a warning if final flush fails.

### 7. Preview table column rule was not explicit enough

Problem:
The live preview repaint instructions did not clearly state the required column count.

Fix:
Updated the integration guide: preview columns must match:

```text
TimestampUtc, SourceIP, SourcePort + buildLiveCsvHeaders()
```

### 8. Test sender accepted invalid ports without clean handling

Problem:
`udp_test_sender.py` used `int(sys.argv[2])` directly and could crash on invalid input.

Fix:
Added argument validation:

```text
- non-integer port -> clean error
- port outside 1..65535 -> clean error
```

## Files committed into the repo

```text
headers/LiveUdpReceiver.h
headers/CsvStreamWriter.h
sources/LiveUdpReceiver.cpp
sources/CsvStreamWriter.cpp
docs/V4_LIVE_UDP_CAPTURE.md
docs/V4_PRO_AND_MAINWINDOW_INTEGRATION.md
docs/V4_FIX_REPORT.md
tools/udp_test_sender.py
```

Updated:

```text
PcapUdpExtractor.pro
```

The `.pro` file now includes:

```qmake
QT += core gui widgets network

SOURCES += sources/LiveUdpReceiver.cpp
SOURCES += sources/CsvStreamWriter.cpp

HEADERS += headers/LiveUdpReceiver.h
HEADERS += headers/CsvStreamWriter.h
```

## Remaining project-level dependency

The standalone V4 infrastructure is committed. Full live mode is not complete until `MainWindow.ui`, `MainWindow.h`, and `MainWindow.cpp` are wired according to `docs/V4_PRO_AND_MAINWINDOW_INTEGRATION.md`.

This is intentional because blindly rewriting the existing UI and MainWindow logic without direct project-specific adaptation can break V1/V2/V3.

## Required local verification

Run:

```text
Clean -> Run qmake -> Rebuild
```

Then run the regression checklist in:

```text
docs/V4_LIVE_UDP_CAPTURE.md
```
