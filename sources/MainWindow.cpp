#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"
#include "CsvExporter.h"
#include "CsvStreamWriter.h"
#include "ExtractionEngine.h"
#include "InputValidator.h"
#include "LiveUdpReceiver.h"
#include "PcapFileReader.h"
#include "UdpPacketParser.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegExp>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimer>
#include <QVariant>

namespace
{
const int FIELD_COL_NAME = 0;
const int FIELD_COL_BYTE = 1;
const int FIELD_COL_LENGTH = 2;
const int FIELD_COL_RESOLUTION = 3;
const int FIELD_COL_BIT_DECODER = 4;

struct OutputPartition
{
    QString label;
    QString filePath;
    CsvExporter* exporter;
    quint64 exportedRows;

    OutputPartition() : exporter(0), exportedRows(0) {}
};

QString safeName(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) text = "export";
    text.replace(QRegExp("[\\\\/:*?\"<>|]"), "_");
    text.replace(QRegExp("\\s+"), "_");
    return text;
}

QString defaultCsvName(const QString& inputFilePath)
{
    const QFileInfo info(inputFilePath.trimmed());
    return QString("%1_%2_%3.csv")
        .arg(safeName(info.completeBaseName()))
        .arg(QDate::currentDate().toString("yyyyMMdd"))
        .arg(QTime::currentTime().toString("HHmmss"));
}

QString defaultLiveCsvName()
{
    return QString("liveCapture_%1.csv")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

void clearVBox(QVBoxLayout* layout)
{
    while (layout && layout->count() > 0)
    {
        QLayoutItem* item = layout->takeAt(0);
        if (item)
        {
            if (item->widget()) delete item->widget();
            delete item;
        }
    }
}

void closePartitions(QList<OutputPartition>& partitions)
{
    for (int i = 0; i < partitions.size(); ++i)
    {
        if (partitions[i].exporter)
        {
            partitions[i].exporter->close();
            delete partitions[i].exporter;
            partitions[i].exporter = 0;
        }
    }
}

QByteArray fieldBytesFromPayload(const QByteArray& payload, const FieldDefinition& field)
{
    if (field.byteOffset < 0 || field.length <= 0) return QByteArray();
    if (field.byteOffset + field.length > payload.size()) return QByteArray();
    return payload.mid(field.byteOffset, field.length);
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_liveReceiver(0),
      m_livePreviewTimer(0),
      m_liveRunning(false),
      m_livePacketsReceived(0),
      m_livePacketsMatched(0),
      m_liveShortPackets(0)
{
    ui->setupUi(this);

    ui->spinFilterCount->setRange(InputValidator::minMessageFilterCount(), InputValidator::maxMessageFilterCount());
    ui->spinFilterCount->setValue(1);
    ui->spinCommonPort->setRange(0, 65535);
    ui->spinCommonPort->setValue(5000);
    ui->spinLivePort->setRange(1, 65535);
    ui->spinLivePort->setValue(5000);
    ui->radPortFilter->setChecked(true);
    ui->radFileMode->setChecked(true);

    ui->tblFields->setColumnCount(5);
    ui->tblFields->setHorizontalHeaderLabels(QStringList() << "Field" << "Byte" << "Length" << "Resolution" << "Bit Decoder");
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
    ui->tblFields->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tblOutput->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblOutput->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblOutput->horizontalHeader()->setStretchLastSection(true);

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);

    m_liveReceiver = new LiveUdpReceiver(this);
    m_livePreviewTimer = new QTimer(this);
    m_livePreviewTimer->setInterval(250);

    connect(ui->btnBrowse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
    connect(ui->btnAddField, SIGNAL(clicked()), this, SLOT(onAddFieldClicked()));
    connect(ui->btnRemoveField, SIGNAL(clicked()), this, SLOT(onRemoveFieldClicked()));
    connect(ui->btnBitfieldDecoder, SIGNAL(clicked()), this, SLOT(onBitfieldDecoderClicked()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartClicked()));
    connect(ui->spinFilterCount, SIGNAL(valueChanged(int)), this, SLOT(onFilterCountChanged(int)));
    connect(ui->radPortFilter, SIGNAL(toggled(bool)), this, SLOT(onFilterModeChanged()));
    connect(ui->radHeaderFilter, SIGNAL(toggled(bool)), this, SLOT(onFilterModeChanged()));

    connect(ui->radFileMode, SIGNAL(toggled(bool)), this, SLOT(onInputModeChanged()));
    connect(ui->radLiveMode, SIGNAL(toggled(bool)), this, SLOT(onInputModeChanged()));
    connect(ui->btnStartLive, SIGNAL(clicked()), this, SLOT(startLiveCapture()));
    connect(ui->btnStopLive, SIGNAL(clicked()), this, SLOT(stopLiveCapture()));
    connect(m_livePreviewTimer, SIGNAL(timeout()), this, SLOT(refreshLivePreview()));
    connect(m_liveReceiver, SIGNAL(socketError(QString)), this, SLOT(onLiveSocketError(QString)));
    connect(m_liveReceiver,
            SIGNAL(datagramReceived(QByteArray,QHostAddress,quint16,QDateTime)),
            this,
            SLOT(onLiveDatagramReceived(QByteArray,QHostAddress,quint16,QDateTime)));

    rebuildFilterInputs();
    onAddFieldClicked();
    onFilterModeChanged();
    onInputModeChanged();
    setLiveUiState(false);
    setStatus("Ready. Select File Mode or Live Mode, define filters and fields, then start.");
}

MainWindow::~MainWindow()
{
    if (m_liveRunning)
        stopLiveCapture();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_liveRunning)
        stopLiveCapture();
    QMainWindow::closeEvent(event);
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
    ui->tblFields->setItem(row, FIELD_COL_NAME, new QTableWidgetItem(QString("Field%1").arg(row + 1)));
    ui->tblFields->setItem(row, FIELD_COL_BYTE, new QTableWidgetItem("0"));
    ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem("2"));
    ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem("1"));
    QTableWidgetItem* decoderItem = new QTableWidgetItem("No");
    decoderItem->setFlags(decoderItem->flags() & ~Qt::ItemIsEditable);
    ui->tblFields->setItem(row, FIELD_COL_BIT_DECODER, decoderItem);
}

