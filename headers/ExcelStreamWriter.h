#ifndef EXCELSTREAMWRITER_H
#define EXCELSTREAMWRITER_H

// Excel (.xlsx) streaming writer — the Excel counterpart of CsvStreamWriter,
// with an identical open / writeRow / flush / close call shape so the live and
// serial capture paths swap 1:1.
//
// .xlsx is a zipped XML package, so rows cannot be appended on disk one at a
// time: writeRow() accumulates rows in the in-memory workbook and flush()
// performs the real save. stopLiveCapture()/closeLiveMessageWriters() already
// call flush() before close(), which is exactly when the workbook is written.

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace QXlsx
{
class Document;
}

class ExcelStreamWriter
{
public:
    ExcelStreamWriter();
    ~ExcelStreamWriter();

    // Open the workbook and write the header row.
    // includeSourceColumns = true adds TimestampUtc, Source, SourcePort at the
    // front of every row (Source carries an IP in live mode, a COM port or file
    // name in serial mode).
    bool open(const QString &filePath,
              const QStringList &fieldHeaders,
              bool includeSourceColumns,
              QString &errorOut);

    // Append one data row. values must match fieldHeaders order/count.
    bool writeRow(const QDateTime &timestampUtc,
                  const QString &source,
                  quint16 sourcePort,
                  const QStringList &values,
                  QString &errorOut);

    // Saves the workbook to disk (the .xlsx equivalent of a CSV flush).
    bool flush(QString &errorOut);
    void close();

    bool isOpen() const;
    QString filePath() const;
    qint64 rowsWritten() const;

private:
    QXlsx::Document* m_doc;
    QString m_filePath;
    qint64 m_rowsWritten;
    int m_nextRow;             // 1-based next worksheet row
    int m_expectedValueCount;
    bool m_includeSourceColumns;
    bool m_dirty;              // rows added since the last successful save
};

#endif // EXCELSTREAMWRITER_H
