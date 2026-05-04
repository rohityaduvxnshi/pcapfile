#ifndef CSVEXPORTER_H
#define CSVEXPORTER_H

#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

class CsvExporter
{
public:
    CsvExporter();
    ~CsvExporter();

    bool open(const QString& filePath, const QStringList& headers, QString& errorMessage);
    bool writeRow(const QStringList& row, QString& errorMessage);
    void close();
    bool isOpen() const;

private:
    static QString safeCell(QString cell);
    static QString escapeCell(QString cell);

    QFile m_file;
    QTextStream m_stream;
};

#endif
