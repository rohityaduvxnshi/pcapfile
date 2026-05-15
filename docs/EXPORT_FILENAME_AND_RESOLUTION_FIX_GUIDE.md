# Export Filename and Resolution Calculation Fix Guide

This guide explains two current problems in the PCAP UDP extractor project and gives the exact implementation plan to fix them safely.

No core parsing logic should be changed for these fixes. The PCAP reader, UDP parser, extraction loop, and CSV writer should remain structurally the same.

---

## Problems Covered

### Problem 1: Export file name format

Required exported CSV file name format:

```text
<uploaded_file_name>_<today_date>_<system_time>.csv
```

Example:

```text
sample_capture_20260515_143022.csv
```

Meaning:

```text
sample_capture = original uploaded PCAP/PCAPNG file name without extension
20260515       = current system date in yyyyMMdd format
143022         = current system time in HHmmss format
.csv           = exported CSV extension
```

---

### Problem 2: Resolution expression calculation

Example field:

```text
Field name  : Heading
Byte offset : 15
Length      : 2
Resolution  : 180/2^15
Hex value   : 1605
```

Correct raw decimal conversion:

```text
0x1605 = 5637
```

Correct resolution:

```text
180 / 2^15
= 180 / 32768
= 0.0054931640625
```

Correct final value:

```text
Final value = raw decimal value * resolution
Final value = 5637 * 0.0054931640625
Final value = 30.9649658203125
```

Displayed value may be rounded to:

```text
30.964966
```

or if 3 decimals are preferred:

```text
30.965
```

Important correction: `180/2^15` gives `0.0054931640625`. `180/2^25` gives `0.000005364418029785156`, which would give a completely different final value. For the value around `30.964`, the exponent must be `15`, not `25`.

---

## Why the Wrong Output Was Coming

The extraction formula in `ExtractionEngine.cpp` is already conceptually correct:

```cpp
const double finalValue = static_cast<double>(rawValue) * field.resolution;
```

So the main problem is not the multiplication formula.

The problem happens before this stage, while reading the Resolution column from the UI.

Current risky logic in `MainWindow.cpp`:

```cpp
field.resolution = resolutionText.toDouble();
```

This works only for direct decimal values such as:

```text
1
0.1
0.005493
```

But it does not properly solve mathematical expressions such as:

```text
180/2^15
```

`QString::toDouble()` is not a math parser. It cannot evaluate `/`, `^`, brackets, or formulas. It only converts a plain numeric string into a double.

That means the application can validate or store the wrong resolution, or fail to apply the intended resolution. Once the wrong resolution is stored in `field.resolution`, the extraction engine multiplies the raw value correctly but with the wrong multiplier. That is why the final answer becomes wrong.

Possible causes of an output around `15.9`:

1. Wrong resolution value was stored because `180/2^15` was not evaluated correctly.
2. Wrong byte offset was used, so the extracted raw hex was not actually `1605`.
3. Byte numbering confusion: the code uses zero-based byte offset from UDP payload. If the user says byte no. 15 and 16 in human counting, the UI offset may need to be `14`, not `15`.
4. Expression parser may be treating `^` incorrectly if a partial evaluator was added. In C++, `^` is bitwise XOR, not exponentiation. A custom parser must explicitly treat `^` as power.

---

## Fix 1: Add Default Export Filename Builder

### File to modify

```text
sources/MainWindow.cpp
```

### Headers to add

At the top of `MainWindow.cpp`, add:

```cpp
#include <QDate>
#include <QFileInfo>
#include <QRegExp>
#include <QTime>
```

`QFileInfo` gets the uploaded file name.
`QDate` gets the current date.
`QTime` gets the current system time.
`QRegExp` is used to sanitize the file name.

---

### Add this helper function above `MainWindow::onStartClicked()`

