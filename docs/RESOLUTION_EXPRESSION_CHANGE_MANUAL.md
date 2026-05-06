# Resolution Expression Change Manual

This document stores the planned changes for adding full mathematical expression support in the **Resolution** field.

No existing project code is changed by this document. It is only a manual for future implementation.

---

## Purpose

Currently, the Resolution field accepts only direct decimal values such as:

```text
0.01
1
0.0005
```

The required improvement is to allow mathematical expressions such as:

```text
180/2^15
(180 + 20) / (2^15)
360 / pow(2, 16)
sqrt(2) * 0.01
pi / 180
1e-3
2 * (10 + 5) / 3
```

The intended flow is:

```text
Resolution text input
-> solve mathematical expression
-> store solved result as double
-> use existing field.resolution in ExtractionEngine
```

The existing extraction formula remains unchanged:

```text
Final Value = Raw Value x Resolution
```

---

## Files to Add

```text
headers/MathExpressionEvaluator.h
sources/MathExpressionEvaluator.cpp
```

## Files to Modify

```text
PcapUdpExtractor.pro
headers/InputValidator.h
sources/InputValidator.cpp
sources/MainWindow.cpp
```

## Files Not to Modify

```text
PcapFileReader.cpp
UdpPacketParser.cpp
ExtractionEngine.cpp
CsvExporter.cpp
AppTypes.h
```

This keeps the change isolated to resolution expression handling only.

---

# Step 1: Add `headers/MathExpressionEvaluator.h`

Create this file:

```text
headers/MathExpressionEvaluator.h
```

Paste this code:

```cpp
#ifndef MATHEXPRESSIONEVALUATOR_H
#define MATHEXPRESSIONEVALUATOR_H

#include <QString>
#include <QList>

class MathExpressionEvaluator
{
public:
    static bool evaluate(const QString& expression, double& result, QString& errorMessage);

private:
    explicit MathExpressionEvaluator(const QString& expression);

    double parseExpression();
    double parseTerm();
    double parseUnary();
    double parsePower();
    double parsePrimary();
    double parseNumber();
    QString parseIdentifier();

    double applyFunction(const QString& name, const QList<double>& args);

    void skipSpaces();
    bool match(QChar ch);
    QChar peek() const;
    void setError(const QString& message);

    QString m_text;
    int m_pos;
    bool m_ok;
    QString m_error;
};

#endif // MATHEXPRESSIONEVALUATOR_H
```

---

# Step 2: Add `sources/MathExpressionEvaluator.cpp`

Create this file:

```text
sources/MathExpressionEvaluator.cpp
```

Paste this code:

```cpp
#include "MathExpressionEvaluator.h"

#include <cmath>

namespace
{
const double CONST_PI = 3.14159265358979323846;
const double CONST_E  = 2.71828182845904523536;

bool isValidNumber(double value)
{
    return std::isfinite(value);
}
}

MathExpressionEvaluator::MathExpressionEvaluator(const QString& expression)
    : m_text(expression),
      m_pos(0),
      m_ok(true)
{
}

bool MathExpressionEvaluator::evaluate(const QString& expression,
                                       double& result,
                                       QString& errorMessage)
{
    MathExpressionEvaluator parser(expression);

    result = parser.parseExpression();
    parser.skipSpaces();

    if (!parser.m_ok)
    {
        errorMessage = parser.m_error;
        return false;
    }

    if (parser.m_pos != parser.m_text.length())
    {
        errorMessage = QString("Unexpected character: '%1'")
                           .arg(parser.m_text.at(parser.m_pos));
        return false;
    }

    if (!isValidNumber(result))
    {
        errorMessage = "Expression result is not a valid finite number.";
        return false;
    }

    return true;
}

void MathExpressionEvaluator::setError(const QString& message)
{
    if (m_ok)
    {
        m_ok = false;
        m_error = message;
    }
}

void MathExpressionEvaluator::skipSpaces()
{
    while (m_pos < m_text.length() && m_text.at(m_pos).isSpace())
    {
        ++m_pos;
    }
}

QChar MathExpressionEvaluator::peek() const
{
    if (m_pos >= m_text.length())
    {
        return QChar();
    }

    return m_text.at(m_pos);
}

bool MathExpressionEvaluator::match(QChar ch)
{
    skipSpaces();

    if (m_pos < m_text.length() && m_text.at(m_pos) == ch)
    {
        ++m_pos;
        return true;
    }

    return false;
}

double MathExpressionEvaluator::parseExpression()
{
    double left = parseTerm();

    while (m_ok)
    {
        if (match(QChar('+')))
        {
            left += parseTerm();
        }
        else if (match(QChar('-')))
        {
            left -= parseTerm();
        }
        else
        {
            break;
        }
    }

    return left;
}

double MathExpressionEvaluator::parseTerm()
{
    double left = parseUnary();

    while (m_ok)
    {
        if (match(QChar('*')))
        {
            left *= parseUnary();
        }
        else if (match(QChar('/')))
        {
            const double right = parseUnary();

            if (right == 0.0)
            {
                setError("Division by zero.");
                return 0.0;
            }

            left /= right;
        }
        else
        {
            break;
        }
    }

    return left;
}

double MathExpressionEvaluator::parseUnary()
{
    skipSpaces();

    if (match(QChar('+')))
    {
        return parseUnary();
    }

    if (match(QChar('-')))
    {
        return -parseUnary();
    }

    return parsePower();
}

double MathExpressionEvaluator::parsePower()
{
    double left = parsePrimary();

    if (m_ok && match(QChar('^')))
    {
        const double right = parseUnary();
        left = std::pow(left, right);

        if (!isValidNumber(left))
        {
            setError("Invalid power operation.");
            return 0.0;
        }
    }

    return left;
}

double MathExpressionEvaluator::parsePrimary()
{
    skipSpaces();

    if (match(QChar('(')))
    {
        const double value = parseExpression();

        if (!match(QChar(')')))
        {
            setError("Missing closing bracket.");
            return 0.0;
        }

        return value;
    }

    const QChar ch = peek();

    if (ch.isLetter() || ch == QChar('_'))
    {
        const QString name = parseIdentifier();

        skipSpaces();

        if (match(QChar('(')))
        {
            QList<double> args;

            skipSpaces();

            if (!match(QChar(')')))
            {
                while (m_ok)
                {
                    args.append(parseExpression());

                    if (match(QChar(',')))
                    {
                        continue;
                    }

                    if (match(QChar(')')))
                    {
                        break;
                    }

                    setError("Missing comma or closing bracket in function.");
                    return 0.0;
                }
            }

            return applyFunction(name, args);
        }

        if (name == "pi")
        {
            return CONST_PI;
        }

        if (name == "e")
        {
            return CONST_E;
        }

        setError(QString("Unknown constant or function: %1").arg(name));
        return 0.0;
    }

    return parseNumber();
}

QString MathExpressionEvaluator::parseIdentifier()
{
    skipSpaces();

    const int start = m_pos;

    if (m_pos >= m_text.length() ||
        !(m_text.at(m_pos).isLetter() || m_text.at(m_pos) == QChar('_')))
    {
        setError("Expected function name or constant.");
        return QString();
    }

    ++m_pos;

    while (m_pos < m_text.length())
    {
        const QChar ch = m_text.at(m_pos);

        if (ch.isLetterOrNumber() || ch == QChar('_'))
        {
            ++m_pos;
        }
        else
        {
            break;
        }
    }

    return m_text.mid(start, m_pos - start).toLower();
}

double MathExpressionEvaluator::parseNumber()
{
    skipSpaces();

    const int start = m_pos;
    bool hasDigit = false;

    while (m_pos < m_text.length() && m_text.at(m_pos).isDigit())
    {
        hasDigit = true;
        ++m_pos;
    }

    if (m_pos < m_text.length() && m_text.at(m_pos) == QChar('.'))
    {
        ++m_pos;

        while (m_pos < m_text.length() && m_text.at(m_pos).isDigit())
        {
            hasDigit = true;
            ++m_pos;
        }
    }

    if (!hasDigit)
    {
        setError("Expected number.");
        return 0.0;
    }

    if (m_pos < m_text.length() &&
        (m_text.at(m_pos) == QChar('e') || m_text.at(m_pos) == QChar('E')))
    {
        const int exponentPosition = m_pos;
        ++m_pos;

        if (m_pos < m_text.length() &&
            (m_text.at(m_pos) == QChar('+') || m_text.at(m_pos) == QChar('-')))
        {
            ++m_pos;
        }

        const int exponentStart = m_pos;

        while (m_pos < m_text.length() && m_text.at(m_pos).isDigit())
        {
            ++m_pos;
        }

        if (exponentStart == m_pos)
        {
            m_pos = exponentPosition;
        }
    }

    bool ok = false;
    const double value = m_text.mid(start, m_pos - start).toDouble(&ok);

    if (!ok)
    {
        setError("Invalid number.");
        return 0.0;
    }

    return value;
}

double MathExpressionEvaluator::applyFunction(const QString& name, const QList<double>& args)
{
    if (name == "sqrt")
    {
        if (args.size() != 1 || args.at(0) < 0.0)
        {
            setError("sqrt() requires one non-negative argument.");
            return 0.0;
        }

        return std::sqrt(args.at(0));
    }

    if (name == "abs")
    {
        if (args.size() != 1)
        {
            setError("abs() requires one argument.");
            return 0.0;
        }

        return std::fabs(args.at(0));
    }

    if (name == "sin")
    {
        if (args.size() != 1)
        {
            setError("sin() requires one argument.");
            return 0.0;
        }

        return std::sin(args.at(0));
    }

    if (name == "cos")
    {
        if (args.size() != 1)
        {
            setError("cos() requires one argument.");
            return 0.0;
        }

        return std::cos(args.at(0));
    }

    if (name == "tan")
    {
        if (args.size() != 1)
        {
            setError("tan() requires one argument.");
            return 0.0;
        }

        return std::tan(args.at(0));
    }

    if (name == "ln" || name == "log")
    {
        if (args.size() != 1 || args.at(0) <= 0.0)
        {
            setError("log() requires one positive argument.");
            return 0.0;
        }

        return std::log(args.at(0));
    }

    if (name == "log10")
    {
        if (args.size() != 1 || args.at(0) <= 0.0)
        {
            setError("log10() requires one positive argument.");
            return 0.0;
        }

        return std::log10(args.at(0));
    }

    if (name == "exp")
    {
        if (args.size() != 1)
        {
            setError("exp() requires one argument.");
            return 0.0;
        }

        return std::exp(args.at(0));
    }

    if (name == "pow")
    {
        if (args.size() != 2)
        {
            setError("pow() requires two arguments.");
            return 0.0;
        }

        return std::pow(args.at(0), args.at(1));
    }

    if (name == "min")
    {
        if (args.size() < 2)
        {
            setError("min() requires at least two arguments.");
            return 0.0;
        }

        double value = args.at(0);

        for (int i = 1; i < args.size(); ++i)
        {
            if (args.at(i) < value)
            {
                value = args.at(i);
            }
        }

        return value;
    }

    if (name == "max")
    {
        if (args.size() < 2)
        {
            setError("max() requires at least two arguments.");
            return 0.0;
        }

        double value = args.at(0);

        for (int i = 1; i < args.size(); ++i)
        {
            if (args.at(i) > value)
            {
                value = args.at(i);
            }
        }

        return value;
    }

    if (name == "floor")
    {
        if (args.size() != 1)
        {
            setError("floor() requires one argument.");
            return 0.0;
        }

        return std::floor(args.at(0));
    }

    if (name == "ceil")
    {
        if (args.size() != 1)
        {
            setError("ceil() requires one argument.");
            return 0.0;
        }

        return std::ceil(args.at(0));
    }

    setError(QString("Unknown function: %1").arg(name));
    return 0.0;
}
```

