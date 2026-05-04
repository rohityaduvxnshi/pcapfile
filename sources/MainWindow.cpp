#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "CsvExporter.h"
#include "ExtractionEngine.h"
#include "InputValidator.h"
#include "PcapFileReader.h"
#include "UdpPacketParser.h"

#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->spinPort->setRange(0, 65535);
    ui->spinPort->setValue(5000);

    ui->tblFields->setColumnCount(4);
    ui->tblFields->setHorizontalHeaderLabels(QStringList() << "Field" << "Byte" << "Length" << "Resolution");
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
    ui->tblFields->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tblOutput->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblOutput->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblOutput->horizontalHeader()->setStretchLastSection(true);

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);

    connect(ui->btnBrowse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
    connect(ui->btnAddField, SIGNAL(clicked()), this, SLOT(onAddFieldClicked()));
    connect(ui->btnRemoveField, SIGNAL(clicked()), this, SLOT(onRemoveFieldClicked()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartClicked()));

    onAddFieldClicked();
    setStatus("Ready. Select a PCAP/PCAPNG file, set UDP port, add fields, then click Start.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onBrowseClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(this, "Select Capture File", QString(), "Packet Capture Files (*.pcap *.pcapng);;All Files (*.*)");
    if (!filePath.isEmpty())
    {
        ui->txtFilePath->setText(filePath);
        setStatus("Selected file: " + filePath);
    }
}

void MainWindow::onAddFieldClicked()
{
    const int row = ui->tblFields->rowCount();
    ui->tblFields->insertRow(row);
    ui->tblFields->setItem(row, 0, new QTableWidgetItem(QString("Field%1").arg(row + 1)));
    ui->tblFields->setItem(row, 1, new QTableWidgetItem("0"));
    ui->tblFields->setItem(row, 2, new QTableWidgetItem("2"));
    ui->tblFields->setItem(row, 3, new QTableWidgetItem("1"));
}

void MainWindow::onRemoveFieldClicked()
{
    QList<int> rows;
    QList<QTableWidgetItem*> selectedItems = ui->tblFields->selectedItems();
    for (int i = 0; i < selectedItems.size(); ++i)
    {
        const int row = selectedItems.at(i)->row();
        if (!rows.contains(row))
        {
            rows.append(row);
        }
    }

    while (!rows.isEmpty())
    {
        int maxIndex = 0;
        for (int i = 1; i < rows.size(); ++i)
        {
            if (rows.at(i) > rows.at(maxIndex))
            {
                maxIndex = i;
            }
        }
        ui->tblFields->removeRow(rows.at(maxIndex));
        rows.removeAt(maxIndex);
    }
}

void MainWindow::onStartClicked()
{
    QString errorMessage;

    if (!InputValidator::validateFilePath(ui->txtFilePath->text(), errorMessage))
    {
        QMessageBox::warning(this, "Invalid File", errorMessage);
        return;
    }

    const int port = ui->spinPort->value();
    if (!InputValidator::validatePortValue(port, errorMessage))
    {
        QMessageBox::warning(this, "Invalid Port", errorMessage);
        return;
    }

    QList<FieldDefinition> fields;
    if (!collectFields(fields, errorMessage))
    {
        QMessageBox::warning(this, "Invalid Field", errorMessage);
        return;
    }

    QString csvPath = QFileDialog::getSaveFileName(this, "Save CSV Output", "extracted_udp_data.csv", "CSV Files (*.csv);;All Files (*.*)");
    if (csvPath.isEmpty())
    {
        return;
    }
    if (!csvPath.toLower().endsWith(".csv"))
    {
        csvPath += ".csv";
    }

    const QStringList headers = buildOutputHeaders(fields);
    prepareOutputTable(headers);

    CsvExporter exporter;
    if (!exporter.open(csvPath, headers, errorMessage))
    {
        QMessageBox::critical(this, "CSV Error", errorMessage);
        return;
    }

    PcapFileReader reader;
    if (!reader.open(ui->txtFilePath->text().trimmed(), errorMessage))
    {
        QMessageBox::critical(this, "Read Error", errorMessage);
        return;
    }
    const QString captureFormat = reader.formatName();

    setBusy(true);
    setStatus("Processing capture file...");

    quint64 totalPackets = 0;
    quint64 validUdpPackets = 0;
    quint64 matchedPackets = 0;
    quint64 exportedRows = 0;
    bool failed = false;

    while (true)
    {
        RawPacket rawPacket;
        QString readError;
        const bool hasPacket = reader.readNextPacket(rawPacket, readError);
        if (!hasPacket)
        {
            if (!readError.isEmpty())
            {
                failed = true;
                errorMessage = readError;
            }
            break;
        }

        ++totalPackets;
        ParsedUdpPacket parsed = UdpPacketParser::parsePacket(rawPacket);
        if (!parsed.valid)
        {
            continue;
        }

        ++validUdpPackets;
        if (parsed.sourcePort != port && parsed.destinationPort != port)
        {
            continue;
        }

        ++matchedPackets;
        QStringList row;
        row << QString::number(static_cast<qulonglong>(rawPacket.packetNumber));
        row << parsed.timestamp;
        row << parsed.sourceIp;
        row << parsed.destinationIp;
        row << QString::number(parsed.sourcePort);
        row << QString::number(parsed.destinationPort);
        row << QString::number(parsed.payloadSize);
        row << ExtractionEngine::valuesFromPayload(parsed.udpPayload, fields);

        if (!exporter.writeRow(row, errorMessage))
        {
            failed = true;
            break;
        }

        ++exportedRows;
        if (ui->tblOutput->rowCount() < PREVIEW_ROW_LIMIT)
        {
            appendPreviewRow(row);
        }

        if ((totalPackets % 500) == 0)
        {
            setStatus(QString("Processing... total=%1, UDP=%2, matched=%3")
                          .arg(static_cast<qulonglong>(totalPackets))
                          .arg(static_cast<qulonglong>(validUdpPackets))
                          .arg(static_cast<qulonglong>(matchedPackets)));
            QApplication::processEvents();
        }
    }

    exporter.close();
    reader.close();
    setBusy(false);

    if (failed)
    {
        setStatus("Processing stopped due to error.");
        QMessageBox::critical(this, "Processing Error", errorMessage);
        return;
    }

    const QString summary = QString("Done. Format=%1, total packets=%2, UDP packets=%3, matched packets=%4, exported rows=%5, preview rows=%6")
                                .arg(captureFormat)
                                .arg(static_cast<qulonglong>(totalPackets))
                                .arg(static_cast<qulonglong>(validUdpPackets))
                                .arg(static_cast<qulonglong>(matchedPackets))
                                .arg(static_cast<qulonglong>(exportedRows))
                                .arg(ui->tblOutput->rowCount());

    setStatus(summary);
    QMessageBox::information(this, "Export Complete", summary + "\n\nSaved to:\n" + csvPath);
}

QString MainWindow::tableText(int row, int column) const
{
    QTableWidgetItem* item = ui->tblFields->item(row, column);
    if (!item)
    {
        return QString();
    }
    return item->text().trimmed();
}

bool MainWindow::collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const
{
    fields.clear();
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        const QString name = tableText(row, 0);
        const QString byteText = tableText(row, 1);
        const QString lengthText = tableText(row, 2);
        const QString resolutionText = tableText(row, 3);

        if (name.isEmpty() && byteText.isEmpty() && lengthText.isEmpty() && resolutionText.isEmpty())
        {
            continue;
        }

        if (!InputValidator::validateField(name, byteText, lengthText, resolutionText, errorMessage))
        {
            errorMessage = QString("Row %1: %2").arg(row + 1).arg(errorMessage);
            return false;
        }

        FieldDefinition field;
        field.name = name;
        field.byteOffset = byteText.toInt();
        field.length = lengthText.toInt();
        field.resolution = resolutionText.toDouble();
        fields.append(field);
    }

    if (fields.isEmpty())
    {
        errorMessage = "Add at least one field before starting extraction.";
        return false;
    }

    return InputValidator::validateFields(fields, errorMessage);
}

