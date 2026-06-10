#ifndef EXCELEXPORTER_H
#define EXCELEXPORTER_H

// Excel (.xlsx) batch exporter — the Excel counterpart of CsvExporter, with the
// same open / writeRow / close call shape so export call sites swap 1:1.
//
// Backed by the vendored QXlsx library (third_party/QXlsx, MIT). Unlike CSV,
// an .xlsx file cannot be appended on disk row-by-row: rows accumulate in the
// in-memory workbook and the file is written ONCE when close()/finalize() runs.
// open() still probes the path immediately so "cannot write there" errors
// surface before a long extraction, exactly like the CSV flow did.

#include <QString>
#include <QStringList>
#include <QVariant>

namespace QXlsx
{
class Document;
}

class ExcelExporter
{
public:
    ExcelExporter();
    ~ExcelExporter();

    bool open(const QString& filePath, const QStringList& headers, QString& errorMessage);
    bool writeRow(const QStringList& row, QString& errorMessage);

    // Saves the workbook and closes. Returns false (with errorMessage) when the
    // final save fails — callers that previously trusted CSV close() should use
    // this in their success path.
    bool finalize(QString& errorMessage);

    // CsvExporter-compatible close. Performs the same save as finalize(); a save
    // failure is kept in lastError() so cleanup paths stay no-throw.
    void close();
    bool isOpen() const;
    QString lastError() const;

    // Shared cell typing rule for all Excel writers: plain decimal numbers become
    // real Excel numbers (so the sheet is immediately sortable/chartable) while
    // hex strings, flags, text, and integers longer than 15 digits (which Excel
    // doubles cannot represent exactly) stay text.
    static QVariant cellVariant(const QString& text);

private:
    QXlsx::Document* m_doc;
    QString m_filePath;
    QString m_lastError;
    int m_nextRow;        // 1-based next worksheet row to write
    int m_columnCount;
    bool m_open;
};

#endif // EXCELEXPORTER_H
