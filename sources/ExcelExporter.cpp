#include "ExcelExporter.h"

#include "xlsxdocument.h"
#include "xlsxformat.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace
{
// Bold header + frozen-feel fill so exported sheets are readable at a glance.
QXlsx::Format headerFormat()
{
    QXlsx::Format fmt;
    fmt.setFontBold(true);
    fmt.setPatternBackgroundColor(QColor(0xE3, 0xF2, 0xFD));
    fmt.setBorderStyle(QXlsx::Format::BorderThin);
    return fmt;
}
}

ExcelExporter::ExcelExporter()
    : m_doc(0),
      m_nextRow(1),
      m_columnCount(0),
      m_open(false)
{
}

ExcelExporter::~ExcelExporter()
{
    close();
}

QVariant ExcelExporter::cellVariant(const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return QVariant(text);

    // Plain decimal/scientific numbers only. Hex (0x..), "N/A", "True", binary
    // strings etc. must stay text.
    static const QRegularExpression numberRe(
        QStringLiteral("^[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?$"));
    if (!numberRe.match(t).hasMatch())
        return QVariant(text);

    // Integers with more than 15 significant digits (e.g. large Uint64 raw
    // values) cannot round-trip through Excel's double cells — keep as text.
    QString digits = t;
    digits.remove(QRegularExpression(QStringLiteral("[+\\-.]")));
    if (!t.contains('e') && !t.contains('E') && digits.size() > 15)
        return QVariant(text);

    bool ok = false;
    const double d = t.toDouble(&ok);
    if (!ok)
        return QVariant(text);
    return QVariant(d);
}

bool ExcelExporter::open(const QString& filePath, const QStringList& headers, QString& errorMessage)
{
    errorMessage.clear();
    close();

    // Probe writability now (the workbook itself is only saved on close) so a
    // bad folder/locked file fails before the extraction loop, like CSV did.
    {
        QFile probe(filePath);
        if (!probe.open(QIODevice::WriteOnly))
        {
            errorMessage = QString("Cannot open '%1' for writing: %2")
                               .arg(filePath).arg(probe.errorString());
            return false;
        }
        probe.close();
    }

    m_doc = new QXlsx::Document();
    m_filePath = filePath;
    m_columnCount = headers.size();
    m_nextRow = 1;
    m_lastError.clear();

    const QXlsx::Format hdrFmt = headerFormat();
    for (int c = 0; c < headers.size(); ++c)
        m_doc->write(1, c + 1, headers.at(c), hdrFmt);
    m_nextRow = 2;

    m_open = true;
    return true;
}

bool ExcelExporter::writeRow(const QStringList& row, QString& errorMessage)
{
    errorMessage.clear();
    if (!m_open || !m_doc)
    {
        errorMessage = "Excel file is not open.";
        return false;
    }

    for (int c = 0; c < row.size(); ++c)
        m_doc->write(m_nextRow, c + 1, cellVariant(row.at(c)));
    ++m_nextRow;
    return true;
}

bool ExcelExporter::finalize(QString& errorMessage)
{
    errorMessage.clear();
    if (!m_open || !m_doc)
        return true;   // nothing pending — same tolerance as CsvExporter::close

    const bool saved = m_doc->saveAs(m_filePath);
    if (!saved)
    {
        errorMessage = QString("Could not save Excel workbook '%1'. "
                               "Is the file open in Excel or the folder read-only?")
                           .arg(m_filePath);
        m_lastError = errorMessage;
    }

    delete m_doc;
    m_doc = 0;
    m_open = false;
    return saved;
}

void ExcelExporter::close()
{
    QString err;
    finalize(err);   // keeps cleanup paths no-throw; failure recorded in lastError()
}

bool ExcelExporter::isOpen() const
{
    return m_open;
}

QString ExcelExporter::lastError() const
{
    return m_lastError;
}