QStringList MainWindow::buildOutputHeaders(const QList<FieldDefinition>& fields) const
{
    QStringList headers;
    headers << "Packet No" << "Timestamp" << "Source IP" << "Destination IP" << "Source UDP Port" << "Destination UDP Port" << "Payload Size";
    for (int i = 0; i < fields.size(); ++i)
    {
        headers << fields.at(i).name;
    }
    return headers;
}

void MainWindow::prepareOutputTable(const QStringList& headers)
{
    ui->tblOutput->clear();
    ui->tblOutput->setColumnCount(headers.size());
    ui->tblOutput->setHorizontalHeaderLabels(headers);
    ui->tblOutput->setRowCount(0);
}

void MainWindow::appendPreviewRow(const QStringList& row)
{
    const int tableRow = ui->tblOutput->rowCount();
    ui->tblOutput->insertRow(tableRow);
    for (int column = 0; column < row.size(); ++column)
    {
        ui->tblOutput->setItem(tableRow, column, new QTableWidgetItem(row.at(column)));
    }
}

void MainWindow::setBusy(bool busy)
{
    ui->btnStart->setEnabled(!busy);
    ui->btnBrowse->setEnabled(!busy);
    ui->btnAddField->setEnabled(!busy);
    ui->btnRemoveField->setEnabled(!busy);
    ui->spinPort->setEnabled(!busy);
    ui->tblFields->setEnabled(!busy);
    if (busy)
    {
        ui->progressBar->setRange(0, 0);
    }
    else
    {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(100);
    }
}

void MainWindow::setStatus(const QString& message)
{
    ui->lblStatus->setText(message);
}
