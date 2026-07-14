#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ui_FilterRowWidget.h"

#include "AppPaths.h"
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
#include "MessageDefinitionDialog.h"
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
const int MESSAGE_COL_NAME = 0;
const int MESSAGE_COL_PORT = 1;
const int MESSAGE_COL_LENGTH = 2;
const int MESSAGE_COL_HEADER = 3;
const int MESSAGE_COL_FIELDS = 4;
const int MESSAGE_COL_CONFIGURE = 5;

// Live-preview render bookkeeping. s_livePreviewAppendSeq is bumped every
// time onLiveDatagramReceived() appends to m_livePreviewRows. refreshLivePreview()
// uses the delta against s_liveRenderedSeq to append only what is new, instead
// of clearing and rebuilding the entire table every 250 ms. Both are reset
// to zero in startLiveCapture() before listening begins.
qint64 s_livePreviewAppendSeq = 0;
qint64 s_liveRenderedSeq = 0;

// Export partitions write Excel workbooks. The workbook is saved when the
// partition closes (xlsx cannot be appended on disk row-by-row).
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

QString defaultLiveXlsxName()
{
    return QString("liveCapture_%1.xlsx")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

// Closing an Excel partition performs the actual workbook save, so the close
// helper optionally collects save failures (saveErrors = 0 keeps the no-throw
// cleanup behaviour for error paths).
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

    ui->radFileMode->setChecked(true);

    ui->tblConfiguredMessages->setColumnCount(6);
    ui->tblConfiguredMessages->setHorizontalHeaderLabels(QStringList()
        << "Message Name" << "UDP Port" << "Payload Length" << "Optional Header"
        << "Fields" << "Configure Fields");
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

    m_liveReceiver = new LiveUdpReceiver(this);
    m_liveTcpReceiver = new LiveTcpReceiver(this);
    m_livePreviewTimer = new QTimer(this);
    m_livePreviewTimer->setInterval(250);

    connect(ui->btnBrowse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartClicked()));
    connect(ui->btnAddMessage, SIGNAL(clicked()), this, SLOT(onAddMessageClicked()));
    connect(ui->btnEditMessage, SIGNAL(clicked()), this, SLOT(onEditMessageClicked()));
    connect(ui->btnRemoveMessage, SIGNAL(clicked()), this, SLOT(onRemoveMessageClicked()));
    connect(ui->btnImportMessagesJson, SIGNAL(clicked()), this, SLOT(onImportMessagesJsonClicked()));
    connect(ui->btnExportMessagesJson, SIGNAL(clicked()), this, SLOT(onExportMessagesJsonClicked()));

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

    refreshConfiguredMessagesTable();
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

    // Restore the previous live-mode session (messages + connections) if one was
    // auto-saved in the Projects folder on the last close.
    tryRestoreLiveAutosave();
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

bool MainWindow::editMessageDefinition(MessageDefinition& message)
{
    MessageDefinitionDialog dlg(this);
    dlg.setMessageName(message.messageName);
    dlg.setPort(message.port);
    dlg.setPayloadLength(message.payloadLengthBytes);
    dlg.setOptionalHeaderHex(QString::fromLatin1(message.optionalHeader.toHex()));
    dlg.setDataFormat(message.dataFormat);
    dlg.setNmeaSentenceType(message.nmeaSentenceType);
    if (dlg.exec() != QDialog::Accepted)
        return false;

    // Update only the dialog-edited scalar properties; the message keeps its fields,
    // compare options and connection binding.
    message.messageName = dlg.messageName();
    message.port = static_cast<quint16>(dlg.port());
    message.payloadLengthBytes = dlg.payloadLengthBytes();
    message.optionalHeader = QByteArray::fromHex(dlg.optionalHeaderHex().toLatin1());
    message.dataFormat = dlg.dataFormat();
    message.nmeaSentenceType = dlg.nmeaSentenceType();
    return true;
}

void MainWindow::onAddMessageClicked()
{
    MessageDefinition msg;
    msg.port = 5000;
    msg.payloadLengthBytes = 1;
    msg.dataFormat = "HEX";
    if (!editMessageDefinition(msg))
        return;
    m_messages.append(msg);
    refreshConfiguredMessagesTable();
    ui->tblConfiguredMessages->selectRow(m_messages.size() - 1);
    setStatus(QString("Added message '%1'.").arg(msg.messageName));
}

void MainWindow::onEditMessageClicked()
{
    const int row = ui->tblConfiguredMessages->currentRow();
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Edit Message", "Select a message to edit.");
        return;
    }
    MessageDefinition msg = m_messages.at(row);
    if (!editMessageDefinition(msg))
        return;
    m_messages[row] = msg;
    refreshConfiguredMessagesTable();
    ui->tblConfiguredMessages->selectRow(row);
}