void MainWindow::onRemoveFieldClicked()
{
    QList<int> rows;
    QList<QTableWidgetItem*> selectedItems = ui->tblFields->selectedItems();
    for (int i = 0; i < selectedItems.size(); ++i)
    {
        const int row = selectedItems.at(i)->row();
        if (!rows.contains(row)) rows.append(row);
    }

    while (!rows.isEmpty())
    {
        int maxIndex = 0;
        for (int i = 1; i < rows.size(); ++i)
        {
            if (rows.at(i) > rows.at(maxIndex)) maxIndex = i;
        }
        ui->tblFields->removeRow(rows.at(maxIndex));
        rows.removeAt(maxIndex);
    }
}

void MainWindow::onBitfieldDecoderClicked()
{
    QList<int> rows;
    QList<QTableWidgetItem*> selectedItems = ui->tblFields->selectedItems();
    for (int i = 0; i < selectedItems.size(); ++i)
    {
        const int row = selectedItems.at(i)->row();
        if (!rows.contains(row)) rows.append(row);
    }

    if (rows.isEmpty() && ui->tblFields->currentRow() >= 0)
        rows << ui->tblFields->currentRow();

    if (rows.size() != 1)
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Select exactly one field to configure bitfield decoder rules.");
        return;
    }

    const int row = rows.first();
    const QString fieldName = tableText(row, FIELD_COL_NAME);
    bool lengthOk = false;
    const int fieldLength = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);

    if (fieldName.isEmpty())
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Selected field name cannot be empty.");
        return;
    }

    if (!lengthOk || fieldLength <= 0 || fieldLength > 8)
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Selected field length must be between 1 and 8 bytes.");
        return;
    }

    QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
    if (!nameItem)
    {
        nameItem = new QTableWidgetItem(fieldName);
        ui->tblFields->setItem(row, FIELD_COL_NAME, nameItem);
    }

    QList<BitDecodeRule> existingRules;
    QString error;
    const QString storedJson = nameItem->data(Qt::UserRole).toString();
    if (!storedJson.trimmed().isEmpty() && !BitfieldDecoder::rulesFromJson(storedJson, fieldLength, existingRules, error))
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Existing bitfield decoder data is invalid and will be cleared:\n" + error);
        existingRules.clear();
    }

    BitfieldDecoderDialog dlg(fieldName, fieldLength, existingRules, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        const QList<BitDecodeRule> rules = dlg.rules();
        if (rules.isEmpty())
        {
            nameItem->setData(Qt::UserRole, QVariant());
            QTableWidgetItem* decoderItem = ui->tblFields->item(row, FIELD_COL_BIT_DECODER);
            if (!decoderItem)
            {
                decoderItem = new QTableWidgetItem();
                decoderItem->setFlags(decoderItem->flags() & ~Qt::ItemIsEditable);
                ui->tblFields->setItem(row, FIELD_COL_BIT_DECODER, decoderItem);
            }
            decoderItem->setText("No");
            decoderItem->setToolTip(QString());
        }
        else
        {
            nameItem->setData(Qt::UserRole, BitfieldDecoder::rulesToJson(rules));
            QTableWidgetItem* decoderItem = ui->tblFields->item(row, FIELD_COL_BIT_DECODER);
            if (!decoderItem)
            {
                decoderItem = new QTableWidgetItem();
                decoderItem->setFlags(decoderItem->flags() & ~Qt::ItemIsEditable);
                ui->tblFields->setItem(row, FIELD_COL_BIT_DECODER, decoderItem);
            }
            decoderItem->setText(QString("Yes (%1)").arg(rules.size()));
            decoderItem->setToolTip("Bitfield decoder configured");
        }
    }
}

