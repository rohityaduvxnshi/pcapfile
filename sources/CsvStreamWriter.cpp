#include "CsvStreamWriter.h"

#include <QByteArray>
#include <QLatin1Char>
#include <QLatin1String>

CsvStreamWriter::CsvStreamWriter()
{
}

CsvStreamWriter::~CsvStreamWriter()
{
    close();
}

bool CsvStreamWriter::open(const QString &filePath,
                           const QStringList &fieldHeaders,
                           bool includeSourceColumns,
                           QString &errorOut)
{
    errorOut.clear();

    if (m_file.isOpen())
        close();

    m_includeSourceColumns = includeSourceColumns;
    m_expectedValueCount = fieldHeaders.size();
    m_rowsWritten = 0;
    m_rowsSinceFlush = 0;

    m_file.setFileName(filePath);
    // Do not use QIODevice::Text here. We manually write CRLF (\r\n).
    // In text mode, Qt may translate \n again on Windows and create bad line endings.
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorOut = m_file.errorString();
        return false;
    }

    // Build the header row.
    QStringList header;
    if (m_includeSourceColumns) {
        header << QStringLiteral("TimestampUtc")
               << QStringLiteral("SourceIP")
               << QStringLiteral("SourcePort");
    }
    header += fieldHeaders;

    QStringList escaped;
    for (const QString &h : header)
        escaped << escapeCsv(h);

    const QString line = escaped.join(QLatin1Char(',')) + QLatin1String("\r\n");

    if (!writeLine(line, errorOut)) {
        close();
        return false;
    }

    // Flush the header immediately so an empty/early-stopped capture still
    // leaves a readable CSV header on disk.
    if (!flush(errorOut)) {
        close();
        return false;
    }

    return true;
}

bool CsvStreamWriter::writeRow(const QDateTime &timestampUtc,
                               const QString &sourceIp,
                               quint16 sourcePort,
                               const QStringList &values,
                               QString &errorOut)
{
    errorOut.clear();

    if (!m_file.isOpen()) {
        errorOut = QStringLiteral("CSV file is not open.");
        return false;
    }

    if (m_expectedValueCount >= 0 && values.size() != m_expectedValueCount) {
        errorOut = QStringLiteral("CSV row has %1 values, but header expects %2 values.")
                       .arg(values.size())
                       .arg(m_expectedValueCount);
        return false;
    }

    QStringList cells;

    if (m_includeSourceColumns) {
        const QString ts = timestampUtc.toUTC().toString(Qt::ISODateWithMs);
        cells << escapeCsv(ts);
        cells << escapeCsv(protectFormula(sourceIp));
        cells << escapeCsv(QString::number(sourcePort));
    }

    for (const QString &v : values)
        cells << escapeCsv(protectFormula(v));

    const QString line = cells.join(QLatin1Char(',')) + QLatin1String("\r\n");

    if (!writeLine(line, errorOut))
        return false;

    ++m_rowsWritten;
    ++m_rowsSinceFlush;

    // Periodic flush so a crash loses only the last few rows, not all of them.
    if (m_rowsSinceFlush >= m_flushEveryRows) {
        QString flushErr;
        if (!flush(flushErr)) {
            errorOut = flushErr;
            return false;
        }
    }

    return true;
}

bool CsvStreamWriter::flush(QString &errorOut)
{
    errorOut.clear();

    if (!m_file.isOpen()) {
        errorOut = QStringLiteral("CSV file is not open.");
        return false;
    }

    if (!m_file.flush()) {
        errorOut = m_file.errorString();
        return false;
    }

    m_rowsSinceFlush = 0;
    return true;
}

void CsvStreamWriter::close()
{
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
    m_rowsSinceFlush = 0;
    m_expectedValueCount = -1;
}

bool CsvStreamWriter::isOpen() const
{
    return m_file.isOpen();
}

QString CsvStreamWriter::filePath() const
{
    return m_file.fileName();
}

qint64 CsvStreamWriter::rowsWritten() const
{
    return m_rowsWritten;
}

// --- private helpers --------------------------------------------------------

bool CsvStreamWriter::writeLine(const QString &line, QString &errorOut)
{
    const QByteArray bytes = line.toUtf8();
    const qint64 written = m_file.write(bytes);

    // A short write means disk full or another I/O error.
    if (written != static_cast<qint64>(bytes.size())) {
        errorOut = m_file.errorString();
        if (errorOut.isEmpty()) {
            errorOut = QStringLiteral("Short write to CSV file. Disk may be full or unavailable.");
        }
        return false;
    }
    return true;
}

QString CsvStreamWriter::escapeCsv(const QString &value) const
{
    const bool needsQuoting =
        value.contains(QLatin1Char(',')) ||
        value.contains(QLatin1Char('"')) ||
        value.contains(QLatin1Char('\n')) ||
        value.contains(QLatin1Char('\r'));

    if (!needsQuoting)
        return value;

    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));   // double internal quotes
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString CsvStreamWriter::protectFormula(const QString &value) const
{
    if (value.isEmpty())
        return value;

    const QChar first = value.at(0);
    if (first == QLatin1Char('=') ||
        first == QLatin1Char('+') ||
        first == QLatin1Char('-') ||
        first == QLatin1Char('@')) {
        return QLatin1Char('\'') + value;
    }
    return value;
}