void MainWindow::onRemoveMessageClicked()
{
    const int row = ui->tblConfiguredMessages->currentRow();
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Remove Message", "Select a message to remove.");
        return;
    }
    const QString name = m_messages.at(row).messageName;
    if (QMessageBox::question(this, "Remove Message",
            QString("Remove message '%1'?").arg(name.isEmpty() ? QString("Row %1").arg(row + 1) : name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    m_messages.removeAt(row);
    refreshConfiguredMessagesTable();
}

void MainWindow::onInputModeChanged()
{
    const bool liveMode = ui->radLiveMode->isChecked();
    ui->inputGroup->setVisible(!liveMode);
    ui->liveGroup->setVisible(liveMode);
    // File mode shows the single Message Definitions box; live mode shows its own
    // configured-messages table (bound to connections) in its place.
    ui->configuredMessagesGroup->setVisible(!liveMode);
    ui->liveConfiguredMessagesGroup->setVisible(liveMode);

    setStatus(liveMode ? "Live Mode selected." : "File Mode selected.");
}

void MainWindow::onConfigureMessageFieldsClicked()
{
    QObject* object = sender();
    const int messageIndex = object ? object->property("messageIndex").toInt() : -1;
    openFieldConfigurationForMessage(messageIndex);
}

void MainWindow::refreshConfiguredMessagesTable()
{
    ui->tblConfiguredMessages->setRowCount(0);

    for (int i = 0; i < m_messages.size(); ++i)
    {
        const MessageDefinition& message = m_messages.at(i);
        const int row = ui->tblConfiguredMessages->rowCount();
        ui->tblConfiguredMessages->insertRow(row);
        ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_NAME, new QTableWidgetItem(message.messageName));
        ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_PORT, new QTableWidgetItem(QString::number(message.port)));

        const QString lengthText = (message.dataFormat == "NMEA")
            ? QString("NMEA %1").arg(message.nmeaSentenceType)
            : QString::number(message.payloadLengthBytes);
        ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_LENGTH, new QTableWidgetItem(lengthText));

        const QString headerText = message.optionalHeader.isEmpty()
            ? QString("-")
            : QString::fromLatin1(message.optionalHeader.toHex()).toUpper();
        ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_HEADER, new QTableWidgetItem(headerText));

        ui->tblConfiguredMessages->setItem(row, MESSAGE_COL_FIELDS, new QTableWidgetItem(fieldStatusText(message.fields)));

        QPushButton* button = new QPushButton("Configure Fields", ui->tblConfiguredMessages);
        button->setProperty("messageIndex", i);
        connect(button, SIGNAL(clicked()), this, SLOT(onConfigureMessageFieldsClicked()));
        ui->tblConfiguredMessages->setCellWidget(row, MESSAGE_COL_CONFIGURE, button);
    }

    ui->tblConfiguredMessages->resizeColumnsToContents();
    ui->tblConfiguredMessages->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::openFieldConfigurationForMessage(int messageIndex)
{
    if (messageIndex < 0 || messageIndex >= m_messages.size())
    {
        QMessageBox::warning(this, "Field Configuration", "Select one configured message.");
        return;
    }

    MessageDefinition& message = m_messages[messageIndex];
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
            refreshConfiguredMessagesTable();
        }
        return;
    }
    const QString title = QString("Fields for %1").arg(message.messageName);
    if (configureFieldList(message.fields, message.payloadLengthBytes, title, &message.offsetUnit))
        refreshConfiguredMessagesTable();
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

    // Single flat message list: each message carries its own port + optional
    // header + length, and routing/export is handled by exportByMessageDefinitions.
    const QList<MessageDefinition> messages = m_messages;
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
    const QString outputDirectory = QFileDialog::getExistingDirectory(this,
                                                                      "Choose Excel Output Folder",
                                                                      AppPaths::outputFilesDir());
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

    // All transport settings (adapter / port / TCP role / multicast) live in the
    // Configure Connections… dialog. At least one connection must be defined.
    if (m_liveConnections.isEmpty())
    {
        QMessageBox::warning(this, "Live Capture",
            "No connections are defined.\n"
            "Solution: open 'Configure Connections…' and add at least one connection "
            "(adapter + port, UDP or TCP) before starting live capture.");
        return;
    }

    QString liveErr;
    if (!startLiveCaptureWithMessages(0, liveErr))
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
    ui->btnManageLiveLengthFilters->setEnabled(!busy && !m_liveRunning);
    ui->configuredMessagesGroup->setEnabled(!busy);
    ui->radFileMode->setEnabled(!busy);
    ui->radLiveMode->setEnabled(!busy);

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
    ui->configuredMessagesGroup->setEnabled(!running);
    ui->btnManageLiveLengthFilters->setEnabled(!running);
    ui->btnConfigureConnections->setEnabled(!running);
    ui->btnBrowse->setEnabled(!running);
    ui->btnStart->setEnabled(!running && ui->radFileMode->isChecked());
    // Refresh the live connection summary label.
    refreshLiveConnectionSummary();
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

    state.messages = m_messages;
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

    // v14 flat message list. Migrate older projects: flatten the old per-port and
    // per-header-row lists into the single list (stamping each port-row's port).
    m_messages = state.messages;
    if (m_messages.isEmpty())
    {
        for (int r = 0; r < state.portMessagesByRow.size(); ++r)
        {
            const QList<MessageDefinition>& rowMsgs = state.portMessagesByRow.at(r);
            int rowPort = 0;
            if (r < state.filterConfig.filters.size())
                rowPort = state.filterConfig.filters.at(r).port;
            for (int i = 0; i < rowMsgs.size(); ++i)
            {
                MessageDefinition m = rowMsgs.at(i);
                if (m.port == 0 && rowPort > 0)
                    m.port = static_cast<quint16>(rowPort);
                m_messages.append(m);
            }
        }
        for (int r = 0; r < state.headerMessagesByRow.size(); ++r)
        {
            const QList<MessageDefinition>& rowMsgs = state.headerMessagesByRow.at(r);
            for (int i = 0; i < rowMsgs.size(); ++i)
            {
                MessageDefinition m = rowMsgs.at(i);
                if (m.port == 0 && state.filterConfig.commonPort > 0)
                    m.port = static_cast<quint16>(state.filterConfig.commonPort);
                m_messages.append(m);
            }
        }
    }

    m_liveMessages = state.liveMessages;
    m_liveConnections = state.liveConnections;

    refreshConfiguredMessagesTable();
    refreshLiveLengthFilterStatus();
    refreshLiveConnectionSummary();
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
    ProjectState state;
    captureProjectState(state);

    // 1) Sidecar / explicit-project save (file mode): unchanged behaviour.
    const QString pcapPath = ui->txtFilePath->text().trimmed();
    QString savePath = m_projectPath;
    if (savePath.isEmpty() && !pcapPath.isEmpty())
        savePath = ProjectFile::sidecarPathFor(pcapPath);
    if (!savePath.isEmpty())
    {
        QString error;
        ProjectFile::save(state, savePath, error);
    }

    // 2) Live-mode autosave to the Projects folder so a live session (which has no
    // pcap file to anchor a sidecar) survives a restart. Written only in live mode.
    if (ui->radLiveMode->isChecked())
    {
        QString error;
        ProjectFile::save(state, liveAutosavePath(), error);
    }
}