void MainWindow::onFilterCountChanged(int count)
{
    Q_UNUSED(count);
    rebuildFilterInputs();
}

void MainWindow::onFilterModeChanged()
{
    const bool headerMode = ui->radHeaderFilter->isChecked();
    ui->portFilterPanel->setVisible(!headerMode);
    ui->headerFilterPanel->setVisible(headerMode);
    if (ui->radLiveMode && ui->radLiveMode->isChecked())
        setStatus(headerMode ? "Live Header Filter mode: bind port + payload header prefixes." : "Live Port mode: bind to one UDP port.");
    else
        setStatus(headerMode ? "Header Filter mode: common port + payload header prefixes." : "Port Filter mode: one output CSV per UDP port.");
}

void MainWindow::onInputModeChanged()
{
    const bool liveMode = ui->radLiveMode->isChecked();
    ui->inputGroup->setVisible(!liveMode);
    ui->liveGroup->setVisible(liveMode);

    if (liveMode)
        setStatus("Live Mode selected. Set Bind UDP Port, define fields, then Start Live Capture.");
    else
        setStatus("File Mode selected. Select capture file, set filters, define fields, then Start Export.");

    onFilterModeChanged();
}

void MainWindow::clearPortFilterBoxes()
{
    clearVBox(ui->portFilterBoxLayout);
    m_portFilterBoxes.clear();
}

void MainWindow::clearHeaderFilterBoxes()
{
    clearVBox(ui->headerFilterBoxLayout);
    m_headerFilterBoxes.clear();
}