---

# Step 3: Modify `PcapUdpExtractor.pro`

Open:

```text
PcapUdpExtractor.pro
```

In the `SOURCES +=` section, add:

```text
    sources/MathExpressionEvaluator.cpp
```

Example:

```pro
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

In the `HEADERS +=` section, add:

```text
    headers/MathExpressionEvaluator.h
```

Example:

```pro
HEADERS += \
    headers/MainWindow.h \
    headers/ui_MainWindow.h \
    headers/AppTypes.h \
    headers/FieldDefinition.h \
    headers/InputValidator.h \
    headers/PcapFileReader.h \
    headers/UdpPacketParser.h \
    headers/ExtractionEngine.h \
    headers/CsvExporter.h \
    headers/MathExpressionEvaluator.h
```

---

# Step 4: Modify `headers/InputValidator.h`

Open:

```text
headers/InputValidator.h
```

Find:

```cpp
static bool validatePortValue(int port, QString& errorMessage);
```

Directly below it, add:

```cpp
static bool solveResolutionExpression(const QString& expression, double& value, QString& errorMessage);
```

So this part becomes:

```cpp
static bool validatePortText(const QString& portText, int& port, QString& errorMessage);
static bool validatePortValue(int port, QString& errorMessage);
static bool solveResolutionExpression(const QString& expression, double& value, QString& errorMessage);
```

---

# Step 5: Modify `sources/InputValidator.cpp`

Open:

```text
sources/InputValidator.cpp
```

At the top, find:

```cpp
#include "InputValidator.h"
```

Add this below it:

```cpp
#include "MathExpressionEvaluator.h"
```

So the top becomes:

```cpp
#include "InputValidator.h"
#include "MathExpressionEvaluator.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
```

Now find this function:

```cpp
bool InputValidator::validatePortValue(int port, QString& errorMessage)
{
    if (port < 0 || port > 65535)
    {
        errorMessage = "UDP port must be between 0 and 65535.";
        return false;
    }

    return true;
}
```

Below it, add this new function:

```cpp
bool InputValidator::solveResolutionExpression(const QString& expression,
                                                double& value,
                                                QString& errorMessage)
{
    if (!MathExpressionEvaluator::evaluate(expression, value, errorMessage))
    {
        errorMessage = "Invalid resolution expression: " + errorMessage;
        return false;
    }

    if (value <= 0.0)
    {
        errorMessage = "Resolution must produce a value greater than 0.";
        return false;
    }

    return true;
}
```

Now inside `validateField()`, find this old block:

```cpp
bool resolutionOk = false;
const double resolution = resolutionText.trimmed().toDouble(&resolutionOk);
if (!resolutionOk || resolution <= 0.0)
{
    errorMessage = "Resolution must be a number greater than 0.";
    return false;
}
```

Replace it with:

```cpp
double resolution = 0.0;