QString MainWindow::liveAutosavePath() const
{
    return QDir(AppPaths::projectsDir()).filePath("live_autosave.pcproj.json");
}

void MainWindow::tryRestoreLiveAutosave()
{
    const QString path = liveAutosavePath();
    if (!ProjectFile::exists(path))
        return;

    ProjectState state;
    QString error;
    if (!ProjectFile::load(path, state, error))
        return;   // silent — a corrupt autosave must never block startup

    applyProjectState(state);
    setStatus(QString("Restored previous live-mode session from %1").arg(path));
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
    if (suggested.isEmpty())
        suggested = QDir(AppPaths::projectsDir()).filePath("project.pcproj.json");

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
        ui->lblLiveConnSummary->setText(
            "No connections defined — open Configure Connections… to add one");
    }
    else
    {
        ui->lblLiveConnSummary->setText(
            QString("%1 connection%2 defined").arg(n).arg(n == 1 ? "" : "s"));
    }
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

void MainWindow::openLiveLengthFilterDialog()
{
    // Transport/port now live in Configure Connections…; show the first connection's
    // port as informational context (0 when no connections are defined yet).
    const int port = m_liveConnections.isEmpty() ? 0 : static_cast<int>(m_liveConnections.first().port);

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

void MainWindow::refreshLiveLengthFilterStatus()
{
    const int count = m_liveMessages.size();
    ui->lblLiveLengthFilterStatus->setText(count == 0
        ? QString("No length filters")
        : QString("%1 message%2").arg(count).arg(count == 1 ? "" : "s"));
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
        AppPaths::outputFilesDir());
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

    Q_UNUSED(bindPort);
    QString socketError;
    // All transport settings live in Configure Connections…; start one receiver
    // per defined connection (startLiveCapture guarantees at least one exists).
    bool started = startSessionReceivers(socketError);
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
    const QString listenWhere = QString("%1 connection%2").arg(m_liveConnections.size())
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

    // Live mode: imported messages join the live length-filter set.
    if (ui->radLiveMode->isChecked())
    {
        for (int i = 0; i < messages.size(); ++i)
            m_liveMessages.append(messages.at(i));
        refreshLiveConfiguredMessagesTable();
        setStatus(QString("Imported %1 message(s) into Live mode.").arg(messages.size()));
        return;
    }

    // File mode: append to the single message list (each message keeps its own
    // port / length / optional header).
    for (int i = 0; i < messages.size(); ++i)
        m_messages.append(messages.at(i));
    refreshConfiguredMessagesTable();
    setStatus(QString("Imported %1 message(s).").arg(messages.size()));
}

QList<MessageDefinition> MainWindow::collectMessagesForJsonExport() const
{
    return ui->radLiveMode->isChecked() ? m_liveMessages : m_messages;
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
        "Export Messages to JSON",
        QDir(AppPaths::outputFilesDir()).filePath("messages.json"),
        "JSON Files (*.json)");
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