void MainWindow::rebuildFilterInputs()
{
    const int count = ui->spinFilterCount->value();

    QList<int> oldPorts;
    for (int i = 0; i < m_portFilterBoxes.size(); ++i) oldPorts << m_portFilterBoxes.at(i)->value();

    QStringList oldHeaders;
    for (int i = 0; i < m_headerFilterBoxes.size(); ++i) oldHeaders << m_headerFilterBoxes.at(i)->text();

    clearPortFilterBoxes();
    for (int i = 0; i < count; ++i)
    {
        QWidget* row = new QWidget(ui->portFilterBoxContainer);
        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        QLabel* label = new QLabel(QString("Port Filter %1").arg(i + 1), row);
        QSpinBox* box = new QSpinBox(row);
        box->setRange(0, 65535);
        box->setValue(i < oldPorts.size() ? oldPorts.at(i) : 5000 + i);
        layout->addWidget(label);
        layout->addWidget(box);
        layout->addStretch();
        ui->portFilterBoxLayout->addWidget(row);
        m_portFilterBoxes << box;
    }

    clearHeaderFilterBoxes();
    for (int i = 0; i < count; ++i)
    {
        QWidget* row = new QWidget(ui->headerFilterBoxContainer);
        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        QLabel* label = new QLabel(QString("Header Filter %1").arg(i + 1), row);
        QLineEdit* box = new QLineEdit(row);
        box->setMaxLength(32);
        box->setPlaceholderText("0 to 8 hex chars, e.g. A1B2");
        if (i < oldHeaders.size()) box->setText(oldHeaders.at(i));
        layout->addWidget(label);
        layout->addWidget(box);
        layout->addStretch();
        ui->headerFilterBoxLayout->addWidget(row);
        m_headerFilterBoxes << box;
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

    FilterConfiguration filterConfig;
    if (!collectFilterConfiguration(filterConfig, errorMessage))
    {
        QMessageBox::warning(this, "Invalid Filter", errorMessage);
        return;
    }

    QList<FieldDefinition> fields;
    if (!collectFields(fields, errorMessage))
    {
        QMessageBox::warning(this, "Invalid Field", errorMessage);
        return;
    }

    const QFileInfo inputInfo(ui->txtFilePath->text().trimmed());
    QString baseCsvPath = QFileDialog::getSaveFileName(this, "Choose Base CSV Output Name", inputInfo.absoluteDir().filePath(defaultCsvName(ui->txtFilePath->text())), "CSV Files (*.csv);;All Files (*.*)");
    if (baseCsvPath.isEmpty()) return;
    if (!baseCsvPath.toLower().endsWith(".csv")) baseCsvPath += ".csv";

    const QStringList csvHeaders = buildOutputHeaders(fields);
    prepareOutputTable(buildPreviewHeaders(fields));

    const QString modeText = (filterConfig.mode == FILTER_MODE_PORT) ? "port" : "header";
    QList<OutputPartition> partitions;
    for (int i = 0; i < filterConfig.filters.size(); ++i)
    {
        OutputPartition part;
        part.label = filterConfig.filters.at(i).label;
        part.filePath = buildPartitionCsvPath(baseCsvPath, modeText, part.label);
        part.exporter = new CsvExporter();
        partitions << part;
    }

    for (int i = 0; i < partitions.size(); ++i)
    {
        QString openError;
        if (!partitions[i].exporter->open(partitions[i].filePath, csvHeaders, openError))
        {
            const QString msg = QString("Cannot open output CSV for filter %1:\n%2\n\n%3").arg(partitions.at(i).label).arg(partitions.at(i).filePath).arg(openError);
            closePartitions(partitions);
            QMessageBox::critical(this, "CSV Error", msg);
            return;
        }
    }

    PcapFileReader reader;
    if (!reader.open(ui->txtFilePath->text().trimmed(), errorMessage))
    {
        closePartitions(partitions);
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
        if (!parsed.valid) continue;

        ++validUdpPackets;
        const int partitionIndex = matchingFilterIndex(parsed, filterConfig);
        if (partitionIndex < 0) continue;

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

        OutputPartition& part = partitions[partitionIndex];
        if (!part.exporter->writeRow(row, errorMessage))
        {
            failed = true;
            errorMessage = QString("CSV write failed for filter %1:\n%2\n\n%3").arg(part.label).arg(part.filePath).arg(errorMessage);
            break;
        }

        ++part.exportedRows;
        ++exportedRows;

        if (ui->tblOutput->rowCount() < PREVIEW_ROW_LIMIT)
        {
            QStringList previewRow;
            previewRow << part.label;
            previewRow << row;
            appendPreviewRow(previewRow);
        }

        if ((totalPackets % 500) == 0)
        {
            setStatus(QString("Processing... total=%1, UDP=%2, matched=%3, exported=%4")
                          .arg(static_cast<qulonglong>(totalPackets))
                          .arg(static_cast<qulonglong>(validUdpPackets))
                          .arg(static_cast<qulonglong>(matchedPackets))
                          .arg(static_cast<qulonglong>(exportedRows)));
            QApplication::processEvents();
        }
    }

    closePartitions(partitions);
    reader.close();
    setBusy(false);

    if (failed)
    {
        setStatus("Processing stopped due to error.");
        QMessageBox::critical(this, "Processing Error", errorMessage);
        return;
    }

    QStringList outputLines;
    for (int i = 0; i < partitions.size(); ++i)
    {
        outputLines << QString("%1 %2 -> %3 rows -> %4")
                           .arg(modeText)
                           .arg(partitions.at(i).label)
                           .arg(static_cast<qulonglong>(partitions.at(i).exportedRows))
                           .arg(partitions.at(i).filePath);
    }

    const QString summary = QString("Done. Format=%1, total packets=%2, UDP packets=%3, matched packets=%4, exported rows=%5, preview rows=%6\n\nOutput files:\n%7")
                                .arg(captureFormat)
                                .arg(static_cast<qulonglong>(totalPackets))
                                .arg(static_cast<qulonglong>(validUdpPackets))
                                .arg(static_cast<qulonglong>(matchedPackets))
                                .arg(static_cast<qulonglong>(exportedRows))
                                .arg(ui->tblOutput->rowCount())
                                .arg(outputLines.join("\n"));

    setStatus(QString("Done. Exported rows=%1. Files=%2").arg(static_cast<qulonglong>(exportedRows)).arg(partitions.size()));
    QMessageBox::information(this, "Export Complete", summary);
}

void MainWindow::startLiveCapture()
{
    if (m_liveRunning)
        return;

    QString errorMessage;
    m_liveFields.clear();

    if (!collectFields(m_liveFields, errorMessage))
    {
        QMessageBox::warning(this, "Invalid Field", errorMessage);
        return;
    }

    const int bindPort = ui->spinLivePort->value();
    if (bindPort < 1 || bindPort > 65535)
    {
        QMessageBox::warning(this, "Live Capture", "Bind UDP port must be between 1 and 65535.");
        return;
    }

    if (bindPort < 1024)
    {
        QMessageBox::information(this, "Live Capture", "Ports below 1024 may require administrator/root privileges.");
    }

    m_liveFilterConfig = FilterConfiguration();
    if (ui->radHeaderFilter->isChecked())
    {
        QStringList headers;
        for (int i = 0; i < m_headerFilterBoxes.size(); ++i)
            headers << m_headerFilterBoxes.at(i)->text();

        m_liveFilterConfig.mode = FILTER_MODE_HEADER;
        m_liveFilterConfig.commonPort = bindPort;
        if (!InputValidator::validateHeaderFilters(bindPort, headers, m_liveFilterConfig.filters, errorMessage))
        {
            QMessageBox::warning(this, "Invalid Header Filter", errorMessage);
            return;
        }
    }
    else
    {
        m_liveFilterConfig.mode = FILTER_MODE_PORT;
        m_liveFilterConfig.commonPort = bindPort;
        QList<int> ports;
        ports << bindPort;
        if (!InputValidator::validatePortFilters(ports, m_liveFilterConfig.filters, errorMessage))
        {
            QMessageBox::warning(this, "Invalid Live Port", errorMessage);
            return;
        }
    }

    const QStringList liveHeaders = buildLiveFieldHeaders(m_liveFields);
    QString outputPath = QFileDialog::getSaveFileName(this,
                                                      "Choose Live CSV Output File",
                                                      QDir::current().filePath(defaultLiveCsvName()),
                                                      "CSV Files (*.csv);;All Files (*.*)");
    if (outputPath.isEmpty())
        return;
    if (!outputPath.toLower().endsWith(".csv"))
        outputPath += ".csv";

    QString writerError;
    if (!m_liveWriter.open(outputPath, liveHeaders, true, writerError))
    {
        QMessageBox::critical(this, "Live CSV Error", "Could not open live CSV file:\n" + writerError);
        return;
    }

    QString socketError;
    if (!m_liveReceiver->start(static_cast<quint16>(bindPort), socketError))
    {
        m_liveWriter.close();
        QMessageBox::critical(this, "Live Socket Error", "Could not bind UDP socket:\n" + socketError);
        return;
    }

    m_livePacketsReceived = 0;
    m_livePacketsMatched = 0;
    m_liveShortPackets = 0;
    m_livePreviewRows.clear();
    m_liveRunning = true;

    QStringList previewHeaders;
    previewHeaders << "TimestampUtc" << "SourceIP" << "SourcePort";
    previewHeaders += liveHeaders;
    prepareOutputTable(previewHeaders);

    ui->lblPacketsReceived->setText("0");
    ui->lblPacketsMatched->setText("0");
    ui->lblRowsWritten->setText("0");
    ui->lblShortPackets->setText("0");
    ui->lblLastLiveError->setText("-");
    ui->lblLiveStatus->setText("Listening");

    setLiveUiState(true);
    m_livePreviewTimer->start();
    setStatus(QString("Live capture listening on UDP port %1. Output: %2").arg(bindPort).arg(outputPath));
}

void MainWindow::stopLiveCapture()
{
    if (!m_liveRunning)
        return;

    m_livePreviewTimer->stop();
    m_liveReceiver->stop();

    const QString savedPath = m_liveWriter.filePath();
    const qint64 rows = m_liveWriter.rowsWritten();

    QString flushError;
    const bool flushed = m_liveWriter.isOpen() ? m_liveWriter.flush(flushError) : true;
    m_liveWriter.close();

    m_liveRunning = false;
    setLiveUiState(false);
    ui->lblLiveStatus->setText("Stopped");
    refreshLivePreview();

    if (!flushed)
    {
        QMessageBox::warning(this, "Live Capture",
                             QString("Capture stopped, but final CSV flush failed:\n%1\n\nFile:\n%2\nRows written before close: %3")
                             .arg(flushError).arg(savedPath).arg(rows));
    }
    else
    {
        QMessageBox::information(this, "Live Capture",
                                 QString("Saved:\n%1\nRows written: %2").arg(savedPath).arg(rows));
    }

    setStatus(QString("Live capture stopped. Rows written=%1").arg(rows));
}

void MainWindow::onLiveDatagramReceived(const QByteArray& payload,
                                        const QHostAddress& sender,
                                        quint16 senderPort,
                                        const QDateTime& arrivalTimeUtc)
{
    if (!m_liveRunning)
        return;

    ++m_livePacketsReceived;

    if (!liveHeaderMatches(payload))
        return;

    ++m_livePacketsMatched;

    bool shortPacket = false;
    const QStringList values = extractLiveRowValues(payload, shortPacket);
    if (shortPacket)
        ++m_liveShortPackets;

    QString writeError;
    if (!m_liveWriter.writeRow(arrivalTimeUtc, sender.toString(), senderPort, values, writeError))
    {
        onLiveSocketError("CSV write failed: " + writeError);
        return;
    }

    QStringList previewRow;
    previewRow << arrivalTimeUtc.toUTC().toString(Qt::ISODateWithMs)
               << sender.toString()
               << QString::number(senderPort);
    previewRow += values;

    m_livePreviewRows.append(previewRow);
    while (m_livePreviewRows.size() > LIVE_PREVIEW_ROW_LIMIT)
        m_livePreviewRows.removeFirst();
}

void MainWindow::onLiveSocketError(const QString& message)
{
    ui->lblLastLiveError->setText(message);
    setStatus("Live capture error: " + message);
    if (m_liveRunning)
    {
        QMessageBox::critical(this, "Live Capture Error", message);
        stopLiveCapture();
    }
}

void MainWindow::refreshLivePreview()
{
    ui->lblPacketsReceived->setText(QString::number(static_cast<qulonglong>(m_livePacketsReceived)));
    ui->lblPacketsMatched->setText(QString::number(static_cast<qulonglong>(m_livePacketsMatched)));
    ui->lblRowsWritten->setText(QString::number(static_cast<qlonglong>(m_liveWriter.rowsWritten())));
    ui->lblShortPackets->setText(QString::number(static_cast<qulonglong>(m_liveShortPackets)));

    ui->tblOutput->setRowCount(0);
    for (int i = 0; i < m_livePreviewRows.size(); ++i)
        appendPreviewRow(m_livePreviewRows.at(i));
}

QString MainWindow::tableText(int row, int column) const
{
    QTableWidgetItem* item = ui->tblFields->item(row, column);
    return item ? item->text().trimmed() : QString();
}

bool MainWindow::collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const
{
    fields.clear();
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        const QString name = tableText(row, FIELD_COL_NAME);
        const QString byteText = tableText(row, FIELD_COL_BYTE);
        const QString lengthText = tableText(row, FIELD_COL_LENGTH);
        const QString resolutionText = tableText(row, FIELD_COL_RESOLUTION);

        if (name.isEmpty() && byteText.isEmpty() && lengthText.isEmpty() && resolutionText.isEmpty()) continue;

        if (!InputValidator::validateField(name, byteText, lengthText, resolutionText, errorMessage))
        {
            errorMessage = QString("Row %1: %2").arg(row + 1).arg(errorMessage);
            return false;
        }

        double solvedResolution = 0.0;
        if (!InputValidator::solveResolutionExpression(resolutionText, solvedResolution, errorMessage))
        {
            errorMessage = QString("Row %1: %2").arg(row + 1).arg(errorMessage);
            return false;
        }

        FieldDefinition field;
        field.name = name;
        field.byteOffset = byteText.toInt();
        field.length = lengthText.toInt();
        field.resolution = solvedResolution;

        QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
        if (nameItem)
        {
            const QString storedJson = nameItem->data(Qt::UserRole).toString();
            if (!storedJson.trimmed().isEmpty())
            {
                QList<BitDecodeRule> rules;
                QString ruleError;
                if (!BitfieldDecoder::rulesFromJson(storedJson, field.length, rules, ruleError))
                {
                    errorMessage = QString("Row %1 bitfield decoder error: %2").arg(row + 1).arg(ruleError);
                    return false;
                }
                field.bitDecodeRules = rules;
                field.hasBitfieldDecoder = !rules.isEmpty();
            }
        }

        fields.append(field);
    }

    if (fields.isEmpty())
    {
        errorMessage = "Add at least one field before starting extraction.";
        return false;
    }

    return InputValidator::validateFields(fields, errorMessage);
}

