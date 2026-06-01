#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "BitfieldDecoder.h"
#include "ConditionalBitfieldDecoder.h"
#include "CsvExporter.h"
#include "CsvStreamWriter.h"
#include "ExtractionEngine.h"
#include "FieldConfigurationDialog.h"
#include "InputValidator.h"
#include "LiveUdpReceiver.h"
#include "MessageLengthFilterDialog.h"
#include "PcapFileReader.h"
#include "ProjectFile.h"
#include "Themes.h"
#include "UdpPacketParser.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCloseEvent>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
const int PORT_COL_PORT = 0;
const int PORT_COL_MANAGE = 1;
const int PORT_COL_COUNT = 2;

const int MESSAGE_COL_NAME = 0;
const int MESSAGE_COL_LENGTH = 1;
const int MESSAGE_COL_PORT = 2;
const int MESSAGE_COL_FIELDS = 3;
const int MESSAGE_COL_CONFIGURE = 4;

// Live-preview render bookkeeping. s_livePreviewAppendSeq is bumped every
// time onLiveDatagramReceived() appends to m_livePreviewRows. refreshLivePreview()
// uses the delta against s_liveRenderedSeq to append only what is new, instead
// of clearing and rebuilding the entire table every 250 ms. Both are reset
// to zero in startLiveCapture() before listening begins.
qint64 s_livePreviewAppendSeq = 0;
qint64 s_liveRenderedSeq = 0;

struct OutputPartition
{
    QString label;
    QString filePath;
    CsvExporter* exporter;
    quint64 exportedRows;

    OutputPartition() : exporter(0), exportedRows(0) {}
};

struct MessageOutputPartition
{
    MessageDefinition definition;
    QString filePath;
    CsvExporter* exporter;
    quint64 exportedRows;

    MessageOutputPartition() : exporter(0), exportedRows(0) {}
};

QString safeName(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) text = "export";
    text.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    text.replace(QRegularExpression("\\s+"), "_");
    text.replace(QRegularExpression("_+"), "_");
    while (text.startsWith('_')) text.remove(0, 1);
    while (text.endsWith('_')) text.chop(1);
    if (text.isEmpty()) text = "export";
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

void closeMessagePartitions(QList<MessageOutputPartition>& partitions)
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
    if (field.byteOffsetcorrect < 0 || field.length <= 0) return QByteArray();
    if (field.byteOffsetcorrect + field.length > payload.size()) return QByteArray();
    return payload.mid(field.byteOffsetcorrect, field.length);
}

bool packetMatchesMessage(const ParsedUdpPacket& parsed, const MessageDefinition& message)
{
    if (parsed.sourcePort != message.port && parsed.destinationPort != message.port)
        return false;

    if (parsed.udpPayload.size() != message.payloadLengthBytes)
        return false;

    // v12: when an optional header signature is configured, the leading bytes of the
    // UDP payload must match. Empty header = no extra check (pre-v12 behaviour).
    if (!message.optionalHeader.isEmpty())
    {
        if (parsed.udpPayload.size() < message.optionalHeader.size())
            return false;
        if (parsed.udpPayload.left(message.optionalHeader.size()) != message.optionalHeader)
            return false;
    }

    return true;
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
    Themes::apply(this);

    ui->spinFilterCount->setRange(InputValidator::minMessageFilterCount(), InputValidator::maxMessageFilterCount());
    ui->spinFilterCount->setValue(1);
    ui->spinCommonPort->setRange(0, 65535);
    ui->spinCommonPort->setValue(5000);
    ui->spinLivePort->setRange(1, 65535);
    ui->spinLivePort->setValue(5000);
    ui->radPortFilter->setChecked(true);
    ui->radFileMode->setChecked(true);

    ui->tblPortFilters->setColumnCount(3);
    ui->tblPortFilters->setHorizontalHeaderLabels(QStringList() << "Port" << "Manage Length Filters" << "Message Count");
    ui->tblPortFilters->horizontalHeader()->setStretchLastSection(true);
    ui->tblPortFilters->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblPortFilters->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblPortFilters->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tblConfiguredMessages->setColumnCount(5);
    ui->tblConfiguredMessages->setHorizontalHeaderLabels(QStringList() << "Message Name" << "Payload Length" << "Port" << "Fields" << "Configure Fields");
    ui->tblConfiguredMessages->horizontalHeader()->setStretchLastSection(true);
    ui->tblConfiguredMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblConfiguredMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblConfiguredMessages->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // v13: live-mode configured-messages table (mirrors tblConfiguredMessages but
    // backed by m_liveMessages, with an Optional Header column for v12 disambiguation).
    ui->tblLiveConfiguredMessages->setColumnCount(5);
    ui->tblLiveConfiguredMessages->setHorizontalHeaderLabels(QStringList()
        << "Message Name" << "Payload Length" << "Optional Header" << "Fields" << "Configure Fields");
    ui->tblLiveConfiguredMessages->horizontalHeader()->setStretchLastSection(true);
    ui->tblLiveConfiguredMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblLiveConfiguredMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblLiveConfiguredMessages->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tblOutput->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblOutput->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblOutput->horizontalHeader()->setStretchLastSection(true);

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);

    m_headerFields = defaultFields();
    m_liveFields = defaultFields();

    m_liveReceiver = new LiveUdpReceiver(this);
    m_livePreviewTimer = new QTimer(this);
    m_livePreviewTimer->setInterval(250);

    connect(ui->btnBrowse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartClicked()));
    connect(ui->spinFilterCount, SIGNAL(valueChanged(int)), this, SLOT(onFilterCountChanged(int)));
    connect(ui->radPortFilter, SIGNAL(toggled(bool)), this, SLOT(onFilterModeChanged()));
    connect(ui->radHeaderFilter, SIGNAL(toggled(bool)), this, SLOT(onFilterModeChanged()));
    connect(ui->btnConfigureHeaderFields, SIGNAL(clicked()), this, SLOT(onConfigureHeaderFieldsClicked()));
    // v13: btnConfigureLiveFields removed in v13. The slot stays defined for project-file
    // backward compatibility but is no longer wired to any widget.

    connect(ui->actOpenProject,   SIGNAL(triggered()), this, SLOT(onOpenProject()));
    connect(ui->actSaveProject,   SIGNAL(triggered()), this, SLOT(onSaveProject()));
    connect(ui->actSaveProjectAs, SIGNAL(triggered()), this, SLOT(onSaveProjectAs()));

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
    refreshStandaloneFieldStatus();
    onFilterModeChanged();
    onInputModeChanged();
    setLiveUiState(false);

    // v12: theme toggle button + live mode length-filter affordance.
    ui->btnToggleTheme->setText(Themes::currentMode() == Themes::Dark ? "Light Theme" : "Dark Theme");
    connect(ui->btnToggleTheme, SIGNAL(clicked()), this, SLOT(onToggleThemeClicked()));
    connect(ui->btnManageLiveLengthFilters, SIGNAL(clicked()), this, SLOT(onManageLiveLengthFiltersClicked()));
    refreshLiveLengthFilterStatus();

    // v13: initial empty render of the live configured-messages table.
    refreshLiveConfiguredMessagesTable();

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
    autoSaveProjectOnClose();
    QMainWindow::closeEvent(event);
}

void MainWindow::onBrowseClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(this, "Select Capture File", QString(), "Packet Capture Files (*.pcap *.pcapng);;All Files (*.*)");
    if (!filePath.isEmpty())
    {
        ui->txtFilePath->setText(filePath);
        setStatus("Selected file: " + filePath);
        tryRestoreProjectForPcap(filePath);
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
    ui->configuredMessagesGroup->setVisible(!headerMode && ui->radFileMode->isChecked());

    if (ui->radLiveMode && ui->radLiveMode->isChecked())
        setStatus(headerMode ? "Live Header Filter mode selected." : "Live Port mode selected.");
    else
        setStatus(headerMode ? "Header Filter mode selected." : "Port Filter mode: configure length filters per UDP port.");
}