if (!solveResolutionExpression(resolutionText, resolution, errorMessage))
{
    return false;
}
```

---

# Step 6: Modify `sources/MainWindow.cpp`

Open:

```text
sources/MainWindow.cpp
```

Inside `collectFields()`, find this old block:

```cpp
FieldDefinition field;
field.name = name;
field.byteOffset = byteText.toInt();
field.length = lengthText.toInt();
field.resolution = resolutionText.toDouble();
fields.append(field);
```

Replace it with this:

```cpp
double solvedResolution = 0.0;

if (!InputValidator::solveResolutionExpression(resolutionText, solvedResolution, errorMessage))
{
    errorMessage = QString("Row %1: %2").arg(row + 1).arg(errorMessage);
    return false;
}

FieldDefinition field;
field.name = name;
field.byteOffset = byteText.toInt();
field.length = lengthText.toInt();
field.resolution = solvedResolution;
fields.append(field);
```

This part is required because `validateField()` only validates the expression. This line stores the solved double value into the field:

```cpp
field.resolution = solvedResolution;
```

---

# Updated Technical Logic Section

Add this under **Custom Field Extraction** in `docs/TECHNICAL_LOGIC.md` after the current explanation of `Final Value = Raw Value x Resolution`.

```text
Resolution Expression Handling

The Resolution column accepts mathematical expressions as text.

Examples:
180/2^15
(180 + 20) / (2^15)
360 / pow(2, 16)
sqrt(2) * 0.01
pi / 180

Before extraction starts, the expression is solved once and converted into a double.

Example:
Resolution input = 180/2^15
Solved resolution = 0.0054931640625

Then normal extraction continues:
Final Value = Raw Value x Solved Resolution

This does not change packet reading, UDP parsing, payload extraction, or CSV export logic.
```

---

# Supported Expressions

This evaluator supports:

```text
+
-
*
/
^
()
decimal numbers
scientific notation like 1e-3
constants: pi, e
functions:
sqrt(x)
abs(x)
sin(x)
cos(x)
tan(x)
log(x)
ln(x)
log10(x)
exp(x)
pow(x,y)
min(x,y,...)
max(x,y,...)
floor(x)
ceil(x)
```

Examples that should work:

```text
180/2^15
360/2^16
(180 + 20) / (2^15)
360 / pow(2,16)
sqrt(2) * 0.01
pi / 180
1e-3
max(10,20)/100
```

Examples that should not work:

```text
2pi
180deg
x + 1
```

Use:

```text
2*pi
```

instead of:

```text
2pi
```

---

# Build Steps After Changes

Because new files are added to the `.pro` file, run qmake again.

In terminal:

```bash
qmake PcapUdpExtractor.pro
mingw32-make
```

In Qt Creator:

```text
Build -> Run qmake
Build -> Rebuild Project
```

---

# Quick Validation Test

In the app, use this field:

```text
Field: Angle
Byte: 0
Length: 2
Resolution: 180/2^15
```

If the raw value is:

```text
32768
```

The output should be:

```text
180
```

Reason:

```text
32768 x (180 / 32768) = 180
```

---

# Safety Note

This change is safe and isolated because it only changes how the Resolution text is converted into a double value.

It does not change:

```text
PCAP reading
PCAPNG reading
Ethernet parsing
IPv4 parsing
UDP parsing
payload byte extraction
CSV writing
memory strategy
```

The existing extraction engine continues to use:

```text
Final Value = Raw Value x field.resolution
```

Only `field.resolution` now stores the solved mathematical expression result instead of requiring a plain decimal number.
