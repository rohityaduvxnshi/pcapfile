#ifndef CSVSTREAMWRITER_H
#define CSVSTREAMWRITER_H

// V4 - Live UDP Capture
// Streaming CSV writer for live mode.
// Opens one file, writes the header immediately, appends one row per
// matched datagram, flushes periodically, and closes cleanly.
//
// This class is SEPARATE from CsvExporter on purpose: CsvExporter's
// batch behaviour must stay untouched. This writer only reuses the same
// escaping and formula-injection rules.

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDateTime>

class CsvStreamWriter
{
public:
    CsvStreamWriter();
    ~CsvStreamWriter();

    // Open the file and write the header row.
    // fieldHeaders = the same extracted-field columns used by file mode,
    //                including any bitfield-decoder expansion columns.
    // includeSourceColumns = true adds TimestampUtc, SourceIP, SourcePort
    //                        at the front of every row.
    bool open(const QString &filePath,
              const QStringList &fieldHeaders,
              bool includeSourceColumns,
              QString &errorOut);

    // Append one data row. values must match fieldHeaders order/count.
    bool writeRow(const QDateTime &timestampUtc,
                  const QString &sourceIp,
                  quint16 sourcePort,
                  const QStringList &values,
                  QString &errorOut);

    bool flush(QString &errorOut);
    void close();

    bool isOpen() const;
    QString filePath() const;
    qint64 rowsWritten() const;

private:
    QString escapeCsv(const QString &value) const;
    QString protectFormula(const QString &value) const;
    bool writeLine(const QString &line, QString &errorOut);

private:
    QFile m_file;
    qint64 m_rowsWritten = 0;
    int m_rowsSinceFlush = 0;
    int m_flushEveryRows = 50;
    int m_expectedValueCount = -1;
    bool m_includeSourceColumns = true;
};

#endif // CSVSTREAMWRITER_H