void MainWindow::onInputModeChanged()
{
    const bool liveMode = ui->radLiveMode->isChecked();
    ui->inputGroup->setVisible(!liveMode);
    ui->liveGroup->setVisible(liveMode);
    // v13: Message Filters group is irrelevant in Live Mode (single bind port +
    // optional headers in length filters handle disambiguation). Show the live-mode
    // configured-messages table in its place.
    ui->filterGroup->setVisible(!liveMode);
    ui->liveConfiguredMessagesGroup->setVisible(liveMode);

    if (liveMode)
        setStatus("Live Mode selected.");
    else
        setStatus("File Mode selected.");

    onFilterModeChanged();
}

void MainWindow::onPortValueChanged(int value)
{
    Q_UNUSED(value);

    QObject* object = sender();
    const int row = object ? object->property("portRow").toInt() : -1;
    if (row < 0 || row >= m_portMessagesByRow.size() || row >= m_portFilterBoxes.size())
        return;

    const quint16 port = static_cast<quint16>(m_portFilterBoxes.at(row)->value());
    for (int i = 0; i < m_portMessagesByRow[row].size(); ++i)
        m_portMessagesByRow[row][i].port = port;

    refreshPortFilterTable();
    refreshConfiguredMessagesTable();
}

void MainWindow::onManageLengthFiltersClicked()
{
    QObject* object = sender();
    const int row = object ? object->property("portRow").toInt() : -1;
    openLengthFilterDialogForPortRow(row);
}

void MainWindow::onConfigureMessageFieldsClicked()
{
    QObject* object = sender();
    const int messageIndex = object ? object->property("messageIndex").toInt() : -1;
    openFieldConfigurationForMessage(messageIndex);
}

void MainWindow::onConfigureHeaderFieldsClicked()
{
    if (configureFieldList(m_headerFields, 0, "Header Mode Fields"))
        refreshStandaloneFieldStatus();
}

void MainWindow::onConfigureLiveFieldsClicked()
{
    if (configureFieldList(m_liveFields, 0, "Live Capture Fields"))
        refreshStandaloneFieldStatus();
}

void MainWindow::clearPortFilterBoxes()
{
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
    for (int i = 0; i < m_portFilterBoxes.size(); ++i)
        oldPorts << m_portFilterBoxes.at(i)->value();

    QList< QList<MessageDefinition> > oldMessages = m_portMessagesByRow;

    clearPortFilterBoxes();
    m_portMessagesByRow.clear();
    ui->tblPortFilters->setRowCount(0);

    for (int i = 0; i < count; ++i)
    {
        QList<MessageDefinition> rowMessages;
        if (i < oldMessages.size())
            rowMessages = oldMessages.at(i);

        m_portMessagesByRow << rowMessages;

        const int row = ui->tblPortFilters->rowCount();
        ui->tblPortFilters->insertRow(row);

        QSpinBox* box = new QSpinBox(ui->tblPortFilters);
        box->setRange(1, 65535);
        box->setValue(i < oldPorts.size() ? oldPorts.at(i) : 5000 + i);
        box->setProperty("portRow", row);
        connect(box, SIGNAL(valueChanged(int)), this, SLOT(onPortValueChanged(int)));
        ui->tblPortFilters->setCellWidget(row, PORT_COL_PORT, box);
        m_portFilterBoxes << box;

        QPushButton* manageButton = new QPushButton("Manage Length Filters", ui->tblPortFilters);
        manageButton->setProperty("portRow", row);
        connect(manageButton, SIGNAL(clicked()), this, SLOT(onManageLengthFiltersClicked()));
        ui->tblPortFilters->setCellWidget(row, PORT_COL_MANAGE, manageButton);

        QTableWidgetItem* countItem = new QTableWidgetItem("0");
        countItem->setFlags(countItem->flags() & ~Qt::ItemIsEditable);
        ui->tblPortFilters->setItem(row, PORT_COL_COUNT, countItem);
    }

    QStringList oldHeaders;
    for (int i = 0; i < m_headerFilterBoxes.size(); ++i)
        oldHeaders << m_headerFilterBoxes.at(i)->text();

    // v12: preserve per-row header-mode messages across filter-count edits.
    const QList< QList<MessageDefinition> > oldHeaderMessages = m_headerMessagesByRow;
    m_headerMessagesByRow.clear();
    m_headerLengthFilterButtons.clear();

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
        // v12: per-header-row "Manage Length Filters" button + status label.
        QPushButton* lenBtn = new QPushButton("Manage Length Filters", row);
        lenBtn->setProperty("headerRow", i);
        lenBtn->setToolTip("Optional: define per-message length filters scoped to this header row.");
        connect(lenBtn, SIGNAL(clicked()), this, SLOT(onManageHeaderLengthFiltersClicked()));
        layout->addWidget(lenBtn);
        QLabel* lenStatus = new QLabel("No length filters", row);
        lenStatus->setObjectName("lblHeaderLengthFilterStatus");
        layout->addWidget(lenStatus);
        layout->addStretch();
        ui->headerFilterBoxLayout->addWidget(row);
        m_headerFilterBoxes << box;
        m_headerLengthFilterButtons << lenBtn;
        m_headerMessagesByRow << (i < oldHeaderMessages.size() ? oldHeaderMessages.at(i) : QList<MessageDefinition>());
    }

    refreshPortFilterTable();
    refreshConfiguredMessagesTable();
    refreshHeaderLengthFilterStatus();
}

void MainWindow::refreshPortFilterTable()
{
    for (int row = 0; row < m_portFilterBoxes.size() && row < m_portMessagesByRow.size(); ++row)
    {
        const quint16 port = static_cast<quint16>(m_portFilterBoxes.at(row)->value());
        for (int i = 0; i < m_portMessagesByRow[row].size(); ++i)
            m_portMessagesByRow[row][i].port = port;

        QTableWidgetItem* countItem = ui->tblPortFilters->item(row, PORT_COL_COUNT);
        if (!countItem)
        {
            countItem = new QTableWidgetItem();
            countItem->setFlags(countItem->flags() & ~Qt::ItemIsEditable);
            ui->tblPortFilters->setItem(row, PORT_COL_COUNT, countItem);
        }
        countItem->setText(QString::number(m_portMessagesByRow.at(row).size()));
    }

    ui->tblPortFilters->resizeColumnsToContents();
    ui->tblPortFilters->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::refreshConfiguredMessagesTable()
{
    ui->tblConfiguredMessages->setRowCount(0);
    int messageIndex = 0;

    for (int portRow = 0; portRow < m_portMessagesByRow.size(); ++portRow)
    {
        for (int messageRow = 0; messageRow < m_portMessagesByRow.at(portRow).size(); ++messageRow)
        {
            const MessageDefinition& message = m_portMessagesByRow.at(portRow).at(messageRow);
            const int row = ui->tblConfiguredMessages->rowCount();
            ui->tblConfiguredMessages->insertRow(row);
            ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_NAME, new QTableWidgetItem(message.messageName));
            ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_LENGTH, new QTableWidgetItem(QString::number(message.payloadLengthBytes)));
            ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_PORT, new QTableWidgetItem(QString::number(message.port)));
            ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_FIELDS, new QTableWidgetItem(fieldStatusText(message.fields)));

            QPushButton* button = new QPushButton("Configure Fields", ui->tblConfiguredMessages);
            button->setProperty("messageIndex", messageIndex);
            connect(button, SIGNAL(clicked()), this, SLOT(onConfigureMessageFieldsClicked()));
            ui->tblConfiguredMessages->setCellWidget(row, MESSAGE_COL_CONFIGURE, button);

            ++messageIndex;
        }
    }

    ui->tblConfiguredMessages->resizeColumnsToContents();
    ui->tblConfiguredMessages->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::openLengthFilterDialogForPortRow(int row)
{
    if (row < 0 || row >= m_portFilterBoxes.size() || row >= m_portMessagesByRow.size())
    {
        QMessageBox::warning(this, "Length Filters", "Select one port row.");
        return;
    }

    const int port = m_portFilterBoxes.at(row)->value();
    if (port < 1 || port > 65535)
    {
        QMessageBox::warning(this, "Invalid Port", "UDP port must be between 1 and 65535.");
        return;
    }

    MessageLengthFilterDialog dlg(this);
    dlg.setPort(static_cast<quint16>(port));
    dlg.setMessages(m_portMessagesByRow.at(row));

    if (dlg.exec() == QDialog::Accepted)
    {
        m_portMessagesByRow[row] = dlg.messages();
        refreshPortFilterTable();
        refreshConfiguredMessagesTable();
    }
}