```cpp
namespace
{
QString safeExportBaseName(QString name)
{
    name = name.trimmed();

    if (name.isEmpty())
    {
        name = "export";
    }

    // Replace characters that are unsafe in Windows file names.
    name.replace(QRegExp("[\\\\/:*?\"<>|]"), "_");
    name.replace(QRegExp("\\s+"), "_");

    return name;
}

QString buildDefaultCsvFileName(const QString& inputFilePath)
{
    const QFileInfo inputInfo(inputFilePath.trimmed());
    const QString uploadedName = safeExportBaseName(inputInfo.completeBaseName());
    const QString today = QDate::currentDate().toString("yyyyMMdd");
    const QString systemTime = QTime::currentTime().toString("HHmmss");

    return QString("%1_%2_%3.csv")
        .arg(uploadedName)
        .arg(today)
        .arg(systemTime);
}
}
```

---

### Replace current save dialog default name

Current code:

```cpp
QString csvPath = QFileDialog::getSaveFileName(this, "Save CSV Output", "extracted_udp_data.csv", "CSV Files (*.csv);;All Files (*.*)");
```

Replace with:

```cpp
const QFileInfo inputInfo(ui->txtFilePath->text().trimmed());
const QString defaultCsvName = buildDefaultCsvFileName(ui->txtFilePath->text());
const QString defaultCsvPath = inputInfo.absoluteDir().filePath(defaultCsvName);

QString csvPath = QFileDialog::getSaveFileName(
    this,
    "Save CSV Output",
    defaultCsvPath,
    "CSV Files (*.csv);;All Files (*.*)");
```

Keep this existing extension guard after it:

```cpp
if (!csvPath.toLower().endsWith(".csv"))
{
    csvPath += ".csv";
}
```

### Result

If the uploaded file is:

```text
radar_log.pcapng
```

and export is done on 15 May 2026 at 14:30:22, the default export name becomes:

```text
radar_log_20260515_143022.csv
```

The user can still change the folder or file name manually from the save dialog.

---

## Fix 2: Properly Evaluate Resolution Expressions

### Files already planned in previous manual

```text
headers/MathExpressionEvaluator.h
sources/MathExpressionEvaluator.cpp
```

These should evaluate strings like:

```text
180/2^15
360/2^16
(180 + 20) / 2^15
1e-3
```

The parser must support:

```text
+
-
*
/
^
parentheses
floating point numbers
scientific notation
```

Critical rule:

```text
^ must mean exponent/power, not C++ bitwise XOR.
```

---

## Main Required Change in `MainWindow.cpp`

### Add include

At the top of `MainWindow.cpp`, add:

```cpp
#include "MathExpressionEvaluator.h"
```

---

### Replace this wrong/weak line in `collectFields()`

Current code:

```cpp
field.resolution = resolutionText.toDouble();
```

Replace with:

```cpp
double solvedResolution = 0.0;
QString resolutionError;

if (!MathExpressionEvaluator::evaluate(resolutionText, solvedResolution, resolutionError))
{
    errorMessage = QString("Row %1: Invalid resolution expression. %2")
        .arg(row + 1)
        .arg(resolutionError);
    return false;
}

field.resolution = solvedResolution;
```

This ensures that the field stores:

```text
0.0054931640625
```

not the raw text:

```text
180/2^15
```

and not an incorrect partial conversion.

---

## Required Change in `InputValidator.cpp`

Current validation uses:

```cpp
const double resolution = resolutionText.trimmed().toDouble(&resolutionOk);
if (!resolutionOk || resolution <= 0.0)
{
    errorMessage = "Resolution must be a number greater than 0.";
    return false;
}
```

This rejects or mishandles expressions.

Replace it with expression validation:

```cpp
#include "MathExpressionEvaluator.h"
```

Then inside `validateField()` replace the resolution validation block with:

```cpp
double resolution = 0.0;
QString resolutionError;

if (!MathExpressionEvaluator::evaluate(resolutionText.trimmed(), resolution, resolutionError))
{
    errorMessage = "Resolution expression is invalid: " + resolutionError;
    return false;
}

if (resolution <= 0.0)
{
    errorMessage = "Resolution must evaluate to a number greater than 0.";
    return false;
}
```

---

## Required Change in `.pro` File

### File to modify

```text
PcapUdpExtractor.pro
```