bool MainWindow::collectFilterConfiguration(FilterConfiguration& config, QString& errorMessage) const
{
    config = FilterConfiguration();
    if (!InputValidator::validateMessageFilterCount(ui->spinFilterCount->value(), errorMessage)) return false;

    if (ui->radHeaderFilter->isChecked())
    {
        config.mode = FILTER_MODE_HEADER;
        config.commonPort = ui->spinCommonPort->value();
        QStringList headers;
        for (int i = 0; i < m_headerFilterBoxes.size(); ++i) headers << m_headerFilterBoxes.at(i)->text();
        if (!InputValidator::validateHeaderFilters(config.commonPort, headers, config.filters, errorMessage)) return false;
    }
    else
    {
        config.mode = FILTER_MODE_PORT;
        QList<int> ports;
        for (int i = 0; i < m_portFilterBoxes.size(); ++i) ports << m_portFilterBoxes.at(i)->value();
        if (!InputValidator::validatePortFilters(ports, config.filters, errorMessage)) return false;
    }

    return InputValidator::validateFilterConfiguration(config, errorMessage);
}

QStringList MainWindow::buildOutputHeaders(const QList<FieldDefinition>& fields) const
{
    QStringList headers;
    headers << "Packet No" << "Timestamp" << "Source IP" << "Destination IP" << "Source UDP Port" << "Destination UDP Port" << "Payload Size";
    headers += buildLiveFieldHeaders(fields);
    return headers;
}

