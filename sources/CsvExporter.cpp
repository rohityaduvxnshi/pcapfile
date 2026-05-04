#include "CsvExporter.h"

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

    QStringList cells;
    for (int i = 0; i < row.size(); ++i)
    {
        cells << escapeCell(safeCell(row.at(i)));
    }

    m_stream << cells.join(',') << "\n";
    if (m_stream.status() != QTextStream::Ok)
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
        cell = QString('"') + cell + QString('"');
    }

    return cell;
}