void MainWindow::openFieldConfigurationForMessage(int messageIndex)
{
    if (messageIndex < 0)
    {
        QMessageBox::warning(this, "Field Configuration", "Select one configured message.");
        return;
    }

    int currentIndex = 0;
    for (int portRow = 0; portRow < m_portMessagesByRow.size(); ++portRow)
    {
        for (int messageRow = 0; messageRow < m_portMessagesByRow.at(portRow).size(); ++messageRow)
        {
            if (currentIndex == messageIndex)
            {
                MessageDefinition& message = m_portMessagesByRow[portRow][messageRow];
                const QString title = QString("Fields for %1").arg(message.messageName);
                const bool changed = configureFieldList(message.fields, message.payloadLengthBytes, title);
                if (changed)
                {
                    refreshPortFilterTable();
                    refreshConfiguredMessagesTable();
                }
                return;
            }
            ++currentIndex;
        }
    }

    QMessageBox::warning(this, "Field Configuration", "The selected message definition no longer exists.");
}

bool MainWindow::configureFieldList(QList<FieldDefinition>& fields, int payloadLengthBytes, const QString& title)
{
    FieldConfigurationDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setPayloadLength(payloadLengthBytes);
    dlg.setFields(fields);

    if (dlg.exec() == QDialog::Accepted)
    {
        fields = dlg.fields();
        return true;
    }

    return false;
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

    if (filterConfig.mode == FILTER_MODE_PORT)
    {
        const QList<MessageDefinition> messages = collectMessageDefinitions();
        if (!validateMessageDefinitions(messages, errorMessage))
        {
            QMessageBox::warning(this, "Invalid Message", errorMessage);
            return;
        }

        if (ui->chkVerifyMessagesBeforeExport->isChecked())
        {
            setBusy(true);
            setStatus("Checking configured messages against capture file...");
            QApplication::processEvents();
            const bool messagesExist = validateMessagesExistInCapture(messages, errorMessage);
            setBusy(false);

            if (!messagesExist)
            {
                QMessageBox::critical(this, "Message Not Found", errorMessage);
                return;
            }
        }

        if (!exportByMessageDefinitions(messages, errorMessage))
        {
            QMessageBox::critical(this, "Export Error", errorMessage);
            return;
        }

        return;
    }

    // v12: header-filter mode + per-row length filters → reuse exportByMessageDefinitions.
    // When the user has configured at least one header row with length filters, route the
    // entire header-mode export through the per-message path (each message keyed by the
    // common UDP port + payload length + optional header bytes for disambiguation).
    if (filterConfig.mode == FILTER_MODE_HEADER && anyHeaderRowHasMessages())
    {
        const QList<MessageDefinition> hdrMessages = collectHeaderModeMessageDefinitions(filterConfig.commonPort);
        if (!validateMessageDefinitions(hdrMessages, errorMessage))
        {
            QMessageBox::warning(this, "Invalid Message", errorMessage);
            return;
        }
        if (!exportByMessageDefinitions(hdrMessages, errorMessage))
        {
            QMessageBox::critical(this, "Export Error", errorMessage);
            return;
        }
        return;
    }

    if (m_headerFields.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Field", "Add at least one field before starting extraction.");
        return;
    }

    if (!InputValidator::validateFields(m_headerFields, errorMessage))
    {
        QMessageBox::warning(this, "Invalid Field", errorMessage);
        return;
    }

    const QFileInfo inputInfo(ui->txtFilePath->text().trimmed());
    QString baseCsvPath = QFileDialog::getSaveFileName(this, "Choose Base CSV Output Name", inputInfo.absoluteDir().filePath(defaultCsvName(ui->txtFilePath->text())), "CSV Files (*.csv);;All Files (*.*)");
    if (baseCsvPath.isEmpty()) return;
    if (!baseCsvPath.toLower().endsWith(".csv")) baseCsvPath += ".csv";

    const QStringList csvHeaders = buildOutputHeaders(m_headerFields);
    prepareOutputTable(buildPreviewHeaders(m_headerFields));

    const QString modeText = "header";
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
    // Defer preview-table repaints until the export loop ends; per-row paints
    // dominate when PREVIEW_ROW_LIMIT is large. Re-enabled below before setBusy(false).
    ui->tblOutput->setUpdatesEnabled(false);
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
        row << ExtractionEngine::valuesFromPayload(parsed.udpPayload, m_headerFields);

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
    ui->tblOutput->setUpdatesEnabled(true);
    ui->tblOutput->viewport()->update();
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

QList<FieldDefinition> MainWindow::defaultFields() const
{
    FieldDefinition field;
    field.name = "Field1";
    field.byteOffset = 1;
    field.byteOffsetcorrect = 0;
    field.length = 2;
    field.resolution = 1.0;
    field.resolutionExpression = "1";

    QList<FieldDefinition> fields;
    fields << field;
    return fields;
}

QString MainWindow::fieldStatusText(const QList<FieldDefinition>& fields) const
{
    if (fields.isEmpty())
        return "No fields";

    int decoderCount = 0;
    int condDecoderCount = 0;
    for (int i = 0; i < fields.size(); ++i)
    {
        if (fields.at(i).hasBitfieldDecoder)
            ++decoderCount;
        if (fields.at(i).hasConditionalBitfieldDecoder)
            ++condDecoderCount;
    }

    QString text = (fields.size() == 1) ? "1 field" : QString("%1 fields").arg(fields.size());
    if (decoderCount > 0)
        text += QString(", %1 decoder%2").arg(decoderCount).arg(decoderCount == 1 ? "" : "s");
    if (condDecoderCount > 0)
        text += QString(", %1 conditional").arg(condDecoderCount);
    return text;
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

QList<MessageDefinition> MainWindow::collectMessageDefinitions() const
{
    QList<MessageDefinition> messages;
    for (int portRow = 0; portRow < m_portMessagesByRow.size(); ++portRow)
    {
        const quint16 port = (portRow < m_portFilterBoxes.size())
            ? static_cast<quint16>(m_portFilterBoxes.at(portRow)->value())
            : 0;

        for (int messageRow = 0; messageRow < m_portMessagesByRow.at(portRow).size(); ++messageRow)
        {
            MessageDefinition message = m_portMessagesByRow.at(portRow).at(messageRow);
            message.port = port;
            messages << message;
        }
    }
    return messages;
}

bool MainWindow::validateMessageDefinitions(const QList<MessageDefinition>& messages, QString& errorMessage) const
{
    if (messages.isEmpty())
    {
        errorMessage = "Add at least one length filter before export.";
        return false;
    }

    QSet<QString> namesByPort;
    QSet<QString> lengthsByPort;

    for (int i = 0; i < messages.size(); ++i)
    {
        const MessageDefinition& message = messages.at(i);
        const QString name = message.messageName.trimmed();

        if (name.isEmpty())
        {
            errorMessage = "Message name cannot be empty.";
            return false;
        }

        if (message.port == 0)
        {
            errorMessage = "UDP port must be between 1 and 65535.";
            return false;
        }

        if (message.payloadLengthBytes <= 0)
        {
            errorMessage = "Payload length must be greater than 0.";
            return false;
        }

        // v12: dedupe key now includes the optional header hex so that two messages
        // can share length on the same port if they have distinct header signatures.
        const QString lengthKey = QString("%1:%2:%3").arg(message.port)
                                                      .arg(message.payloadLengthBytes)
                                                      .arg(QString::fromLatin1(message.optionalHeader.toHex()));
        if (lengthsByPort.contains(lengthKey))
        {
            errorMessage = QString("Another message with payload length %1 bytes and the same optional header already exists for port %2.")
                               .arg(message.payloadLengthBytes)
                               .arg(message.port);
            return false;
        }
        lengthsByPort.insert(lengthKey);

        const QString nameKey = QString("%1:%2").arg(message.port).arg(name.toLower());
        if (namesByPort.contains(nameKey))
        {
            errorMessage = QString("Another message named '%1' already exists for port %2.")
                               .arg(name)
                               .arg(message.port);
            return false;
        }
        namesByPort.insert(nameKey);

        if (message.fields.isEmpty())
        {
            errorMessage = QString("Message '%1' has no configured fields.").arg(name);
            return false;
        }

        QString fieldError;
        if (!InputValidator::validateFields(message.fields, fieldError))
        {
            errorMessage = QString("Message '%1': %2").arg(name).arg(fieldError);
            return false;
        }

        for (int f = 0; f < message.fields.size(); ++f)
        {
            const FieldDefinition& field = message.fields.at(f);
            if (field.byteOffsetcorrect < 0
                || field.byteOffsetcorrect + field.length > message.payloadLengthBytes)
            {
                errorMessage = QString("Field '%1' exceeds payload length %2 bytes.")
                                   .arg(field.name)
                                   .arg(message.payloadLengthBytes);
                return false;
            }

            if (field.hasBitfieldDecoder)
            {
                QString ruleError;
                if (!BitfieldDecoder::validateRules(field.bitDecodeRules, field.length, ruleError))
                {
                    errorMessage = QString("Message '%1', field '%2': %3").arg(name).arg(field.name).arg(ruleError);
                    return false;
                }
            }

            if (field.hasConditionalBitfieldDecoder)
            {
                QString condError;
                if (!ConditionalBitfieldDecoder::validate(field.conditionalDecoder,
                                                         message.fields,
                                                         field.name,
                                                         field.length,
                                                         condError))
                {
                    errorMessage = QString("Message '%1', field '%2' conditional decoder: %3")
                                       .arg(name).arg(field.name).arg(condError);
                    return false;
                }
            }
        }
    }

    return true;
}

bool MainWindow::validateMessagesExistInCapture(const QList<MessageDefinition>& messages, QString& errorMessage)
{
    QList<bool> found;
    for (int i = 0; i < messages.size(); ++i)
        found << false;

    PcapFileReader reader;
    if (!reader.open(ui->txtFilePath->text().trimmed(), errorMessage))
        return false;

    quint64 totalPackets = 0;
    while (true)
    {
        RawPacket rawPacket;
        QString readError;
        const bool hasPacket = reader.readNextPacket(rawPacket, readError);
        if (!hasPacket)
        {
            if (!readError.isEmpty())
            {
                errorMessage = readError;
                reader.close();
                return false;
            }
            break;
        }

        ++totalPackets;
        ParsedUdpPacket parsed = UdpPacketParser::parsePacket(rawPacket);
        if (!parsed.valid) continue;

        bool allFound = true;
        for (int i = 0; i < messages.size(); ++i)
        {
            if (!found.at(i) && packetMatchesMessage(parsed, messages.at(i)))
                found[i] = true;
            if (!found.at(i))
                allFound = false;
        }

        if (allFound)
            break;

        if ((totalPackets % 1000) == 0)
        {
            setStatus(QString("Checking messages... packets scanned=%1").arg(static_cast<qulonglong>(totalPackets)));
            QApplication::processEvents();
        }
    }

    reader.close();

    for (int i = 0; i < messages.size(); ++i)
    {
        if (!found.at(i))
        {
            const MessageDefinition& message = messages.at(i);
            errorMessage = QString("No packet found for message '%1' with port %2 and payload length %3 bytes.")
                               .arg(message.messageName)
                               .arg(message.port)
                               .arg(message.payloadLengthBytes);
            return false;
        }
    }

    return true;
}

bool MainWindow::exportByMessageDefinitions(const QList<MessageDefinition>& messages, QString& errorMessage)
{
    const QFileInfo inputInfo(ui->txtFilePath->text().trimmed());
    const QString outputDirectory = QFileDialog::getExistingDirectory(this,
                                                                      "Choose CSV Output Folder",
                                                                      inputInfo.absolutePath());
    if (outputDirectory.isEmpty())
    {
        setStatus("Export canceled.");
        return true;
    }

    const QString timestampText = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    QList<MessageOutputPartition> partitions;
    for (int i = 0; i < messages.size(); ++i)
    {
        MessageOutputPartition part;
        part.definition = messages.at(i);
        part.filePath = buildMessageCsvPath(outputDirectory, part.definition, timestampText);
        part.exporter = new CsvExporter();
        partitions << part;
    }

    // v13: per-partition refresh-rate trackers, parallel to partitions.
    QList<RefreshRateTracker> compareTrackers;
    for (int i = 0; i < partitions.size(); ++i)
        compareTrackers.append(RefreshRateTracker());

    for (int i = 0; i < partitions.size(); ++i)
    {
        QString openError;
        QStringList partHeaders = buildLiveFieldHeaders(partitions.at(i).definition.fields);
        // v13: append compare-options column names when configured for this message.
        partHeaders += CompareOptionsEngine::compareColumnNames(partitions.at(i).definition);
        if (!partitions[i].exporter->open(partitions[i].filePath, partHeaders, openError))
        {
            errorMessage = QString("Cannot open output CSV for message %1:\n%2\n\n%3")
                               .arg(partitions.at(i).definition.messageName)
                               .arg(partitions.at(i).filePath)
                               .arg(openError);
            closeMessagePartitions(partitions);
            return false;
        }
    }

    PcapFileReader reader;
    if (!reader.open(ui->txtFilePath->text().trimmed(), errorMessage))
    {
        closeMessagePartitions(partitions);
        return false;
    }
    const QString captureFormat = reader.formatName();

    prepareOutputTable(buildPortMessagePreviewHeaders());
    setBusy(true);
    // Defer preview-table repaints until the export loop ends; per-row paints
    // dominate when PREVIEW_ROW_LIMIT is large. Re-enabled below before setBusy(false).
    ui->tblOutput->setUpdatesEnabled(false);
    setStatus("Exporting message CSV files...");

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
        bool packetMatchedAnyMessage = false;

        for (int i = 0; i < partitions.size(); ++i)
        {
            MessageOutputPartition& part = partitions[i];
            if (!packetMatchesMessage(parsed, part.definition))
                continue;

            packetMatchedAnyMessage = true;

            QStringList row = ExtractionEngine::valuesFromPayload(parsed.udpPayload, part.definition.fields);
            // v13: append compare-options results when configured.
            if (part.definition.hasCompareOptions)
            {
                const qint64 tsMs = qint64(rawPacket.tsSec) * 1000 + qint64(rawPacket.tsUsec) / 1000;
                row += CompareOptionsEngine::compareRow(parsed.udpPayload, part.definition,
                                                         compareTrackers[i], tsMs);
            }

            if (!part.exporter->writeRow(row, errorMessage))
            {
                failed = true;
                errorMessage = QString("CSV write failed for message %1:\n%2\n\n%3")
                                   .arg(part.definition.messageName)
                                   .arg(part.filePath)
                                   .arg(errorMessage);
                break;
            }

            ++part.exportedRows;
            ++exportedRows;

            if (ui->tblOutput->rowCount() < PREVIEW_ROW_LIMIT)
            {
                QStringList previewRow;
                previewRow << part.definition.messageName;
                previewRow << QString::number(static_cast<qulonglong>(rawPacket.packetNumber));
                previewRow << parsed.timestamp;
                previewRow << parsed.sourceIp;
                previewRow << parsed.destinationIp;
                previewRow << QString::number(parsed.sourcePort);
                previewRow << QString::number(parsed.destinationPort);
                previewRow << QString::number(parsed.payloadSize);
                previewRow << row.join(" | ");
                appendPreviewRow(previewRow);
            }
        }

        if (failed)
            break;

        if (packetMatchedAnyMessage)
            ++matchedPackets;

        if ((totalPackets % 500) == 0)
        {
            setStatus(QString("Exporting... total=%1, UDP=%2, matched=%3, exported=%4")
                          .arg(static_cast<qulonglong>(totalPackets))
                          .arg(static_cast<qulonglong>(validUdpPackets))
                          .arg(static_cast<qulonglong>(matchedPackets))
                          .arg(static_cast<qulonglong>(exportedRows)));
            QApplication::processEvents();
        }
    }

    closeMessagePartitions(partitions);
    reader.close();
    ui->tblOutput->setUpdatesEnabled(true);
    ui->tblOutput->viewport()->update();
    setBusy(false);

    if (failed)
    {
        setStatus("Processing stopped due to error.");
        return false;
    }

    QStringList outputLines;
    QStringList missingLines;
    for (int i = 0; i < partitions.size(); ++i)
    {
        if (partitions.at(i).exportedRows == 0)
        {
            missingLines << QString("%1 length %2 port %3")
                                .arg(partitions.at(i).definition.messageName)
                                .arg(partitions.at(i).definition.payloadLengthBytes)
                                .arg(partitions.at(i).definition.port);
        }

        outputLines << QString("%1 length %2 port %3 -> %4 rows -> %5")
                           .arg(partitions.at(i).definition.messageName)
                           .arg(partitions.at(i).definition.payloadLengthBytes)
                           .arg(partitions.at(i).definition.port)
                           .arg(static_cast<qulonglong>(partitions.at(i).exportedRows))
                           .arg(partitions.at(i).filePath);
    }

    QString summary = QString("Done. Format=%1, total packets=%2, UDP packets=%3, matched packets=%4, exported rows=%5, preview rows=%6\n\nOutput files:\n%7")
                                .arg(captureFormat)
                                .arg(static_cast<qulonglong>(totalPackets))
                                .arg(static_cast<qulonglong>(validUdpPackets))
                                .arg(static_cast<qulonglong>(matchedPackets))
                                .arg(static_cast<qulonglong>(exportedRows))
                                .arg(ui->tblOutput->rowCount())
                                .arg(outputLines.join("\n"));

    if (!missingLines.isEmpty())
    {
        summary += "\n\nNo matching packets were found for:\n" + missingLines.join("\n");
    }

    setStatus(QString("Done. Exported rows=%1. Files=%2").arg(static_cast<qulonglong>(exportedRows)).arg(partitions.size()));
    if (missingLines.isEmpty())
        QMessageBox::information(this, "Export Complete", summary);
    else
        QMessageBox::warning(this, "Export Complete With Missing Messages", summary);
    return true;
}

void MainWindow::startLiveCapture()
{
    if (m_liveRunning)
        return;

    // v13: Live Mode now REQUIRES at least one length filter. The pre-v12
    // single-writer path below stays as dead code (additive per CLAUDE.md) — it
    // is no longer reachable because the trigger button (btnConfigureLiveFields)
    // is gone and this guard rejects empty m_liveMessages.
    if (m_liveMessages.isEmpty())
    {
        QMessageBox::warning(this, "Live Capture",
            "Define at least one length filter before starting live capture.\n"
            "Use 'Manage Length Filters' to add a message definition.");
        return;
    }

    QString errorMessage;
    if (m_liveFields.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Field", "Add at least one field before starting extraction.");
        return;
    }

    if (!InputValidator::validateFields(m_liveFields, errorMessage))
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

    // v12: when the user has configured per-message length filters for live mode,
    // delegate startup to startLiveCaptureWithMessages and return. That path opens
    // one CSV per MessageDefinition in a user-chosen directory. The pre-v12 single
    // writer path below remains the default when m_liveMessages is empty.
    if (!m_liveMessages.isEmpty())
    {
        QString liveErr;
        if (!startLiveCaptureWithMessages(bindPort, liveErr))
            QMessageBox::warning(this, "Live Capture", liveErr);
        return;
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
    s_livePreviewAppendSeq = 0;
    s_liveRenderedSeq = 0;
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

    // v12: close per-message writers (no-op when single-writer mode was used).
    closeLiveMessageWriters();
    // v13: clear per-message refresh-rate trackers.
    m_liveCompareTrackers.clear();

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

    // v12: when per-message writers are active, route by MessageDefinition and skip
    // the pre-v12 single-writer path entirely.
    if (!m_activeLiveMessages.isEmpty())
    {
        tryRouteLivePacketByMessage(payload, senderPort, sender, arrivalTimeUtc);
        return;
    }

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
    ++s_livePreviewAppendSeq;
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

    // Delta append: only push rows that arrived since the last refresh, then
    // trim from the top if we now exceed LIVE_PREVIEW_ROW_LIMIT. End-state
    // matches the previous "clear + rebuild from m_livePreviewRows" behavior
    // exactly, without per-tick table thrash when nothing arrived.
    const qint64 delta = s_livePreviewAppendSeq - s_liveRenderedSeq;
    if (delta > 0)
    {
        const int currentSize = m_livePreviewRows.size();
        const int toAppend = static_cast<int>(qMin<qint64>(delta, currentSize));
        const int startIndex = currentSize - toAppend;
        for (int i = startIndex; i < currentSize; ++i)
            appendPreviewRow(m_livePreviewRows.at(i));

        while (ui->tblOutput->rowCount() > LIVE_PREVIEW_ROW_LIMIT)
            ui->tblOutput->removeRow(0);

        s_liveRenderedSeq = s_livePreviewAppendSeq;
    }
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
    return ExtractionEngine::columnHeaders(fields);
}

QStringList MainWindow::buildPreviewHeaders(const QList<FieldDefinition>& fields) const
{
    QStringList headers;
    headers << "Filter";
    headers += buildOutputHeaders(fields);
    return headers;
}

QStringList MainWindow::buildPortMessagePreviewHeaders() const
{
    QStringList headers;
    headers << "Message"
            << "Packet No"
            << "Timestamp"
            << "Source IP"
            << "Destination IP"
            << "Source UDP Port"
            << "Destination UDP Port"
            << "Payload Size"
            << "Extracted Values";
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
    shortPacket = false;
    for (int i = 0; i < m_liveFields.size(); ++i)
    {
        const FieldDefinition& field = m_liveFields.at(i);
        if (field.byteOffsetcorrect >= 0 && field.length > 0
            && field.byteOffsetcorrect + field.length > payload.size())
        {
            shortPacket = true;
            break;
        }
    }
    return ExtractionEngine::valuesFromPayload(payload, m_liveFields);
}

QString MainWindow::buildPartitionCsvPath(const QString& baseCsvPath, const QString& modeText, const QString& filterLabel) const
{
    const QFileInfo info(baseCsvPath);
    return info.absoluteDir().filePath(QString("%1_%2_%3.csv").arg(safeName(info.completeBaseName())).arg(modeText).arg(safeName(filterLabel)));
}

QString MainWindow::buildMessageCsvPath(const QString& outputDirectory,
                                        const MessageDefinition& message,
                                        const QString& timestampText) const
{
    const QString fileName = QString("%1_%2_%3_%4.csv")
                                 .arg(safeName(message.messageName))
                                 .arg(message.payloadLengthBytes)
                                 .arg(message.port)
                                 .arg(timestampText);
    return QDir(outputDirectory).filePath(fileName);
}

void MainWindow::setBusy(bool busy)
{
    ui->btnStart->setEnabled(!busy);
    ui->btnBrowse->setEnabled(!busy);
    ui->btnStartLive->setEnabled(!busy && ui->radLiveMode->isChecked() && !m_liveRunning);
    ui->btnStopLive->setEnabled(m_liveRunning);
    ui->btnConfigureHeaderFields->setEnabled(!busy);
    // v13: btnConfigureLiveFields widget removed.
    ui->btnManageLiveLengthFilters->setEnabled(!busy && !m_liveRunning);
    ui->spinFilterCount->setEnabled(!busy);
    ui->radPortFilter->setEnabled(!busy);
    ui->radHeaderFilter->setEnabled(!busy);
    ui->portFilterPanel->setEnabled(!busy);
    ui->headerFilterPanel->setEnabled(!busy);
    ui->tblConfiguredMessages->setEnabled(!busy);
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
    ui->tblConfiguredMessages->setEnabled(!running);
    // v13: btnConfigureLiveFields widget removed.
    ui->btnConfigureHeaderFields->setEnabled(!running);
    ui->btnManageLiveLengthFilters->setEnabled(!running);
    ui->btnBrowse->setEnabled(!running);
    ui->btnStart->setEnabled(!running && ui->radFileMode->isChecked());
}

void MainWindow::refreshStandaloneFieldStatus()
{
    ui->lblHeaderFieldStatus->setText(fieldStatusText(m_headerFields));
    // v13: lblLiveFieldStatus widget removed; live fields surface in the per-row
    // 'Fields' column of tblLiveConfiguredMessages instead.
}

void MainWindow::setStatus(const QString& message)
{
    ui->lblStatus->setText(message);
}

void MainWindow::captureProjectState(ProjectState& state) const
{
    state.appSchemaVersion = 1;
    state.pcapPath = ui->txtFilePath->text().trimmed();
    state.inputMode = ui->radLiveMode->isChecked() ? QString("live") : QString("file");
    state.filterMode = ui->radHeaderFilter->isChecked() ? QString("header") : QString("port");
    state.filterCount = ui->spinFilterCount->value();

    QString filterErr;
    collectFilterConfiguration(state.filterConfig, filterErr);

    state.portMessagesByRow = m_portMessagesByRow;
    state.headerFields = m_headerFields;
    state.liveFields = m_liveFields;
    state.liveFilterConfig = m_liveFilterConfig;
    // v12: persist header-mode per-row length filters and live-mode length filters.
    state.headerMessagesByRow = m_headerMessagesByRow;
    state.liveMessages = m_liveMessages;
}

void MainWindow::applyProjectState(const ProjectState& state)
{
    if (!state.pcapPath.isEmpty())
        ui->txtFilePath->setText(state.pcapPath);

    if (state.inputMode == "live")
        ui->radLiveMode->setChecked(true);
    else
        ui->radFileMode->setChecked(true);

    if (state.filterMode == "header")
        ui->radHeaderFilter->setChecked(true);
    else
        ui->radPortFilter->setChecked(true);

    const int desiredCount = state.filterCount > 0 ? state.filterCount : 1;
    ui->spinFilterCount->setValue(desiredCount);
    rebuildFilterInputs();

    if (state.filterConfig.commonPort > 0)
        ui->spinCommonPort->setValue(state.filterConfig.commonPort);

    if (state.filterMode == "port")
    {
        for (int i = 0; i < state.filterConfig.filters.size() && i < m_portFilterBoxes.size(); ++i)
        {
            const int p = state.filterConfig.filters.at(i).port;
            if (p >= 0)
                m_portFilterBoxes.at(i)->setValue(p);
        }
    }
    else
    {
        for (int i = 0; i < state.filterConfig.filters.size() && i < m_headerFilterBoxes.size(); ++i)
        {
            const QByteArray& hdr = state.filterConfig.filters.at(i).header;
            m_headerFilterBoxes.at(i)->setText(QString::fromLatin1(hdr.toHex()));
        }
    }

    m_headerFields = state.headerFields;
    m_liveFields = state.liveFields;
    m_liveFilterConfig = state.liveFilterConfig;
    m_portMessagesByRow = state.portMessagesByRow;

    // v12: restore header-row length filters (resized to filterCount in rebuildFilterInputs)
    // and live-mode length filters.
    for (int i = 0; i < state.headerMessagesByRow.size() && i < m_headerMessagesByRow.size(); ++i)
        m_headerMessagesByRow[i] = state.headerMessagesByRow.at(i);
    m_liveMessages = state.liveMessages;

    refreshStandaloneFieldStatus();
    refreshPortFilterTable();
    refreshConfiguredMessagesTable();
    refreshHeaderLengthFilterStatus();
    refreshLiveLengthFilterStatus();
    // v13: re-render the live configured-messages table after restoring state.
    refreshLiveConfiguredMessagesTable();
}

void MainWindow::tryRestoreProjectForPcap(const QString& pcapPath)
{
    const QString sidecarPath = ProjectFile::sidecarPathFor(pcapPath);
    if (!ProjectFile::exists(sidecarPath))
        return;

    QFileInfo info(sidecarPath);
    const QString sizeText = QString::number(info.size());
    const QString modifiedText = info.lastModified().toString(Qt::ISODate);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Restore Previous Progress");
    box.setText("A saved project file was found for this capture.");
    box.setInformativeText(QString("Path:\n%1\n\nSaved at: %2\nSize: %3 bytes\n\nRestore previous progress?")
                              .arg(sidecarPath).arg(modifiedText).arg(sizeText));
    QPushButton* restoreBtn = box.addButton("Restore", QMessageBox::AcceptRole);
    box.addButton("Discard", QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(restoreBtn);
    box.exec();

    if (box.clickedButton() != restoreBtn)
        return;

    ProjectState state;
    QString error;
    if (!ProjectFile::load(sidecarPath, state, error))
    {
        QMessageBox::warning(this, "Restore Failed",
            QString("Could not load saved project:\n%1").arg(error));
        return;
    }
    applyProjectState(state);
    m_projectPath = sidecarPath;
    setStatus(QString("Restored project from %1").arg(sidecarPath));
}

void MainWindow::autoSaveProjectOnClose()
{
    const QString pcapPath = ui->txtFilePath->text().trimmed();
    if (m_projectPath.isEmpty() && pcapPath.isEmpty())
        return;

    ProjectState state;
    captureProjectState(state);

    QString savePath = m_projectPath;
    if (savePath.isEmpty())
        savePath = ProjectFile::sidecarPathFor(pcapPath);
    if (savePath.isEmpty())
        return;

    QString error;
    ProjectFile::save(state, savePath, error);
}

void MainWindow::onOpenProject()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Open Project File",
        QString(),
        "Project Files (*.pcproj.json);;All Files (*.*)");
    if (path.isEmpty()) return;

    ProjectState state;
    QString error;
    if (!ProjectFile::load(path, state, error))
    {
        QMessageBox::warning(this, "Open Project",
            QString("Failed to load project:\n%1").arg(error));
        return;
    }
    applyProjectState(state);
    m_projectPath = path;
    setStatus(QString("Loaded project from %1").arg(path));
}

void MainWindow::onSaveProject()
{
    if (m_projectPath.isEmpty())
    {
        const QString pcapPath = ui->txtFilePath->text().trimmed();
        if (!pcapPath.isEmpty())
            m_projectPath = ProjectFile::sidecarPathFor(pcapPath);
        if (m_projectPath.isEmpty())
        {
            onSaveProjectAs();
            return;
        }
    }

    ProjectState state;
    captureProjectState(state);
    QString error;
    if (!ProjectFile::save(state, m_projectPath, error))
    {
        QMessageBox::warning(this, "Save Project",
            QString("Failed to save project:\n%1").arg(error));
        return;
    }
    setStatus(QString("Project saved to %1").arg(m_projectPath));
}

void MainWindow::onSaveProjectAs()
{
    const QString pcapPath = ui->txtFilePath->text().trimmed();
    QString suggested = m_projectPath;
    if (suggested.isEmpty() && !pcapPath.isEmpty())
        suggested = ProjectFile::sidecarPathFor(pcapPath);

    const QString path = QFileDialog::getSaveFileName(this,
        "Save Project As",
        suggested,
        "Project Files (*.pcproj.json);;All Files (*.*)");
    if (path.isEmpty()) return;

    ProjectState state;
    captureProjectState(state);
    QString error;
    if (!ProjectFile::save(state, path, error))
    {
        QMessageBox::warning(this, "Save Project As",
            QString("Failed to save project:\n%1").arg(error));
        return;
    }
    m_projectPath = path;
    setStatus(QString("Project saved as %1").arg(path));
}

// ============================================================================
// v12 additions: theme toggle, header-mode length filters, live-mode length
// filters with per-message CSV writers, related helpers.
// ============================================================================

void MainWindow::onToggleThemeClicked()
{
    const Themes::Mode nowDark = (Themes::currentMode() == Themes::Dark) ? Themes::Light : Themes::Dark;
    Themes::setMode(nowDark);
    Themes::applyToAllTopLevels();
    ui->btnToggleTheme->setText(Themes::currentMode() == Themes::Dark ? "Light Theme" : "Dark Theme");
    setStatus(QString("Theme: %1").arg(Themes::currentMode() == Themes::Dark ? "Dark" : "Light"));
}

void MainWindow::onManageHeaderLengthFiltersClicked()
{
    QObject* obj = sender();
    const int row = obj ? obj->property("headerRow").toInt() : -1;
    openHeaderLengthFilterDialogForRow(row);
}

void MainWindow::onManageLiveLengthFiltersClicked()
{
    openLiveLengthFilterDialog();
}

void MainWindow::openHeaderLengthFilterDialogForRow(int row)
{
    if (row < 0 || row >= m_headerMessagesByRow.size())
    {
        QMessageBox::warning(this, "Length Filters", "Invalid header row selection.");
        return;
    }

    const int port = ui->spinCommonPort->value();
    if (port < 1 || port > 65535)
    {
        QMessageBox::warning(this, "Invalid Port",
            "Common UDP port must be between 1 and 65535 before defining length filters.");
        return;
    }

    MessageLengthFilterDialog dlg(this);
    dlg.setWindowTitle(QString("Length Filters for Header Filter %1").arg(row + 1));
    dlg.setPort(static_cast<quint16>(port));
    dlg.setMessages(m_headerMessagesByRow.at(row));

    if (dlg.exec() == QDialog::Accepted)
    {
        m_headerMessagesByRow[row] = dlg.messages();
        refreshHeaderLengthFilterStatus();
    }
}

void MainWindow::openLiveLengthFilterDialog()
{
    const int port = ui->spinLivePort->value();
    if (port < 1 || port > 65535)
    {
        QMessageBox::warning(this, "Invalid Port",
            "Live bind UDP port must be between 1 and 65535 before defining length filters.");
        return;
    }

    MessageLengthFilterDialog dlg(this);
    dlg.setWindowTitle("Length Filters for Live Capture");
    dlg.setPort(static_cast<quint16>(port));
    dlg.setMessages(m_liveMessages);

    if (dlg.exec() == QDialog::Accepted)
    {
        m_liveMessages = dlg.messages();
        refreshLiveLengthFilterStatus();
        // v13: keep the configured-messages table in sync with m_liveMessages.
        refreshLiveConfiguredMessagesTable();
    }
}

void MainWindow::refreshHeaderLengthFilterStatus()
{
    // Walk the header-filter container and update each status QLabel
    // (objectName == "lblHeaderLengthFilterStatus") to reflect how many messages
    // are configured for that row.
    QWidget* container = ui->headerFilterBoxContainer;
    if (!container) return;
    const QList<QWidget*> rowWidgets = container->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    int rowIndex = 0;
    for (int i = 0; i < rowWidgets.size(); ++i)
    {
        QWidget* rowWidget = rowWidgets.at(i);
        QLabel* lblStatus = rowWidget->findChild<QLabel*>("lblHeaderLengthFilterStatus");
        if (!lblStatus) continue;
        const int count = (rowIndex < m_headerMessagesByRow.size())
            ? m_headerMessagesByRow.at(rowIndex).size() : 0;
        lblStatus->setText(count == 0
            ? QString("No length filters")
            : QString("%1 message%2").arg(count).arg(count == 1 ? "" : "s"));
        ++rowIndex;
    }
}

void MainWindow::refreshLiveLengthFilterStatus()
{
    const int count = m_liveMessages.size();
    ui->lblLiveLengthFilterStatus->setText(count == 0
        ? QString("No length filters")
        : QString("%1 message%2").arg(count).arg(count == 1 ? "" : "s"));
}

bool MainWindow::anyHeaderRowHasMessages() const
{
    for (int i = 0; i < m_headerMessagesByRow.size(); ++i)
    {
        if (!m_headerMessagesByRow.at(i).isEmpty())
            return true;
    }
    return false;
}

QList<MessageDefinition> MainWindow::collectHeaderModeMessageDefinitions(int commonPort) const
{
    QList<MessageDefinition> all;
    for (int i = 0; i < m_headerMessagesByRow.size(); ++i)
    {
        const QList<MessageDefinition>& rowMsgs = m_headerMessagesByRow.at(i);
        for (int j = 0; j < rowMsgs.size(); ++j)
        {
            MessageDefinition m = rowMsgs.at(j);
            m.port = static_cast<quint16>(commonPort);
            all << m;
        }
    }
    return all;
}

bool MainWindow::startLiveCaptureWithMessages(int bindPort, QString& errorMessage)
{
    errorMessage.clear();

    // Validate every configured message definition before opening anything.
    for (int i = 0; i < m_liveMessages.size(); ++i)
    {
        const MessageDefinition& msg = m_liveMessages.at(i);
        if (msg.fields.isEmpty())
        {
            errorMessage = QString("Message '%1' has no configured fields.").arg(msg.messageName);
            return false;
        }
        QString fieldErr;
        if (!InputValidator::validateFields(msg.fields, fieldErr))
        {
            errorMessage = QString("Message '%1': %2").arg(msg.messageName).arg(fieldErr);
            return false;
        }
    }

    const QString outputDirectory = QFileDialog::getExistingDirectory(this,
        "Choose Output Folder for Per-Message Live CSV Files",
        QDir::currentPath());
    if (outputDirectory.isEmpty())
    {
        errorMessage = "Output folder selection canceled.";
        return false;
    }

    const QString timestampText = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    closeLiveMessageWriters();
    m_activeLiveMessages.clear();
    m_liveMessageRowCounts.clear();
    m_liveCompareTrackers.clear();  // v13

    for (int i = 0; i < m_liveMessages.size(); ++i)
    {
        const MessageDefinition& msg = m_liveMessages.at(i);
        const QString fileName = QString("liveCapture_%1_%2_%3.csv")
                                     .arg(safeName(msg.messageName))
                                     .arg(msg.payloadLengthBytes)
                                     .arg(timestampText);
        const QString outPath = QDir(outputDirectory).filePath(fileName);

        CsvStreamWriter* writer = new CsvStreamWriter();
        QStringList headers = buildLiveFieldHeaders(msg.fields);
        // v13: append compare-options column names when configured for this message.
        headers += CompareOptionsEngine::compareColumnNames(msg);
        QString openErr;
        if (!writer->open(outPath, headers, true, openErr))
        {
            errorMessage = QString("Could not open '%1' for writing: %2").arg(outPath).arg(openErr);
            delete writer;
            closeLiveMessageWriters();
            return false;
        }
        m_liveMessageWriters << writer;
        m_activeLiveMessages << msg;
        m_liveMessageRowCounts << 0;
        m_liveCompareTrackers << RefreshRateTracker();  // v13
    }

    QString socketError;
    if (!m_liveReceiver->start(static_cast<quint16>(bindPort), socketError))
    {
        closeLiveMessageWriters();
        m_activeLiveMessages.clear();
        m_liveMessageRowCounts.clear();
        errorMessage = QString("Could not bind UDP socket: %1").arg(socketError);
        return false;
    }

    m_livePacketsReceived = 0;
    m_livePacketsMatched = 0;
    m_liveShortPackets = 0;
    m_livePreviewRows.clear();
    m_liveRunning = true;

    // Preview headers when in per-message mode: TimestampUtc | SourceIP | SourcePort |
    // MessageName | ExtractedValues (joined).
    QStringList previewHeaders;
    previewHeaders << "TimestampUtc" << "SourceIP" << "SourcePort" << "MessageName" << "ExtractedValues";
    prepareOutputTable(previewHeaders);

    ui->lblPacketsReceived->setText("0");
    ui->lblPacketsMatched->setText("0");
    ui->lblRowsWritten->setText("0");
    ui->lblShortPackets->setText("0");
    ui->lblLastLiveError->setText("-");
    ui->lblLiveStatus->setText(QString("Listening (per-message, %1 writers)").arg(m_liveMessageWriters.size()));

    setLiveUiState(true);
    m_livePreviewTimer->start();
    setStatus(QString("Live capture listening on UDP port %1. Output dir: %2 (%3 per-message CSV files)")
                 .arg(bindPort).arg(outputDirectory).arg(m_liveMessageWriters.size()));
    return true;
}

bool MainWindow::tryRouteLivePacketByMessage(const QByteArray& payload,
                                             quint16 senderPort,
                                             const QHostAddress& sender,
                                             const QDateTime& arrivalTimeUtc)
{
    Q_UNUSED(senderPort);
    // Build a synthetic ParsedUdpPacket-like check: only port+length+optional header
    // matter here. Each active message stores its own port; we accept either source
    // OR destination port — but in live mode the binding is already on bindPort, so
    // matching the message's port suffices.
    for (int i = 0; i < m_activeLiveMessages.size(); ++i)
    {
        const MessageDefinition& msg = m_activeLiveMessages.at(i);
        if (payload.size() != msg.payloadLengthBytes) continue;
        if (!msg.optionalHeader.isEmpty())
        {
            if (payload.size() < msg.optionalHeader.size()) continue;
            if (payload.left(msg.optionalHeader.size()) != msg.optionalHeader) continue;
        }

        ++m_livePacketsMatched;

        bool shortPacket = false;
        for (int f = 0; f < msg.fields.size(); ++f)
        {
            const FieldDefinition& field = msg.fields.at(f);
            if (field.byteOffsetcorrect >= 0 && field.length > 0
                && field.byteOffsetcorrect + field.length > payload.size())
            {
                shortPacket = true;
                break;
            }
        }
        if (shortPacket) ++m_liveShortPackets;

        QStringList values = ExtractionEngine::valuesFromPayload(payload, msg.fields);
        // v13: append compare-options results when configured.
        if (msg.hasCompareOptions && i < m_liveCompareTrackers.size())
        {
            values += CompareOptionsEngine::compareRow(payload, msg,
                                                       m_liveCompareTrackers[i],
                                                       arrivalTimeUtc.toMSecsSinceEpoch());
        }

        if (i < m_liveMessageWriters.size() && m_liveMessageWriters.at(i))
        {
            QString writeErr;
            if (!m_liveMessageWriters[i]->writeRow(arrivalTimeUtc, sender.toString(), senderPort, values, writeErr))
            {
                onLiveSocketError(QString("CSV write failed for '%1': %2").arg(msg.messageName).arg(writeErr));
                return false;
            }
            m_liveMessageRowCounts[i] += 1;
        }

        QStringList previewRow;
        previewRow << arrivalTimeUtc.toUTC().toString(Qt::ISODateWithMs)
                   << sender.toString()
                   << QString::number(senderPort)
                   << msg.messageName
                   << values.join(" | ");
        m_livePreviewRows.append(previewRow);
        // Reuse the seq counter pattern from the namespace-scope variables in this file
        // by simply appending — refreshLivePreview will not pick up new rows in
        // per-message mode (different preview shape), so update labels directly here.
        while (m_livePreviewRows.size() > LIVE_PREVIEW_ROW_LIMIT)
            m_livePreviewRows.removeFirst();
        return true;
    }
    return false;
}

void MainWindow::closeLiveMessageWriters()
{
    for (int i = 0; i < m_liveMessageWriters.size(); ++i)
    {
        CsvStreamWriter* w = m_liveMessageWriters.at(i);
        if (!w) continue;
        if (w->isOpen())
        {
            QString flushErr;
            w->flush(flushErr);
            w->close();
        }
        delete w;
    }
    m_liveMessageWriters.clear();
    m_activeLiveMessages.clear();
    m_liveMessageRowCounts.clear();
}

// ============================================================================
// v13: live configured-messages table — mirrors the file-mode tblConfiguredMessages
// pattern (see refreshConfiguredMessagesTable / onConfigureMessageFieldsClicked)
// but is backed by m_liveMessages and includes an Optional Header column.
// ============================================================================

void MainWindow::refreshLiveConfiguredMessagesTable()
{
    const int LIVE_MSG_COL_NAME = 0;
    const int LIVE_MSG_COL_LENGTH = 1;
    const int LIVE_MSG_COL_HEADER = 2;
    const int LIVE_MSG_COL_FIELDS = 3;
    const int LIVE_MSG_COL_CONFIGURE = 4;

    ui->tblLiveConfiguredMessages->setRowCount(0);

    for (int i = 0; i < m_liveMessages.size(); ++i)
    {
        const MessageDefinition& msg = m_liveMessages.at(i);
        const int row = ui->tblLiveConfiguredMessages->rowCount();
        ui->tblLiveConfiguredMessages->insertRow(row);

        ui->tblLiveConfiguredMessages->setItem(row, LIVE_MSG_COL_NAME,
            new QTableWidgetItem(msg.messageName));
        ui->tblLiveConfiguredMessages->setItem(row, LIVE_MSG_COL_LENGTH,
            new QTableWidgetItem(QString::number(msg.payloadLengthBytes)));
        ui->tblLiveConfiguredMessages->setItem(row, LIVE_MSG_COL_HEADER,
            new QTableWidgetItem(msg.optionalHeader.isEmpty()
                ? QString("-")
                : QString::fromLatin1(msg.optionalHeader.toHex()).toUpper()));
        ui->tblLiveConfiguredMessages->setItem(row, LIVE_MSG_COL_FIELDS,
            new QTableWidgetItem(fieldStatusText(msg.fields)));

        QPushButton* button = new QPushButton("Configure Fields", ui->tblLiveConfiguredMessages);
        button->setProperty("liveMessageIndex", i);
        connect(button, SIGNAL(clicked()), this, SLOT(onConfigureLiveMessageFieldsClicked()));
        ui->tblLiveConfiguredMessages->setCellWidget(row, LIVE_MSG_COL_CONFIGURE, button);
    }

    ui->tblLiveConfiguredMessages->resizeColumnsToContents();
    ui->tblLiveConfiguredMessages->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::onConfigureLiveMessageFieldsClicked()
{
    QObject* obj = sender();
    const int idx = obj ? obj->property("liveMessageIndex").toInt() : -1;
    if (idx < 0 || idx >= m_liveMessages.size())
    {
        QMessageBox::warning(this, "Configure Fields",
            "The selected live message no longer exists.");
        return;
    }

    MessageDefinition& msg = m_liveMessages[idx];
    const QString title = QString("Fields for %1 (Live)").arg(msg.messageName);
    const bool changed = configureFieldList(msg.fields, msg.payloadLengthBytes, title);
    if (changed)
        refreshLiveConfiguredMessagesTable();
}
