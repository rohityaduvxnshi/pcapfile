# Excel (.xlsx) export — QXlsx library guide

Every extraction output is now a real Excel workbook (`.xlsx`): File-mode
exports (single and per-message) and Live-mode per-message captures.
Configuration import/export (field-definition CSV/JSON, bit-rule CSV)
intentionally **stays CSV/JSON** — those are interchange formats, not data
outputs.

## The library

| | |
|---|---|
| Library | **QXlsx** (https://github.com/QtExcel/QXlsx) |
| Version vendored | **v1.4.10** (tag `v1.4.10`) |
| License | MIT — `third_party/QXlsx/LICENSE` |
| Location in this repo | `third_party/QXlsx/` (`QXlsx.pri` + `header/` + `source/`) |
| Dependencies | **None beyond Qt itself.** It compiles from source into the app (no DLL) and uses Qt's private `QZipReader`/`QZipWriter` — the same `QT += gui-private` module this project already used for the ICD `.docx` importer. |
| Qt compatibility | Qt 5 and Qt 6; plain C++11. Verified API-clean for Qt 5.10 (no `Qt::endl`, `QRandomGenerator`, `Qt::SplitBehavior`, `qsizetype`, etc.). |

## How it is wired into this project

`PcapUdpExtractor.pro`:

```qmake
QXLSX_PARENTPATH = $$PWD/third_party/QXlsx/
QXLSX_HEADERPATH = $$PWD/third_party/QXlsx/header/
QXLSX_SOURCEPATH = $$PWD/third_party/QXlsx/source/
include(third_party/QXlsx/QXlsx.pri)
```

The `.pri` adds all QXlsx sources/headers and the include path itself, and
declares `QT += core gui-private`. Nothing else is needed. After pulling this
repo on any PC: open the `.pro` in Qt Creator → qmake → build. There is **no
separate install step** — the library travels with the repo.

## How to add QXlsx to ANY other project / PC

1. Copy the whole `third_party/QXlsx/` folder into the other project
   (any folder name works; adjust the paths below).
2. In that project's `.pro` add the four lines shown above (fix the paths to
   where you put the folder).
3. Make sure the kit has the Qt private GUI headers (every normal Qt install
   from the online/offline installer has them; `QT += gui-private` in the
   `.pri` picks them up).
4. In code:
   ```cpp
   #include "xlsxdocument.h"
   QXlsx::Document xlsx;
   xlsx.write(1, 1, "Hello");      // row, column are 1-based
   xlsx.saveAs("Test.xlsx");
   ```
That's all — it compiles into your binary, works fully offline, and the MIT
license only requires keeping the `LICENSE` file with the sources.

## The two wrappers this app uses

* [`ExcelExporter`](../headers/ExcelExporter.h) — batch writer
  (`open(path, headers)` / `writeRow` / `close` / `finalize`). Used by
  File-mode exports. `finalize()` reports save failures.
* [`ExcelStreamWriter`](../headers/ExcelStreamWriter.h) — streaming writer
  (`open(path, fieldHeaders, includeSourceColumns)` /
  `writeRow(timestamp, source, port, values)` / `flush` / `close`). Used by
  Live mode (one workbook per configured message).

Both share one cell-typing rule (`ExcelExporter::cellVariant`): plain decimal
numbers become real Excel numbers (sortable/chartable immediately); hex strings
(`0xAB`), flags (`True`), text, and integers longer than 15 digits (which an
Excel double cannot hold exactly — e.g. large Uint64 raw values) stay text.
Header rows are bold with a light-blue fill.

## Important behaviour difference vs CSV

A `.xlsx` is a zipped XML package — rows **cannot be appended on disk one at a
time**. Rows accumulate in memory and the workbook is written when the export
finishes / the capture stops (`flush()`/`close()`). Practical consequences:

* If the app is killed mid-capture, the unfinished workbook is lost (CSV would
  have kept already-flushed rows). Stop the capture to commit the file.
* Do not keep the output file open in Excel while exporting — Excel locks the
  file and the final save fails (the app reports exactly that).
* Very large captures (hundreds of thousands of rows) use correspondingly more
  RAM during the run.
