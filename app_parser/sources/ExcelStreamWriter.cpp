#include "ExcelStreamWriter.h"

#include "ExcelExporter.h"

#include "xlsxdocument.h"
#include "xlsxformat.h"

#include <QFile>

ExcelStreamWriter::ExcelStreamWriter()
    : m_doc(0),
      m_rowsWritten(0),
      m_nextRow(1),
      m_expectedValueCount(-1),
      m_includeSourceColumns(true),
      m_dirty(false)
{
}

ExcelStreamWriter::~ExcelStreamWriter()
{
    close();
}

bool ExcelStreamWriter::open(const QString &filePath,
                             const QStringList &fieldHeaders,
                             bool includeSourceColumns,
                             QString &errorOut)
{
    errorOut.clear();
    close();

    // Probe writability immediately so bind-time errors surface before capture
    // starts (the workbook itself is only saved on flush/close).
    {
        QFile probe(filePath);
        if (!probe.open(QIODevice::WriteOnly))
        {
            errorOut = QString("Cannot open '%1' for writing: %2")
                           .arg(filePath).arg(probe.errorString());
            return false;
        }
        probe.close();
    }

    m_doc = new QXlsx::Document();
    m_filePath = filePath;
    m_includeSourceColumns = includeSourceColumns;
    m_expectedValueCount = fieldHeaders.size();
    m_rowsWritten = 0;
    m_dirty = false;

    QStringList headers;
    if (includeSourceColumns)
        headers << "TimestampUtc" << "Source" << "SourcePort";
    headers += fieldHeaders;

    QXlsx::Format hdrFmt;
    hdrFmt.setFontBold(true);
    hdrFmt.setPatternBackgroundColor(QColor(0xE3, 0xF2, 0xFD));
    hdrFmt.setBorderStyle(QXlsx::Format::BorderThin);
    for (int c = 0; c < headers.size(); ++c)
        m_doc->write(1, c + 1, headers.at(c), hdrFmt);
    m_nextRow = 2;
    m_dirty = true;   // header row is pending until the first save
    return true;
}

bool ExcelStreamWriter::writeRow(const QDateTime &timestampUtc,
                                 const QString &source,
                                 quint16 sourcePort,
                                 const QStringList &values,
                                 QString &errorOut)
{
    errorOut.clear();
    if (!m_doc)
    {
        errorOut = "Excel file is not open.";
        return false;
    }
    if (m_expectedValueCount >= 0 && values.size() != m_expectedValueCount)
    {
        errorOut = QString("Excel row has %1 values, but header expects %2 values.")
                       .arg(values.size())
                       .arg(m_expectedValueCount);
        return false;
    }

    int col = 1;
    if (m_includeSourceColumns)
    {
        m_doc->write(m_nextRow, col++, timestampUtc.toUTC().toString(Qt::ISODateWithMs));
        m_doc->write(m_nextRow, col++, source);
        m_doc->write(m_nextRow, col++, static_cast<int>(sourcePort));
    }
    for (int i = 0; i < values.size(); ++i)
        m_doc->write(m_nextRow, col++, ExcelExporter::cellVariant(values.at(i)));

    ++m_nextRow;
    ++m_rowsWritten;
    m_dirty = true;
    return true;
}

bool ExcelStreamWriter::flush(QString &errorOut)
{
    errorOut.clear();
    if (!m_doc)
        return true;
    if (!m_dirty)
        return true;

    if (!m_doc->saveAs(m_filePath))
    {
        errorOut = QString("Could not save Excel workbook '%1'. "
                           "Is the file open in Excel or the folder read-only?")
                       .arg(m_filePath);
        return false;
    }
    m_dirty = false;
    return true;
}

void ExcelStreamWriter::close()
{
    if (m_doc)
    {
        if (m_dirty)
        {
            QString err;
            flush(err);   // best effort; stop paths already flush() and report first
        }
        delete m_doc;
        m_doc = 0;
    }
    m_expectedValueCount = -1;
    m_nextRow = 1;
    m_dirty = false;
    // m_filePath and m_rowsWritten survive close() on purpose: stop paths read
    // both after closing for the per-file summary.
}

bool ExcelStreamWriter::isOpen() const
{
    return m_doc != 0;
}

QString ExcelStreamWriter::filePath() const
{
    return m_filePath;
}

qint64 ExcelStreamWriter::rowsWritten() const
{
    return m_rowsWritten;
}