Add the new header:

```text
headers/MathExpressionEvaluator.h
```

Add the new source:

```text
sources/MathExpressionEvaluator.cpp
```

Example:

```qmake
HEADERS += \
    headers/MainWindow.h \
    headers/AppTypes.h \
    headers/FieldDefinition.h \
    headers/InputValidator.h \
    headers/PcapFileReader.h \
    headers/UdpPacketParser.h \
    headers/ExtractionEngine.h \
    headers/CsvExporter.h \
    headers/MathExpressionEvaluator.h

SOURCES += \
    sources/main.cpp \
    sources/MainWindow.cpp \
    sources/FieldDefinition.cpp \
    sources/InputValidator.cpp \
    sources/PcapFileReader.cpp \
    sources/UdpPacketParser.cpp \
    sources/ExtractionEngine.cpp \
    sources/CsvExporter.cpp \
    sources/MathExpressionEvaluator.cpp
```

Do not remove existing files from the `.pro` file.

---

## Verification Test for Resolution

Use this field definition:

```text
Field Name  : Heading
Byte Offset : 15
Length      : 2
Resolution  : 180/2^15
```

Expected extraction if the two payload bytes at offset 15 and 16 are:

```text
16 05
```

Step-by-step expected calculation:

```text
Raw hex     = 1605
Raw decimal = 5637
Resolution  = 180 / 32768
Resolution  = 0.0054931640625
Final value = 5637 * 0.0054931640625
Final value = 30.9649658203125
```

Expected CSV/display value with current 6-decimal formatting:

```text
30.964966
```

If the displayed value is still around `15.9`, check these immediately:

1. Confirm the payload bytes at the selected offset are really `16 05`.
2. Confirm whether the UI byte offset is zero-based or human byte number based.
3. Confirm `field.resolution` is storing `0.0054931640625` after evaluation.
4. Confirm the latest build is running, not an older `.exe`.
5. Clean and rebuild the project after adding the new evaluator files.

---

## Byte Offset Rule

The current extraction code uses this access pattern:

```cpp
payload.at(field.byteOffset + i)
```

That means byte offset is zero-based.

So:

```text
UI byte offset 0  = first byte of UDP payload
UI byte offset 1  = second byte of UDP payload
UI byte offset 15 = sixteenth byte of UDP payload
```

If your document says `byte no. 15 and 16` using human counting, the zero-based UI offset should be:

```text
14
```

If your document says `offset 15 and 16`, then the UI offset should be:

```text
15
```

This must be verified from the packet format document.

---

## What Not to Change

Do not change these unless a separate bug is proven:

```text
PcapFileReader.cpp
UdpPacketParser.cpp
ExtractionEngine.cpp raw byte reading logic
CsvExporter.cpp row writing logic
```

The extraction formula is already correct:

```text
Final Value = Raw Unsigned Big-Endian Integer * Resolution
```

The required fix is to make sure Resolution is correctly evaluated before it reaches the extraction engine.

---

## Compact Implementation Checklist

1. Add `MathExpressionEvaluator.h`.
2. Add `MathExpressionEvaluator.cpp`.
3. Register both files in `PcapUdpExtractor.pro`.
4. Include `MathExpressionEvaluator.h` in `InputValidator.cpp`.
5. Replace `resolutionText.toDouble()` validation with expression evaluation.
6. Include `MathExpressionEvaluator.h` in `MainWindow.cpp`.
7. Replace `field.resolution = resolutionText.toDouble();` with evaluated expression result.
8. Add export filename helper in `MainWindow.cpp`.
9. Change save dialog default file name to `<uploaded>_<yyyyMMdd>_<HHmmss>.csv`.
10. Clean build and run the 0x1605 test.

---

## Final Expected Behaviour

For uploaded file:

```text
radar_capture.pcapng
```

Exported default file name:

```text
radar_capture_20260515_143022.csv
```

For payload bytes:

```text
16 05
```

with resolution:

```text
180/2^15
```

Final extracted field value:

```text
30.964966
```

This is the correct behaviour.
