#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ui_FilterRowWidget.h"

#include "BitfieldDecoder.h"
#include "ConditionalBitfieldDecoder.h"
#include "ConfigureConnectionsDialog.h"
#include "ExcelExporter.h"
#include "HelpManualDialog.h"
#include "ExcelStreamWriter.h"
#include "ExtractionEngine.h"
#include "FieldConfigurationDialog.h"
#include "IcdDocxImporter.h"
#include "IcdImportDialog.h"
#include "MessageJsonCodec.h"
#include "InputValidator.h"
#include "LiveUdpReceiver.h"
#include "MessageLengthFilterDialog.h"
#include "NmeaDecoder.h"
#include "NmeaFieldConfigurationDialog.h"
#include "NmeaSentenceRegistry.h"
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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QShortcut>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimer>
#include <QUrl>
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

// Export partitions write Excel workbooks. The workbook is saved when the
// partition closes (xlsx cannot be appended on disk row-by-row).
struct OutputPartition
{
    QString label;
    QString filePath;
    ExcelExporter* exporter;
    quint64 exportedRows;

    OutputPartition() : exporter(0), exportedRows(0) {}
};

struct MessageOutputPartition
{
    MessageDefinition definition;
    QString filePath;
    ExcelExporter* exporter;
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

QString defaultXlsxName(const QString& inputFilePath)
{
    const QFileInfo info(inputFilePath.trimmed());
    return QString("%1_%2_%3.xlsx")
        .arg(safeName(info.completeBaseName()))
        .arg(QDate::currentDate().toString("yyyyMMdd"))
        .arg(QTime::currentTime().toString("HHmmss"));
}

QString defaultLiveXlsxName()
{
    return QString("liveCapture_%1.xlsx")
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

// Closing an Excel partition performs the actual workbook save, so the close
// helpers optionally collect save failures (saveErrors = 0 keeps the old
// no-throw cleanup behaviour for error paths).
void closePartitions(QList<OutputPartition>& partitions, QStringList* saveErrors = 0)
{
    for (int i = 0; i < partitions.size(); ++i)
    {
        if (partitions[i].exporter)
        {
            QString err;
            if (!partitions[i].exporter->finalize(err) && saveErrors)
                *saveErrors << err;
            delete partitions[i].exporter;
            partitions[i].exporter = 0;
        }
    }
}

void closeMessagePartitions(QList<MessageOutputPartition>& partitions, QStringList* saveErrors = 0)
{
    for (int i = 0; i < partitions.size(); ++i)
    {
        if (partitions[i].exporter)
        {
            QString err;
            if (!partitions[i].exporter->finalize(err) && saveErrors)
                *saveErrors << err;
            delete partitions[i].exporter;
            partitions[i].exporter = 0;
        }
    }
}

// NMEA: build one CSV row for a decoded sentence, in the same column order as
// ExtractionEngine::columnHeaders(fields) produces (field.name per enabled
// field). Mirrors the old buildAsterixRow. A field whose nmeaFieldIndex is not
// present in the record yields an empty cell.
//
// For predefined sentences the decoder already formatted each token using the
// registry kind, so we use the formatted value. For CUSTOM sentences (formatter
// not in the registry) there is no registry to consult, so we re-format the raw
// token using the value kind the user chose per field.
QStringList buildNmeaRow(const NmeaDecodedRecord& record,
                         const QList<FieldDefinition>& fields)
{
    const bool custom = (NmeaSentenceRegistry::lookup(record.formatter) == 0);
    QStringList row;
    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& f = fields.at(i);
        if (custom)
            row << NmeaDecoder::formatValue(
                        static_cast<NmeaValueKind>(f.nmeaValueKind),
                        record.rawValueAt(f.nmeaFieldIndex));
        else
            row << record.valueAt(f.nmeaFieldIndex);
    }
    return row;
}

// NMEA: does this payload carry at least one sentence with the message's
// formatter? Scans for "$" + any 2-char talker + the 3-char formatter.
bool payloadContainsNmeaFormatter(const QByteArray& payload, const QString& formatter)
{
    const QString wanted = formatter.trimmed().toUpper();
    if (wanted.isEmpty())
        return false;
    const QString text = QString::fromLatin1(payload.constData(), payload.size()).toUpper();
    int from = 0;
    while (true)
    {
        const int dollar = text.indexOf(QChar('$'), from);
        if (dollar < 0)
            return false;
        // Address field is talker(2) + formatter(3) right after '$'. QString::mid
        // is bounds-safe, so a short tail simply fails the comparison.
        if (text.mid(dollar + 3, 3) == wanted)
            return true;
        from = dollar + 1;
    }
}

bool packetMatchesMessage(const ParsedUdpPacket& parsed, const MessageDefinition& message)
{
    if (parsed.sourcePort != message.port && parsed.destinationPort != message.port)
        return false;

    // NMEA: match by sentence formatter, ignoring exact byte length and the
    // byte-oriented optional header (those are Hex-only concepts).
    if (message.dataFormat == "NMEA")
        return payloadContainsNmeaFormatter(parsed.udpPayload, message.nmeaSentenceType);

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
      m_liveTcpReceiver(0),
      m_liveTransportTcp(false),
      m_livePreviewTimer(0),
      m_liveRunning(false),
      m_livePacketsReceived(0),
      m_livePacketsMatched(0),
      m_liveShortPackets(0)
{
    ui->setupUi(this);
    Themes::apply(this);
    setAcceptDrops(true);

    ui->spinFilterCount->setRange(InputValidator::minMessageFilterCount(), InputValidator::maxMessageFilterCount());
    ui->spinFilterCount->setValue(1);
    ui->spinCommonPort->setRange(0, 65535);
    ui->spinCommonPort->setValue(5000);
    ui->spinLivePort->setRange(1, 65535);
    ui->spinLivePort->setValue(5000);
    ui->radPortFilter->setChecked(true);
    ui->radFileMode->setChecked(true);

    ui->tblPortFilters->setColumnCount(3);
    ui->tblPortFilters->setHorizontalHeaderLabels(QStringList() << "Port" << "Configure Messages" << "Message Count");
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
    ui->tblLiveConfiguredMessages->setColumnCount(6);
    ui->tblLiveConfiguredMessages->setHorizontalHeaderLabels(QStringList()
        << "Message Name" << "Payload Length" << "Optional Header" << "Fields"
        << "Configure Fields" << "Connection");
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

    m_liveReceiver = new LiveUdpReceiver(this);
    m_liveTcpReceiver = new LiveTcpReceiver(this);
    m_livePreviewTimer = new QTimer(this);
    m_livePreviewTimer->setInterval(250);

    connect(ui->btnBrowse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartClicked()));
    connect(ui->spinFilterCount, SIGNAL(valueChanged(int)), this, SLOT(onFilterCountChanged(int)));
    connect(ui->radPortFilter, SIGNAL(toggled(bool)), this, SLOT(onFilterModeChanged()));
    connect(ui->radHeaderFilter, SIGNAL(toggled(bool)), this, SLOT(onFilterModeChanged()));
    connect(ui->btnConfigureHeaderFields, SIGNAL(clicked()), this, SLOT(onConfigureHeaderFieldsClicked()));

    connect(ui->actOpenProject,   SIGNAL(triggered()), this, SLOT(onOpenProject()));
    connect(ui->actSaveProject,   SIGNAL(triggered()), this, SLOT(onSaveProject()));
    connect(ui->actSaveProjectAs, SIGNAL(triggered()), this, SLOT(onSaveProjectAs()));
    connect(ui->actImportIcd,     SIGNAL(triggered()), this, SLOT(onImportIcdClicked()));
    connect(ui->btnImportIcd,     SIGNAL(clicked()),   this, SLOT(onImportIcdClicked()));
    connect(ui->actImportMessagesJson, SIGNAL(triggered()), this, SLOT(onImportMessagesJsonClicked()));
    connect(ui->actExportMessagesJson, SIGNAL(triggered()), this, SLOT(onExportMessagesJsonClicked()));

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

    // TCP receiver feeds the SAME live routing as UDP (each frame = one datagram).
    connect(m_liveTcpReceiver, SIGNAL(socketError(QString)), this, SLOT(onLiveSocketError(QString)));
    connect(m_liveTcpReceiver,
            SIGNAL(datagramReceived(QByteArray,QHostAddress,quint16,QDateTime)),
            this,
            SLOT(onLiveDatagramReceived(QByteArray,QHostAddress,quint16,QDateTime)));
    connect(m_liveTcpReceiver, SIGNAL(peerChanged(QString)), ui->lblLiveStatus, SLOT(setText(QString)));
    connect(ui->cmbLiveTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(onLiveTransportChanged()));
    connect(ui->cmbLiveTcpRole, SIGNAL(currentIndexChanged(int)), this, SLOT(onLiveTransportChanged()));

    rebuildFilterInputs();
    onLiveTransportChanged();
    refreshStandaloneFieldStatus();
    onFilterModeChanged();
    onInputModeChanged();
    setLiveUiState(false);

    // v12: theme toggle button + live mode length-filter affordance.
    ui->btnToggleTheme->setText(Themes::currentMode() == Themes::Dark ? "Light Theme" : "Dark Theme");
    connect(ui->btnToggleTheme, SIGNAL(clicked()), this, SLOT(onToggleThemeClicked()));
    connect(ui->btnManageLiveLengthFilters, SIGNAL(clicked()), this, SLOT(onManageLiveLengthFiltersClicked()));
    refreshLiveLengthFilterStatus();

    // Multi-connection live capture manager.
    connect(ui->btnConfigureConnections, SIGNAL(clicked()), this, SLOT(onConfigureConnectionsClicked()));
    refreshLiveConnectionSummary();

    // v13: initial empty render of the live configured-messages table.
    refreshLiveConfiguredMessagesTable();

    // Keyboard shortcuts. The full list lives in Help > Keyboard Shortcuts (Shift+F1).
    connect(ui->actShortcuts, SIGNAL(triggered()), this, SLOT(onShowShortcutsHelp()));
    connect(ui->actUserManual, SIGNAL(triggered()), this, SLOT(onShowUserManual()));
    new QShortcut(QKeySequence("Ctrl+1"), this, SLOT(onSelectFileMode()));
    new QShortcut(QKeySequence("Ctrl+2"), this, SLOT(onSelectLiveMode()));
    new QShortcut(QKeySequence("Ctrl+B"), this, SLOT(onBrowseClicked()));
    new QShortcut(QKeySequence("Ctrl+T"), this, SLOT(onToggleThemeClicked()));
    new QShortcut(QKeySequence(Qt::Key_F5), this, SLOT(onShortcutStart()));
    new QShortcut(QKeySequence("Shift+F5"), this, SLOT(onShortcutStop()));

    setStatus("Ready. Pick File or Live mode (Ctrl+1/2), define messages and fields, then start (F5). Press F1 for all shortcuts.");
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

    setStatus(liveMode ? "Live Mode selected." : "File Mode selected.");

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

        QPushButton* manageButton = new QPushButton("Configure Messages", ui->tblPortFilters);
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
        // Each header-filter row is instantiated from forms/FilterRowWidget.ui
        // rather than hand-built here. The status label keeps its objectName
        // ("lblHeaderLengthFilterStatus") in the .ui so refreshHeaderLengthFilterStatus()
        // still locates it.
        QWidget* row = new QWidget(ui->headerFilterBoxContainer);
        Ui::FilterRowWidget rowUi;
        rowUi.setupUi(row);
        rowUi.lblRow->setText(QString("Header Filter %1").arg(i + 1));
        QLineEdit* box = rowUi.txtHeader;
        if (i < oldHeaders.size()) box->setText(oldHeaders.at(i));
        // v12: per-header-row "Manage Length Filters" button + status label.
        QPushButton* lenBtn = rowUi.btnManageLengths;
        lenBtn->setProperty("headerRow", i);
        connect(lenBtn, SIGNAL(clicked()), this, SLOT(onManageHeaderLengthFiltersClicked()));
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
                // NMEA: registry-driven configurator instead of the Hex editor.
                if (message.dataFormat == "NMEA")
                {
                    NmeaFieldConfigurationDialog dlg(this);
                    dlg.setWindowTitle(QString("NMEA Fields for %1").arg(message.messageName));
                    dlg.setSentenceType(message.nmeaSentenceType);
                    dlg.setExistingConfig(message.fields);
                    if (dlg.exec() == QDialog::Accepted)
                    {
                        message.fields = dlg.fieldConfig();
                        refreshPortFilterTable();
                        refreshConfiguredMessagesTable();
                    }
                    return;
                }
                const QString title = QString("Fields for %1").arg(message.messageName);
                const bool changed = configureFieldList(message.fields, message.payloadLengthBytes, title,
                                                        &message.offsetUnit);
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

bool MainWindow::configureFieldList(QList<FieldDefinition>& fields, int payloadLengthBytes, const QString& title,
                                    QString* offsetUnit)
{
    FieldConfigurationDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setPayloadLength(payloadLengthBytes);
    if (offsetUnit)
        dlg.setOffsetUnit(*offsetUnit);   // before setFields so the table draws in the chosen unit
    dlg.setFields(fields);

    if (dlg.exec() == QDialog::Accepted)
    {
        fields = dlg.fields();
        if (offsetUnit)
            *offsetUnit = dlg.offsetUnit();
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

    if (!InputValidator::validateFields(m_headerFields, errorMessage, InputValidator::kNoNumericLengthCap))
    {
        QMessageBox::warning(this, "Invalid Field", errorMessage);
        return;
    }

    const QFileInfo inputInfo(ui->txtFilePath->text().trimmed());
    QString baseExportPath = QFileDialog::getSaveFileName(this, "Choose Base Excel Output Name", inputInfo.absoluteDir().filePath(defaultXlsxName(ui->txtFilePath->text())), "Excel Workbook (*.xlsx);;All Files (*.*)");
    if (baseExportPath.isEmpty()) return;
    if (!baseExportPath.toLower().endsWith(".xlsx")) baseExportPath += ".xlsx";

    const QStringList exportHeaders = buildOutputHeaders(m_headerFields);
    prepareOutputTable(buildPreviewHeaders(m_headerFields));

    const QString modeText = "header";
    QList<OutputPartition> partitions;
    for (int i = 0; i < filterConfig.filters.size(); ++i)
    {
        OutputPartition part;
        part.label = filterConfig.filters.at(i).label;
        part.filePath = buildPartitionExportPath(baseExportPath, modeText, part.label);
        part.exporter = new ExcelExporter();
        partitions << part;
    }

    for (int i = 0; i < partitions.size(); ++i)
    {
        QString openError;
        if (!partitions[i].exporter->open(partitions[i].filePath, exportHeaders, openError))
        {
            const QString msg = QString("Cannot open output Excel file for filter %1:\n%2\n\n%3").arg(partitions.at(i).label).arg(partitions.at(i).filePath).arg(openError);
            closePartitions(partitions);
            QMessageBox::critical(this, "Excel Error", msg);
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
            errorMessage = QString("Excel write failed for filter %1:\n%2\n\n%3").arg(part.label).arg(part.filePath).arg(errorMessage);
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

    // The workbook save happens at close — collect any save failure and surface it.
    QStringList saveErrors;
    closePartitions(partitions, &saveErrors);
    reader.close();
    ui->tblOutput->setUpdatesEnabled(true);
    ui->tblOutput->viewport()->update();
    setBusy(false);

    if (!failed && !saveErrors.isEmpty())
    {
        failed = true;
        errorMessage = saveErrors.join("\n");
    }

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

        // NMEA: fields are addressed by comma position, not byte offset, and
        // matching is by sentence formatter — so the Hex offset/length/dedup
        // checks below do not apply. A lightweight structural check suffices.
        if (message.dataFormat == "NMEA")
        {
            // Predefined formatters resolve via the registry; custom formatters
            // (not in the registry) are allowed as long as a formatter is set.
            if (message.nmeaSentenceType.trimmed().isEmpty())
            {
                errorMessage = QString("Message '%1': no NMEA sentence formatter set.").arg(name);
                return false;
            }
            if (message.fields.isEmpty())
            {
                errorMessage = QString("Message '%1' has no configured fields.").arg(name);
                return false;
            }
            for (int f = 0; f < message.fields.size(); ++f)
            {
                const FieldDefinition& nf = message.fields.at(f);
                if (nf.name.trimmed().isEmpty() || nf.nmeaFieldIndex <= 0)
                {
                    errorMessage = QString("Message '%1': NMEA field row %2 is missing a name or field index.")
                                       .arg(name).arg(f + 1);
                    return false;
                }
            }
            continue;
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
        if (!InputValidator::validateFields(message.fields, fieldError, InputValidator::kNoNumericLengthCap))
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
                                                                      "Choose Excel Output Folder",
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
        part.filePath = buildMessageExportPath(outputDirectory, part.definition, timestampText);
        part.exporter = new ExcelExporter();
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
            errorMessage = QString("Cannot open output Excel file for message %1:\n%2\n\n%3")
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
    setStatus("Exporting message Excel files...");

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

            // NMEA: decode the payload into one or more sentence records and
            // emit one CSV row per record (multi-sentence datagrams produce
            // multiple rows), mirroring the Asterix one-row-per-record path.
            if (part.definition.dataFormat == "NMEA")
            {
                const NmeaDecoder::Result dec =
                    NmeaDecoder::decodePacket(part.definition.nmeaSentenceType, parsed.udpPayload);
                for (int r = 0; r < dec.records.size(); ++r)
                {
                    QStringList nrow = buildNmeaRow(dec.records.at(r), part.definition.fields);
                    if (!part.exporter->writeRow(nrow, errorMessage))
                    {
                        failed = true;
                        errorMessage = QString("Excel write failed for NMEA message %1:\n%2\n\n%3")
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
                        previewRow << nrow.join(" | ");
                        appendPreviewRow(previewRow);
                    }
                }
                continue;
            }

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
                errorMessage = QString("Excel write failed for message %1:\n%2\n\n%3")
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

    // The workbook save happens at close — collect any save failure and surface it.
    QStringList saveErrors;
    closeMessagePartitions(partitions, &saveErrors);
    reader.close();
    ui->tblOutput->setUpdatesEnabled(true);
    ui->tblOutput->viewport()->update();
    setBusy(false);

    if (!failed && !saveErrors.isEmpty())
    {
        failed = true;
        errorMessage = saveErrors.join("\n");
    }

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

void MainWindow::onLiveTransportChanged()
{
    const bool tcp = (ui->cmbLiveTransport->currentIndex() == 1);
    ui->lblLiveTcpRole->setVisible(tcp);
    ui->cmbLiveTcpRole->setVisible(tcp);
    ui->lblLiveTcpHost->setVisible(tcp);
    ui->txtLiveTcpHost->setVisible(tcp);
    ui->lblLiveFrameLen->setVisible(tcp);
    ui->spinLiveFrameLen->setVisible(tcp);
    ui->lblLiveMulticast->setVisible(!tcp);
    ui->txtLiveMulticast->setVisible(!tcp);

    const bool connectMode = tcp && (ui->cmbLiveTcpRole->currentIndex() == 1);
    ui->txtLiveTcpHost->setEnabled(connectMode);   // host only used when dialling out
    ui->lblLivePort->setText(!tcp ? "Bind UDP Port" : (connectMode ? "Server port" : "Listen port"));
}

void MainWindow::startLiveCapture()
{
    if (m_liveRunning)
        return;

    // Live Mode REQUIRES at least one length filter; the per-message writer path
    // (startLiveCaptureWithMessages) is the only supported live flow.
    if (m_liveMessages.isEmpty())
    {
        QMessageBox::warning(this, "Live Capture",
            "Define at least one length filter before starting live capture.\n"
            "Use 'Configure Messages' to add a message definition.");
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

    // Join a multicast group when the user supplied one (blank = unicast).
    m_liveReceiver->setMulticastGroup(ui->txtLiveMulticast->text().trimmed());

    QString liveErr;
    if (!startLiveCaptureWithMessages(bindPort, liveErr))
        QMessageBox::warning(this, "Live Capture", liveErr);
}

void MainWindow::stopLiveCapture()
{
    if (!m_liveRunning)
        return;

    m_livePreviewTimer->stop();
    m_liveReceiver->stop();
    m_liveTcpReceiver->stop();
    stopSessionReceivers();

    // Collect per-file summary before the writers are destroyed.
    QStringList outputLines;
    quint64 totalRows = 0;
    for (int i = 0; i < m_liveMessageWriters.size(); ++i)
    {
        const ExcelStreamWriter* w = m_liveMessageWriters.at(i);
        if (!w) continue;
        totalRows += static_cast<quint64>(w->rowsWritten());
        outputLines << QString("%1 -> %2 rows -> %3")
                           .arg(i < m_activeLiveMessages.size()
                                    ? m_activeLiveMessages.at(i).messageName : QString("message"))
                           .arg(w->rowsWritten())
                           .arg(w->filePath());
    }

    closeLiveMessageWriters();
    m_liveCompareTrackers.clear();

    m_liveRunning = false;
    setLiveUiState(false);
    ui->lblLiveStatus->setText("Stopped");
    refreshLivePreview();

    QMessageBox::information(this, "Live Capture",
        QString("Live capture stopped.\n\nPackets received: %1\nPackets matched: %2\n"
                "Rows written: %3\n\nSaved files:\n%4")
            .arg(static_cast<qulonglong>(m_livePacketsReceived))
            .arg(static_cast<qulonglong>(m_livePacketsMatched))
            .arg(static_cast<qulonglong>(totalRows))
            .arg(outputLines.isEmpty() ? QString("(none)") : outputLines.join("\n")));

    setStatus(QString("Live capture stopped. Rows written=%1").arg(static_cast<qulonglong>(totalRows)));
}

void MainWindow::onLiveDatagramReceived(const QByteArray& payload,
                                        const QHostAddress& sender,
                                        quint16 senderPort,
                                        const QDateTime& arrivalTimeUtc)
{
    if (!m_liveRunning)
        return;

    ++m_livePacketsReceived;
    // Which connection produced this datagram? Legacy single-port receivers
    // (m_liveReceiver / m_liveTcpReceiver) aren't in the map → empty id, which the
    // router treats as "match every message" (unchanged behaviour).
    const QString connId = m_receiverConnectionId.value(QObject::sender(), QString());
    tryRouteLivePacketByMessage(payload, senderPort, sender, arrivalTimeUtc, connId);
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
    qint64 totalRows = 0;
    for (int i = 0; i < m_liveMessageRowCounts.size(); ++i)
        totalRows += m_liveMessageRowCounts.at(i);
    ui->lblRowsWritten->setText(QString::number(totalRows));
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

QString MainWindow::buildPartitionExportPath(const QString& baseExportPath, const QString& modeText, const QString& filterLabel) const
{
    const QFileInfo info(baseExportPath);
    return info.absoluteDir().filePath(QString("%1_%2_%3.xlsx").arg(safeName(info.completeBaseName())).arg(modeText).arg(safeName(filterLabel)));
}

QString MainWindow::buildMessageExportPath(const QString& outputDirectory,
                                           const MessageDefinition& message,
                                           const QString& timestampText) const
{
    const QString fileName = QString("%1_%2_%3_%4.xlsx")
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
    ui->btnConfigureConnections->setEnabled(!running);
    ui->btnBrowse->setEnabled(!running);
    ui->btnStart->setEnabled(!running && ui->radFileMode->isChecked());
    // Re-apply the connection-mode enable/disable on the single Transport/Port row
    // (it must stay disabled when explicit connections are in use).
    refreshLiveConnectionSummary();
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
    // v12: persist header-mode per-row length filters and live-mode length filters.
    state.headerMessagesByRow = m_headerMessagesByRow;
    state.liveMessages = m_liveMessages;
    state.liveConnections = m_liveConnections;
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
    m_portMessagesByRow = state.portMessagesByRow;

    // v12: restore header-row length filters (resized to filterCount in rebuildFilterInputs)
    // and live-mode length filters.
    for (int i = 0; i < state.headerMessagesByRow.size() && i < m_headerMessagesByRow.size(); ++i)
        m_headerMessagesByRow[i] = state.headerMessagesByRow.at(i);
    m_liveMessages = state.liveMessages;
    m_liveConnections = state.liveConnections;

    refreshStandaloneFieldStatus();
    refreshPortFilterTable();
    refreshConfiguredMessagesTable();
    refreshHeaderLengthFilterStatus();
    refreshLiveLengthFilterStatus();
    refreshLiveConnectionSummary();
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

void MainWindow::onConfigureConnectionsClicked()
{
    if (m_liveRunning)
    {
        QMessageBox::information(this, "Connections",
            "Stop live capture before changing connections.");
        return;
    }

    ConfigureConnectionsDialog dlg(this);
    dlg.setConnections(m_liveConnections);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_liveConnections = dlg.connections();

    // Drop bindings to connections that no longer exist so a stale id never
    // silently filters a message out of every connection.
    QStringList validIds;
    for (int i = 0; i < m_liveConnections.size(); ++i)
        validIds << m_liveConnections.at(i).id;
    for (int i = 0; i < m_liveMessages.size(); ++i)
    {
        if (!m_liveMessages.at(i).connectionId.isEmpty()
            && !validIds.contains(m_liveMessages.at(i).connectionId))
            m_liveMessages[i].connectionId.clear();
    }

    refreshLiveConnectionSummary();
    refreshLiveConfiguredMessagesTable();
}

void MainWindow::refreshLiveConnectionSummary()
{
    const int n = m_liveConnections.size();
    if (n == 0)
    {
        ui->lblLiveConnSummary->setText("No connections defined — using single Transport/Port below");
    }
    else
    {
        ui->lblLiveConnSummary->setText(
            QString("%1 connection%2 defined").arg(n).arg(n == 1 ? "" : "s"));
    }
    // The single Transport/Port controls are the fallback used only when no
    // explicit connections exist; disable them once connections take over.
    const bool single = (n == 0);
    ui->lblLiveTransport->setEnabled(single);
    ui->cmbLiveTransport->setEnabled(single);
    ui->lblLivePort->setEnabled(single);
    ui->spinLivePort->setEnabled(single && !m_liveRunning);
    ui->lblLiveMulticast->setEnabled(single);
    ui->txtLiveMulticast->setEnabled(single);
}

int MainWindow::frameLengthForConnection(const QString& connId) const
{
    // Exact TCP framing needs a single applicable message length. Applicable =
    // bound to this connection, or unbound (matches any). Mixed lengths => 0.
    int chosen = 0;
    int distinct = 0;
    for (int i = 0; i < m_liveMessages.size(); ++i)
    {
        const MessageDefinition& m = m_liveMessages.at(i);
        if (!m.connectionId.isEmpty() && m.connectionId != connId)
            continue;
        if (m.dataFormat == "NMEA")
            continue;   // NMEA isn't fixed-length framed
        if (chosen != m.payloadLengthBytes)
        {
            ++distinct;
            chosen = m.payloadLengthBytes;
        }
    }
    return (distinct == 1) ? chosen : 0;
}

bool MainWindow::startSessionReceivers(QString& errorMessage)
{
    errorMessage.clear();
    stopSessionReceivers();

    for (int i = 0; i < m_liveConnections.size(); ++i)
    {
        const ConnectionDefinition& c = m_liveConnections.at(i);
        QString err;
        bool ok = false;
        QObject* receiver = 0;

        if (c.transport == "TCP")
        {
            LiveTcpReceiver* tcp = new LiveTcpReceiver(this);
            connect(tcp, SIGNAL(socketError(QString)), this, SLOT(onLiveSocketError(QString)));
            connect(tcp,
                    SIGNAL(datagramReceived(QByteArray,QHostAddress,quint16,QDateTime)),
                    this,
                    SLOT(onLiveDatagramReceived(QByteArray,QHostAddress,quint16,QDateTime)));
            connect(tcp, SIGNAL(peerChanged(QString)), ui->lblLiveStatus, SLOT(setText(QString)));
            const LiveTcpReceiver::Role role =
                (c.tcpRole == "Connect") ? LiveTcpReceiver::Connect : LiveTcpReceiver::Listen;
            ok = tcp->start(role, c.host, c.port, frameLengthForConnection(c.id),
                            c.adapterAddress, err);
            receiver = tcp;
        }
        else
        {
            LiveUdpReceiver* udp = new LiveUdpReceiver(this);
            // UDP host field doubles as an optional multicast group.
            udp->setMulticastGroup(c.host.trimmed());
            udp->setBindAddress(c.adapterAddress);
            connect(udp, SIGNAL(socketError(QString)), this, SLOT(onLiveSocketError(QString)));
            connect(udp,
                    SIGNAL(datagramReceived(QByteArray,QHostAddress,quint16,QDateTime)),
                    this,
                    SLOT(onLiveDatagramReceived(QByteArray,QHostAddress,quint16,QDateTime)));
            ok = udp->start(c.port, err);
            receiver = udp;
        }

        if (!ok)
        {
            errorMessage = QString("Connection '%1' could not start: %2").arg(c.name).arg(err);
            if (receiver) receiver->deleteLater();
            stopSessionReceivers();
            return false;
        }

        m_liveSessionReceivers.append(receiver);
        m_receiverConnectionId.insert(receiver, c.id);
    }
    return true;
}

void MainWindow::stopSessionReceivers()
{
    for (int i = 0; i < m_liveSessionReceivers.size(); ++i)
    {
        QObject* obj = m_liveSessionReceivers.at(i);
        if (!obj) continue;
        if (LiveUdpReceiver* udp = qobject_cast<LiveUdpReceiver*>(obj))
            udp->stop();
        else if (LiveTcpReceiver* tcp = qobject_cast<LiveTcpReceiver*>(obj))
            tcp->stop();
        obj->deleteLater();
    }
    m_liveSessionReceivers.clear();
    m_receiverConnectionId.clear();
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
    dlg.setWindowTitle(QString("Configure Messages for Header Filter %1").arg(row + 1));
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
    dlg.setWindowTitle("Configure Messages for Live Capture");
    dlg.setPort(static_cast<quint16>(port));
    dlg.setMessages(m_liveMessages);
    // Multi-connection: offer per-message binding to the defined live connections.
    dlg.setConnections(m_liveConnections);

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
        // NMEA: skip the Hex offset/length validator; do a structural check.
        // Predefined and custom formatters are both accepted.
        if (msg.dataFormat == "NMEA")
        {
            if (msg.nmeaSentenceType.trimmed().isEmpty())
            {
                errorMessage = QString("Message '%1': no NMEA sentence formatter set.")
                                   .arg(msg.messageName);
                return false;
            }
            for (int f = 0; f < msg.fields.size(); ++f)
            {
                if (msg.fields.at(f).nmeaFieldIndex <= 0)
                {
                    errorMessage = QString("Message '%1': NMEA field row %2 is missing a field index.")
                                       .arg(msg.messageName).arg(f + 1);
                    return false;
                }
            }
            continue;
        }
        QString fieldErr;
        if (!InputValidator::validateFields(msg.fields, fieldErr, InputValidator::kNoNumericLengthCap))
        {
            errorMessage = QString("Message '%1': %2").arg(msg.messageName).arg(fieldErr);
            return false;
        }
    }

    const QString outputDirectory = QFileDialog::getExistingDirectory(this,
        "Choose Output Folder for Per-Message Live Excel Files",
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
        const QString fileName = QString("liveCapture_%1_%2_%3.xlsx")
                                     .arg(safeName(msg.messageName))
                                     .arg(msg.payloadLengthBytes)
                                     .arg(timestampText);
        const QString outPath = QDir(outputDirectory).filePath(fileName);

        ExcelStreamWriter* writer = new ExcelStreamWriter();
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
    bool started = false;
    if (!m_liveConnections.isEmpty())
    {
        // Multi-connection mode: one receiver per defined connection.
        started = startSessionReceivers(socketError);
    }
    else
    {
        // Legacy single Transport/Port path.
        m_liveTransportTcp = (ui->cmbLiveTransport->currentIndex() == 1);
        if (m_liveTransportTcp)
        {
            int frameLen = ui->spinLiveFrameLen->value();
            // 0 + a single configured message => frame by that message's length (exact).
            if (frameLen <= 0 && m_liveMessages.size() == 1)
                frameLen = m_liveMessages.at(0).payloadLengthBytes;
            const LiveTcpReceiver::Role role =
                (ui->cmbLiveTcpRole->currentIndex() == 1) ? LiveTcpReceiver::Connect : LiveTcpReceiver::Listen;
            started = m_liveTcpReceiver->start(role, ui->txtLiveTcpHost->text(),
                                               static_cast<quint16>(bindPort), frameLen,
                                               QString(), socketError);
        }
        else
        {
            started = m_liveReceiver->start(static_cast<quint16>(bindPort), socketError);
        }
    }
    if (!started)
    {
        stopSessionReceivers();
        closeLiveMessageWriters();
        m_activeLiveMessages.clear();
        m_liveMessageRowCounts.clear();
        errorMessage = socketError;
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
    const QString listenWhere = m_liveConnections.isEmpty()
        ? QString("UDP port %1").arg(bindPort)
        : QString("%1 connection%2").arg(m_liveConnections.size())
              .arg(m_liveConnections.size() == 1 ? "" : "s");
    setStatus(QString("Live capture listening on %1. Output dir: %2 (%3 per-message Excel files; saved when capture stops)")
                 .arg(listenWhere).arg(outputDirectory).arg(m_liveMessageWriters.size()));
    return true;
}

bool MainWindow::tryRouteLivePacketByMessage(const QByteArray& payload,
                                             quint16 senderPort,
                                             const QHostAddress& sender,
                                             const QDateTime& arrivalTimeUtc,
                                             const QString& connectionId)
{
    Q_UNUSED(senderPort);
    // Build a synthetic ParsedUdpPacket-like check: only port+length+optional header
    // matter here. Each active message stores its own port; we accept either source
    // OR destination port — but in live mode the binding is already on bindPort, so
    // matching the message's port suffices.
    for (int i = 0; i < m_activeLiveMessages.size(); ++i)
    {
        const MessageDefinition& msg = m_activeLiveMessages.at(i);

        // Multi-connection isolation: when this datagram came from a specific
        // connection, only decode it against messages bound to that connection.
        // An unbound message (empty connectionId) matches any connection, and a
        // legacy receiver (empty connectionId here) matches every message.
        if (!connectionId.isEmpty()
            && !msg.connectionId.isEmpty()
            && msg.connectionId != connectionId)
            continue;

        // NMEA: match by sentence formatter and emit one row per decoded record.
        if (msg.dataFormat == "NMEA")
        {
            if (!payloadContainsNmeaFormatter(payload, msg.nmeaSentenceType))
                continue;

            ++m_livePacketsMatched;

            const NmeaDecoder::Result dec =
                NmeaDecoder::decodePacket(msg.nmeaSentenceType, payload);
            for (int r = 0; r < dec.records.size(); ++r)
            {
                QStringList values = buildNmeaRow(dec.records.at(r), msg.fields);

                if (i < m_liveMessageWriters.size() && m_liveMessageWriters.at(i))
                {
                    QString writeErr;
                    if (!m_liveMessageWriters[i]->writeRow(arrivalTimeUtc, sender.toString(),
                                                           senderPort, values, writeErr))
                    {
                        onLiveSocketError(QString("Excel write failed for NMEA '%1': %2")
                                              .arg(msg.messageName).arg(writeErr));
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
                ++s_livePreviewAppendSeq;
                while (m_livePreviewRows.size() > LIVE_PREVIEW_ROW_LIMIT)
                    m_livePreviewRows.removeFirst();
            }
            return true;
        }

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
                onLiveSocketError(QString("Excel write failed for '%1': %2").arg(msg.messageName).arg(writeErr));
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
        ++s_livePreviewAppendSeq;
        while (m_livePreviewRows.size() > LIVE_PREVIEW_ROW_LIMIT)
            m_livePreviewRows.removeFirst();
        return true;
    }
    return false;
}

void MainWindow::closeLiveMessageWriters()
{
    QStringList saveErrors;
    for (int i = 0; i < m_liveMessageWriters.size(); ++i)
    {
        ExcelStreamWriter* w = m_liveMessageWriters.at(i);
        if (!w) continue;
        if (w->isOpen())
        {
            QString flushErr;
            if (!w->flush(flushErr) && !flushErr.isEmpty())
                saveErrors << flushErr;
            w->close();
        }
        delete w;
    }
    m_liveMessageWriters.clear();
    m_activeLiveMessages.clear();
    m_liveMessageRowCounts.clear();
    if (!saveErrors.isEmpty())
    {
        ui->lblLastLiveError->setText(saveErrors.first());
        QMessageBox::warning(this, "Live Capture",
            QString("One or more Excel workbooks could not be saved:\n%1").arg(saveErrors.join("\n")));
    }
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
    const int LIVE_MSG_COL_CONNECTION = 5;

    // The Connection column only carries meaning when connections are defined.
    ui->tblLiveConfiguredMessages->setColumnHidden(LIVE_MSG_COL_CONNECTION, m_liveConnections.isEmpty());

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

        // Show which live connection this message is bound to ("(any)" when unbound).
        QString connName = "(any)";
        for (int c = 0; c < m_liveConnections.size(); ++c)
        {
            if (m_liveConnections.at(c).id == msg.connectionId)
            {
                connName = m_liveConnections.at(c).name;
                break;
            }
        }
        ui->tblLiveConfiguredMessages->setItem(row, LIVE_MSG_COL_CONNECTION,
            new QTableWidgetItem(connName));
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
    // NMEA: registry-driven configurator instead of the Hex editor.
    if (msg.dataFormat == "NMEA")
    {
        NmeaFieldConfigurationDialog dlg(this);
        dlg.setWindowTitle(QString("NMEA Fields for %1 (Live)").arg(msg.messageName));
        dlg.setSentenceType(msg.nmeaSentenceType);
        dlg.setExistingConfig(msg.fields);
        if (dlg.exec() == QDialog::Accepted)
        {
            msg.fields = dlg.fieldConfig();
            refreshLiveConfiguredMessagesTable();
        }
        return;
    }
    const QString title = QString("Fields for %1 (Live)").arg(msg.messageName);
    const bool changed = configureFieldList(msg.fields, msg.payloadLengthBytes, title, &msg.offsetUnit);
    if (changed)
        refreshLiveConfiguredMessagesTable();
}

// ============================================================================
// Drag-and-drop of a project sidecar (.pcproj.json) onto the main window.
// Dropping the file loads it exactly as Open Project would. Additive — the
// existing onOpenProject slot is unchanged.
// ============================================================================

namespace
{
QString firstProjectFile(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls()) return QString();
    const QList<QUrl> urls = mime->urls();
    for (int i = 0; i < urls.size(); ++i)
    {
        const QString local = urls.at(i).toLocalFile();
        if (local.isEmpty()) continue;
        if (local.endsWith(".pcproj.json", Qt::CaseInsensitive))
            return local;
    }
    return QString();
}
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!firstProjectFile(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QString path = firstProjectFile(event->mimeData());
    if (path.isEmpty())
    {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    loadProjectFromPath(path);
}

void MainWindow::loadProjectFromPath(const QString& path)
{
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

// ============================================================================
// ICD .docx import. File menu -> Import ICD (.docx). Reads a Word ICD, extracts
// its tables, opens the review/selection dialog, and appends the chosen messages
// into the active mode's message list. Strictly additive: existing paths are
// untouched; nothing is written unless the user confirms in the dialog.
// ============================================================================

void MainWindow::onImportIcdClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import ICD (Word .docx)",
        QString(),
        "Word Documents (*.docx);;All Files (*.*)");
    if (path.isEmpty())
        return;

    IcdDocument doc;
    QString error;
    if (!IcdDocxImporter::extract(path, doc, error))
    {
        QMessageBox::warning(this, "Import ICD",
            QString("Could not read the document:\n%1").arg(error));
        return;
    }

    IcdImportDialog dlg(this);
    dlg.setDocument(doc);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QList<MessageDefinition> messages = dlg.selectedMessages();
    if (messages.isEmpty())
    {
        setStatus("ICD import: nothing was selected.");
        return;
    }
    applyImportedMessages(messages);
}

void MainWindow::applyImportedMessages(const QList<MessageDefinition>& messages)
{
    if (messages.isEmpty())
        return;

    // Live mode: all imported messages join the live length-filter set.
    if (ui->radLiveMode->isChecked())
    {
        for (int i = 0; i < messages.size(); ++i)
            m_liveMessages.append(messages.at(i));
        refreshLiveConfiguredMessagesTable();
        setStatus(QString("Imported %1 message(s) into Live mode length filters.")
                      .arg(messages.size()));
        return;
    }

    // File mode, header filter: route into the first header row's length filters.
    if (ui->radHeaderFilter->isChecked())
    {
        if (m_headerMessagesByRow.isEmpty())
            m_headerMessagesByRow << QList<MessageDefinition>();
        for (int i = 0; i < messages.size(); ++i)
            m_headerMessagesByRow[0].append(messages.at(i));
        refreshHeaderLengthFilterStatus();
        setStatus(QString("Imported %1 message(s) into Header Filter row 1's length filters.")
                      .arg(messages.size()));
        return;
    }

    // File mode, port filter: route into the selected port row (default row 0). In
    // port mode the row's port is authoritative, so stamp it onto each message
    // (matching refreshPortFilterTable's existing behaviour).
    int row = ui->tblPortFilters->currentRow();
    if (row < 0)
        row = 0;
    if (row >= m_portMessagesByRow.size())
    {
        QMessageBox::warning(this, "Import ICD",
            "No port filter row is available to receive the imported messages. "
            "Add a port filter first.");
        return;
    }
    const quint16 rowPort = (row < m_portFilterBoxes.size())
        ? static_cast<quint16>(m_portFilterBoxes.at(row)->value())
        : static_cast<quint16>(0);
    for (int i = 0; i < messages.size(); ++i)
    {
        MessageDefinition msg = messages.at(i);
        msg.port = rowPort;
        m_portMessagesByRow[row].append(msg);
    }
    refreshPortFilterTable();
    refreshConfiguredMessagesTable();
    setStatus(QString("Imported %1 message(s) into port row %2 (port %3).")
                  .arg(messages.size()).arg(row + 1).arg(rowPort));
}

QList<MessageDefinition> MainWindow::collectMessagesForJsonExport() const
{
    if (ui->radLiveMode->isChecked())
        return m_liveMessages;

    if (ui->radHeaderFilter->isChecked())
    {
        QList<MessageDefinition> out;
        for (int r = 0; r < m_headerMessagesByRow.size(); ++r)
            out += m_headerMessagesByRow.at(r);
        return out;
    }

    // Port mode (stamps each row's port onto its messages).
    return collectMessageDefinitions();
}

void MainWindow::onExportMessagesJsonClicked()
{
    const QList<MessageDefinition> messages = collectMessagesForJsonExport();
    if (messages.isEmpty())
    {
        QMessageBox::information(this, "Export Messages (JSON)",
            "There are no messages to export in the current mode. "
            "Solution: define at least one length-filter message (or import an ICD) first.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        "Export Messages to JSON", "messages.json", "JSON Files (*.json)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QMessageBox::warning(this, "Export Messages (JSON)",
            QString("Cannot write '%1': %2. Solution: pick a writable location and try again.")
                .arg(path).arg(file.errorString()));
        return;
    }
    file.write(MessageJsonCodec::messagesToJson(messages).toUtf8());
    file.close();

    QMessageBox::information(this, "Export Messages (JSON)",
        QString("Exported %1 message(s) to:\n%2").arg(messages.size()).arg(path));
}

void MainWindow::onImportMessagesJsonClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import Messages from JSON", QString(), "JSON Files (*.json);;All Files (*.*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Import Messages (JSON)",
            QString("Cannot open '%1': %2.").arg(path).arg(file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QList<MessageDefinition> imported;
    QString error;
    if (!MessageJsonCodec::messagesFromJson(QString::fromUtf8(bytes), imported, error))
    {
        QMessageBox::warning(this, "Import Messages (JSON)",
            QString("Import failed:\n\n%1").arg(error));
        return;
    }
    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import Messages (JSON)", "The file contained no messages.");
        return;
    }

    // applyImportedMessages routes them into the active mode's length filters
    // (live / header / port) and updates the relevant table + status line.
    applyImportedMessages(imported);
}

// ============================================================================
// Keyboard shortcuts. Small mode-aware slots so plain QShortcut string-based
// connects work everywhere (Help > Keyboard Shortcuts lists them all).
// ============================================================================

void MainWindow::onSelectFileMode()
{
    if (ui->radFileMode->isEnabled())
        ui->radFileMode->setChecked(true);
}

void MainWindow::onSelectLiveMode()
{
    if (ui->radLiveMode->isEnabled())
        ui->radLiveMode->setChecked(true);
}

void MainWindow::onShortcutStart()
{
    if (ui->radLiveMode->isChecked())
    {
        if (!m_liveRunning && ui->btnStartLive->isEnabled())
            startLiveCapture();
    }
    else if (ui->btnStart->isEnabled())
    {
        onStartClicked();
    }
}

void MainWindow::onShortcutStop()
{
    if (m_liveRunning)
        stopLiveCapture();
}

void MainWindow::onShowUserManual()
{
    // Modeless so users can read the manual while working in the app.
    HelpManualDialog* dlg = new HelpManualDialog(":/manual/parser_manual.html",
        "User Manual — Universal Wireshark Log Reader", this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
}

void MainWindow::onShowShortcutsHelp()
{
    QMessageBox box(this);
    box.setWindowTitle("Keyboard Shortcuts");
    box.setTextFormat(Qt::RichText);
    box.setText(
        "<b>Main window</b><br>"
        "<table cellspacing='6'>"
        "<tr><td><b>Ctrl+1 / Ctrl+2</b></td><td>File / Live mode</td></tr>"
        "<tr><td><b>F5</b></td><td>Start (export / live capture)</td></tr>"
        "<tr><td><b>Shift+F5</b></td><td>Stop the running live capture</td></tr>"
        "<tr><td><b>Ctrl+B</b></td><td>Browse for a capture file</td></tr>"
        "<tr><td><b>Ctrl+O / Ctrl+S / Ctrl+Shift+S</b></td><td>Open / Save / Save project as</td></tr>"
        "<tr><td><b>Ctrl+I</b></td><td>Import ICD (.docx)</td></tr>"
        "<tr><td><b>Ctrl+T</b></td><td>Toggle light / dark theme</td></tr>"
        "<tr><td><b>F1</b></td><td>This help</td></tr>"
        "</table><br>"
        "<b>Field definition tables</b> (Configure Fields dialog)<br>"
        "<table cellspacing='6'>"
        "<tr><td><b>Insert</b></td><td>Add a new field row</td></tr>"
        "<tr><td><b>Ctrl+E</b></td><td>Edit the selected field</td></tr>"
        "<tr><td><b>Ctrl+Delete</b></td><td>Remove the selected field</td></tr>"
        "<tr><td><b>Arrow keys / Tab</b></td><td>Move between rows and cells</td></tr>"
        "</table>");
    box.exec();
}