QStringList MainWindow::buildLiveFieldHeaders(const QList<FieldDefinition>& fields) const
{
    QStringList headers;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& field = fields.at(i);
        headers << field.name;
        if (field.hasBitfieldDecoder)
        {
            for (int r = 0; r < field.bitDecodeRules.size(); ++r)
            {
                const BitDecodeRule& rule = field.bitDecodeRules.at(r);
                headers << QString("%1_%2").arg(field.name).arg(BitfieldDecoder::sanitizeColumnLabel(rule.label));
            }
        }
    }
    return headers;
}

QStringList MainWindow::buildPreviewHeaders(const QList<FieldDefinition>& fields) const
{
    QStringList headers;
    headers << "Filter";
    headers += buildOutputHeaders(fields);
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

int MainWindow::matchingFilterIndex(const ParsedUdpPacket& parsed, const FilterConfiguration& config) const
{
    if (config.mode == FILTER_MODE_PORT)
    {
        for (int i = 0; i < config.filters.size(); ++i)
        {
            const int port = config.filters.at(i).port;
            if (parsed.sourcePort == port || parsed.destinationPort == port) return i;
        }
        return -1;
    }

    if (parsed.sourcePort != config.commonPort && parsed.destinationPort != config.commonPort) return -1;

    for (int i = 0; i < config.filters.size(); ++i)
    {
        const QByteArray header = config.filters.at(i).header;
        if (header.isEmpty()) return i;
        if (parsed.udpPayload.size() >= header.size() && parsed.udpPayload.left(header.size()) == header) return i;
    }

    return -1;
}

bool MainWindow::liveHeaderMatches(const QByteArray& payload) const
{
    if (m_liveFilterConfig.mode != FILTER_MODE_HEADER)
        return true;

    for (int i = 0; i < m_liveFilterConfig.filters.size(); ++i)
    {
        const QByteArray header = m_liveFilterConfig.filters.at(i).header;
        if (header.isEmpty()) return true;
        if (payload.size() >= header.size() && payload.left(header.size()) == header) return true;
    }
    return false;
}

QStringList MainWindow::extractLiveRowValues(const QByteArray& payload, bool& shortPacket) const
{
    QStringList values;
    shortPacket = false;

    for (int i = 0; i < m_liveFields.size(); ++i)
    {
        const FieldDefinition& field = m_liveFields.at(i);
        const bool isShort = (field.byteOffset < 0 || field.length <= 0 || field.byteOffset + field.length > payload.size());

        if (isShort)
        {
            shortPacket = true;
            values << "SHORT_PACKET";
            if (field.hasBitfieldDecoder)
            {
                for (int r = 0; r < field.bitDecodeRules.size(); ++r)
                    values << "SHORT_PACKET";
            }
            continue;
        }

        values << ExtractionEngine::valueFromPayload(payload, field);

        if (field.hasBitfieldDecoder)
        {
            const QByteArray fieldBytes = fieldBytesFromPayload(payload, field);
            for (int r = 0; r < field.bitDecodeRules.size(); ++r)
                values << BitfieldDecoder::decodeRule(fieldBytes, field.bitDecodeRules.at(r));
        }
    }

    return values;
}

QString MainWindow::buildPartitionCsvPath(const QString& baseCsvPath, const QString& modeText, const QString& filterLabel) const
{
    const QFileInfo info(baseCsvPath);
    return info.absoluteDir().filePath(QString("%1_%2_%3.csv").arg(safeName(info.completeBaseName())).arg(modeText).arg(safeName(filterLabel)));
}

void MainWindow::setBusy(bool busy)
{
    ui->btnStart->setEnabled(!busy);
    ui->btnBrowse->setEnabled(!busy);
    ui->btnStartLive->setEnabled(!busy && ui->radLiveMode->isChecked() && !m_liveRunning);
    ui->btnStopLive->setEnabled(m_liveRunning);
    ui->btnAddField->setEnabled(!busy);
    ui->btnRemoveField->setEnabled(!busy);
    ui->btnBitfieldDecoder->setEnabled(!busy);
    ui->spinFilterCount->setEnabled(!busy);
    ui->radPortFilter->setEnabled(!busy);
    ui->radHeaderFilter->setEnabled(!busy);
    ui->portFilterPanel->setEnabled(!busy);
    ui->headerFilterPanel->setEnabled(!busy);
    ui->tblFields->setEnabled(!busy);
    ui->radFileMode->setEnabled(!busy);
    ui->radLiveMode->setEnabled(!busy);
    ui->spinLivePort->setEnabled(!busy);

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

void MainWindow::setLiveUiState(bool running)
{
    ui->btnStartLive->setEnabled(!running);
    ui->btnStopLive->setEnabled(running);
    ui->radFileMode->setEnabled(!running);
    ui->radLiveMode->setEnabled(!running);
    ui->spinLivePort->setEnabled(!running);
    ui->spinFilterCount->setEnabled(!running);
    ui->radPortFilter->setEnabled(!running);
    ui->radHeaderFilter->setEnabled(!running);
    ui->portFilterPanel->setEnabled(!running);
    ui->headerFilterPanel->setEnabled(!running);
    ui->btnAddField->setEnabled(!running);
    ui->btnRemoveField->setEnabled(!running);
    ui->btnBitfieldDecoder->setEnabled(!running);
    ui->tblFields->setEnabled(!running);
    ui->btnBrowse->setEnabled(!running);
    ui->btnStart->setEnabled(!running && ui->radFileMode->isChecked());
}

void MainWindow::setStatus(const QString& message)
{
    ui->lblStatus->setText(message);
}
