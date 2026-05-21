#include "CsvExporter.h"

#include <QByteArray>

namespace
{
// Single-pass UTF-8 row-cell writer that produces byte-identical output to
// CsvExporter::escapeCell(CsvExporter::safeCell(cell)) followed by toUtf8(),
// but appends straight into a reusable QByteArray with no QStringList/join.
void appendEscapedCellUtf8(QByteArray& buf, const QString& cell)
{
    // 1. Formula-protection prefix (mirrors safeCell): prepend '\'' if the
    //    first character is '=', '+', '-' or '@'. The apostrophe itself
    //    (ASCII 39) is never an escape trigger.
    bool prependApostrophe = false;
    if (!cell.isEmpty())
    {
        const QChar first = cell.at(0);
        if (first == QChar('=') || first == QChar('+')
            || first == QChar('-') || first == QChar(64))
        {
            prependApostrophe = true;
        }
    }

    // 2. Quote-need detection + internal-quote detection (mirrors escapeCell).
    bool needsQuote = false;
    bool hasInternalQuote = false;
    for (int i = 0; i < cell.size(); ++i)
    {
        const ushort u = cell.at(i).unicode();
        if (u == ',' || u == '\n' || u == '\r')
        {
            needsQuote = true;
        }
        else if (u == '"')
        {
            needsQuote = true;
            hasInternalQuote = true;
        }
    }

    if (needsQuote)
    {
        buf.append('"');
        if (prependApostrophe)
            buf.append('\'');
        if (hasInternalQuote)
        {
            QString tmp = cell;
            tmp.replace(QLatin1Char('"'), QLatin1String("\"\""));
            buf.append(tmp.toUtf8());
        }
        else
        {
            buf.append(cell.toUtf8());
        }
        buf.append('"');
    }
    else
    {
        if (prependApostrophe)
            buf.append('\'');
        buf.append(cell.toUtf8());
    }
}
}

CsvExporter::CsvExporter()
{
}

CsvExporter::~CsvExporter()
{
    close();
}

bool CsvExporter::open(const QString& filePath, const QStringList& headers, QString& errorMessage)
{
    close();
    errorMessage.clear();

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        errorMessage = "Cannot open CSV file for writing.";
        return false;
    }

    m_stream.setDevice(&m_file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_stream.setCodec("UTF-8");
#endif

    return writeRow(headers, errorMessage);
}

bool CsvExporter::writeRow(const QStringList& row, QString& errorMessage)
{
    if (!m_file.isOpen())
    {
        errorMessage = "CSV file is not open.";
        return false;
    }

    // Reusable buffer keeps per-row allocation cost amortized. CsvExporter is
    // only used from the main thread (offline export path); the live path uses
    // CsvStreamWriter. resize(0) preserves capacity, so growth is bounded by
    // the widest row written.
    static QByteArray buf;
    buf.resize(0);

    for (int i = 0; i < row.size(); ++i)
    {
        if (i > 0)
            buf.append(',');
        appendEscapedCellUtf8(buf, row.at(i));
    }
    // The file is opened with QIODevice::Text, so the trailing '\n' is
    // translated to "\r\n" on Windows and left as "\n" elsewhere — byte
    // output on disk matches the previous QTextStream-based code exactly.
    buf.append('\n');

    const qint64 written = m_file.write(buf);
    if (written < 0 || written < static_cast<qint64>(buf.size()))
    {
        errorMessage = "Failed while writing CSV row.";
        return false;
    }

    return true;
}

void CsvExporter::close()
{
    if (m_file.isOpen())
    {
        m_stream.flush();
        m_stream.setDevice(0);
        m_file.close();
    }
}

bool CsvExporter::isOpen() const
{
    return m_file.isOpen();
}

QString CsvExporter::safeCell(QString cell)
{
    if (!cell.isEmpty())
    {
        const QChar first = cell.at(0);
        if (first == QChar('=') || first == QChar('+') || first == QChar('-') || first == QChar(64))
        {
            cell.prepend(QChar(39));
        }
    }

    return cell;
}

QString CsvExporter::escapeCell(QString cell)
{
    const bool quoted = cell.contains(',') || cell.contains('"') || cell.contains('\n') || cell.contains('\r');
    cell.replace("\"", "\"\"");

    if (quoted)
    {
        cell.prepend(QChar('"'));
        cell.append(QChar('"'));
    }

    return cell;
}
