#include "SimulatorWindow.h"
#include "ui_SimulatorWindow.h"

#include "AppPaths.h"
#include "CheckableComboBox.h"
#include "DataSender.h"
#include "HelpManualDialog.h"
#include "IcdDocxImporter.h"
#include "IcdImportDialog.h"
#include "MessageDefinitionDialog.h"
#include "MessageJsonCodec.h"
#include "NmeaFieldConfigurationDialog.h"
#include "PacketInspectorDialog.h"
#include "PayloadBuilder.h"
#include "PcapWriter.h"
#include "SerialDataSender.h"
#include "SimConnectionsDialog.h"
#include "SimFieldConfigurationDialog.h"
#include "TcpDataSender.h"
#include "Themes.h"
#include "UdpDataSender.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSet>
#include <QShortcut>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimer>

namespace
{
const int MSG_COL_SEND      = 0;
const int MSG_COL_NAME      = 1;
const int MSG_COL_FORMAT    = 2;
const int MSG_COL_LENGTH    = 3;
const int MSG_COL_RATE      = 4;
const int MSG_COL_FIELDS    = 5;
const int MSG_COL_CONFIGURE = 6;
const int MSG_COL_CONNECTION = 7;

const int PREVIEW_MAX_SHOWN_BYTES = 96;
} // namespace

SimulatorWindow::SimulatorWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_sending(false)
    , m_refreshingTable(false)
    , m_previewTimer(0)
    , m_previewDirty(false)
    , m_totalFramesSent(0)
    , ui(new Ui::SimulatorWindow)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->btnTheme->setText(Themes::currentMode() == Themes::Dark ? "Light Theme" : "Dark Theme");

    ui->tblMessages->setColumnCount(8);
    ui->tblMessages->setHorizontalHeaderLabels(QStringList()
        << "Send?" << "Message Name" << "Format" << "Length"
        << "Rate (Hz)" << "Fields" << "Configure Fields" << "Connection");
    ui->tblMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblMessages->setSelectionMode(QAbstractItemView::SingleSelection);

    // Outgoing-data history table.
    ui->tblHistory->setColumnCount(4);
    ui->tblHistory->setHorizontalHeaderLabels(QStringList() << "Time" << "Message" << "Bytes" << "Hex");
    ui->tblHistory->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblHistory->verticalHeader()->setVisible(false);

    // Multi-connection: send destinations are edited in SimConnectionsDialog and
    // opened on demand at Start Sending. Begin with one default UDP connection.
    ConnectionDefinition defaultConn;
    defaultConn.id = makeConnectionId();
    defaultConn.name = "Connection 1";
    defaultConn.transport = "UDP";
    defaultConn.host = "127.0.0.1";
    defaultConn.port = 5000;
    m_connections.append(defaultConn);

    connect(ui->btnConfigureConnection, SIGNAL(clicked()), this, SLOT(onConfigureConnectionClicked()));
    connect(ui->btnClearHistory, SIGNAL(clicked()), this, SLOT(onClearHistoryClicked()));

    connect(ui->btnAddMessage, SIGNAL(clicked()), this, SLOT(onAddMessageClicked()));
    connect(ui->btnEditMessage, SIGNAL(clicked()), this, SLOT(onEditMessageClicked()));
    connect(ui->btnRemoveMessage, SIGNAL(clicked()), this, SLOT(onRemoveMessageClicked()));
    connect(ui->btnImportIcd, SIGNAL(clicked()), this, SLOT(onImportIcdClicked()));
    connect(ui->actImportIcd, SIGNAL(triggered()), this, SLOT(onImportIcdClicked()));
    connect(ui->actImportMessagesJson, SIGNAL(triggered()), this, SLOT(onImportMessagesJsonClicked()));
    connect(ui->actExportMessagesJson, SIGNAL(triggered()), this, SLOT(onExportMessagesJsonClicked()));
    // JSON import/export buttons next to the message table (parity with the reader).
    connect(ui->btnImportMessagesJson, SIGNAL(clicked()), this, SLOT(onImportMessagesJsonClicked()));
    connect(ui->btnExportMessagesJson, SIGNAL(clicked()), this, SLOT(onExportMessagesJsonClicked()));
    // pcapng export of the sent-data history (button + Ctrl+E menu action).
    connect(ui->btnExportPcapng, SIGNAL(clicked()), this, SLOT(onExportPcapngClicked()));
    connect(ui->actExportPcapng, SIGNAL(triggered()), this, SLOT(onExportPcapngClicked()));
    // Double-click a history row to open the Wireshark-style packet inspector.
    connect(ui->tblHistory, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onHistoryDoubleClicked(int,int)));
    connect(ui->tblMessages, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onMessagesItemChanged(QTableWidgetItem*)));
    connect(ui->btnStartSending, SIGNAL(clicked()), this, SLOT(onStartSendingClicked()));
    connect(ui->btnStopSending, SIGNAL(clicked()), this, SLOT(onStopSendingClicked()));
    connect(ui->btnTheme, SIGNAL(clicked()), this, SLOT(onToggleThemeClicked()));
    connect(ui->actOpenSetup, SIGNAL(triggered()), this, SLOT(onOpenSetupClicked()));
    connect(ui->actSaveSetup, SIGNAL(triggered()), this, SLOT(onSaveSetupClicked()));
    connect(ui->actSaveSetupAs, SIGNAL(triggered()), this, SLOT(onSaveSetupAsClicked()));
    connect(ui->actShortcuts, SIGNAL(triggered()), this, SLOT(onShowShortcutsHelp()));
    connect(ui->actUserManual, SIGNAL(triggered()), this, SLOT(onShowUserManual()));
    connect(ui->actQuit, SIGNAL(triggered()), this, SLOT(close()));

    QShortcut* scSend = new QShortcut(QKeySequence(Qt::Key_F5), this, SLOT(onStartSendingClicked()));
    Q_UNUSED(scSend);
    QShortcut* scStop = new QShortcut(QKeySequence("Shift+F5"), this, SLOT(onStopSendingClicked()));
    Q_UNUSED(scStop);
    QShortcut* scTheme = new QShortcut(QKeySequence("Ctrl+T"), this, SLOT(onToggleThemeClicked()));
    Q_UNUSED(scTheme);

    // GUI-side preview flush: send timers only queue text; this 200 ms timer
    // pushes it to the widget so a 1000 Hz stream cannot choke the GUI thread.
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(200);
    connect(m_previewTimer, SIGNAL(timeout()), this, SLOT(onPreviewFlushTick()));
    m_previewTimer->start();

    refreshConnectionBar();   // initialise the connection bar
    refreshMessagesTable();

    // Periodic auto-save to the Projects folder (in addition to the close-event
    // save) so a long unattended session is never lost.
    QTimer* autosaveTimer = new QTimer(this);
    autosaveTimer->setInterval(30000);
    connect(autosaveTimer, &QTimer::timeout, this, [this]() {
        QString err;
        SimSetupFile::save(captureSetup(),
                           QDir(AppPaths::projectsDir()).filePath("simulator_autosave.json"), err);
    });
    autosaveTimer->start();

    // Silent auto-restore of the last session's setup. Prefer the Projects-folder
    // autosave; fall back to the legacy AppData autosave for older installs.
    const QString projAutosave = QDir(AppPaths::projectsDir()).filePath("simulator_autosave.json");
    if (QFile::exists(projAutosave))
        loadSetupFromPath(projAutosave, true);
    else if (QFile::exists(SimSetupFile::autoSavePath()))
        loadSetupFromPath(SimSetupFile::autoSavePath(), true);
}

SimulatorWindow::~SimulatorWindow()
{
    delete ui;
}

void SimulatorWindow::closeEvent(QCloseEvent* event)
{
    if (m_sending)
        onStopSendingClicked();

    // Auto-save the whole setup (connections + messages + values) to the Projects
    // folder so it persists across rebuilds and is easy to find.
    QString error;
    SimSetupFile::save(captureSetup(),
                       QDir(AppPaths::projectsDir()).filePath("simulator_autosave.json"), error);

    event->accept();
}

void SimulatorWindow::setBarDot(const QString& state)
{
    QString color = "#94A3B8"; // gray
    if (state == "green")
        color = "#22C55E";
    else if (state == "red")
        color = "#EF4444";
    ui->lblLinkDot->setStyleSheet(QString("color:%1; font-size:16pt; background:transparent; border:none;").arg(color));
}

void SimulatorWindow::onConfigureConnectionClicked()
{
    if (m_sending)
    {
        QMessageBox::information(this, "Connections",
            "Stop sending before changing connections.");
        return;
    }

    SimConnectionsDialog dlg(this);
    dlg.setConnections(m_connections);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_connections = dlg.connections();

    // Drop bindings to connections that no longer exist so a message never points
    // at a deleted destination (it falls back to the default connection instead).
    QStringList validIds;
    for (int i = 0; i < m_connections.size(); ++i)
        validIds << m_connections.at(i).id;
    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (!m_messages.at(i).connectionId.isEmpty()
            && !validIds.contains(m_messages.at(i).connectionId))
            m_messages[i].connectionId.clear();

        // Prune multi-select bindings to deleted connections too (a now-empty
        // list falls back to the default connection).
        QStringList kept;
        for (int k = 0; k < m_messages.at(i).connectionIds.size(); ++k)
        {
            const QString id = m_messages.at(i).connectionIds.at(k);
            if (validIds.contains(id))
                kept << id;
        }
        m_messages[i].connectionIds = kept;
    }

    refreshConnectionBar();
    refreshMessagesTable();
}

void SimulatorWindow::refreshConnectionBar()
{
    const int n = m_connections.size();
    if (n == 0)
        ui->lblConnName->setText("No connections — press Configure…");
    else if (n == 1)
        ui->lblConnName->setText(QString("1 connection: %1").arg(m_connections.first().name));
    else
        ui->lblConnName->setText(QString("%1 connections").arg(n));
    setBarDot(m_sending ? "green" : "gray");
}

void SimulatorWindow::onSenderLinkError(const QString& message)
{
    // An async link failure (cable pulled, server dropped) on any open sender
    // stops the whole run and reports the reason.
    if (m_sending)
    {
        stopAllSendTimers();
        m_sending = false;
        setSendingUiState(false);
        closeAllSenders();
        setBarDot("red");
        ui->lblSendStats->setText("Stopped — a connection was lost.");
        QMessageBox::warning(this, "Connection Lost", message);
    }
}

void SimulatorWindow::onClearHistoryClicked()
{
    ui->tblHistory->setRowCount(0);
    m_historyPending.clear();
    m_sentRecords.clear();
    ui->lblHistoryCount->setText("0 frames");
}

void SimulatorWindow::onExportPcapngClicked()
{
    if (m_sentRecords.isEmpty())
    {
        QMessageBox::information(this, "Export pcapng",
            "There are no sent packets in the history to export.\n"
            "Solution: send at least one message first (Send / F5), then export.");
        return;
    }

    const QString defaultName = QString("simulator_%1.pcapng")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString path = QFileDialog::getSaveFileName(this,
        "Export Sent Data to pcapng",
        QDir(AppPaths::outputFilesDir()).filePath(defaultName),
        "pcapng capture (*.pcapng)");
    if (path.isEmpty())
        return;

    PcapWriter writer;
    QString error;
    if (!writer.openPcapng(path, error))
    {
        QMessageBox::warning(this, "Export pcapng", error);
        return;
    }

    int written = 0;
    int skipped = 0;
    const qint64 baseMs = QDateTime::currentDateTime().toMSecsSinceEpoch()
                          - static_cast<qint64>(m_sentRecords.size());
    for (int i = 0; i < m_sentRecords.size(); ++i)
    {
        const SentRecord& rec = m_sentRecords.at(i);
        if (rec.transport.compare("SERIAL", Qt::CaseInsensitive) == 0)
        {
            ++skipped;   // serial has no IP framing; cannot be a pcapng Ethernet frame
            continue;
        }
        const QByteArray frame = (rec.transport.compare("TCP", Qt::CaseInsensitive) == 0)
            ? PcapFrame::buildEthIpTcp(rec.srcIp, rec.srcPort, rec.dstIp, rec.dstPort, rec.payload)
            : PcapFrame::buildEthIpUdp(rec.srcIp, rec.srcPort, rec.dstIp, rec.dstPort, rec.payload);
        const quint64 tsUsec = static_cast<quint64>(baseMs + i) * 1000ULL;
        if (!writer.writePacket(tsUsec, frame, error))
        {
            writer.close();
            QMessageBox::warning(this, "Export pcapng", error);
            return;
        }
        ++written;
    }
    writer.close();

    QString msg = QString("Exported %1 packet(s) to:\n%2\n\n"
                          "Synthesized Ethernet/IPv4/UDP|TCP — opens in Wireshark and in the reader.")
                      .arg(written).arg(path);
    if (skipped > 0)
        msg += QString("\n\n%1 serial frame(s) were skipped (serial has no IP framing).").arg(skipped);
    QMessageBox::information(this, "Export pcapng", msg);
}

void SimulatorWindow::onHistoryDoubleClicked(int row, int /*column*/)
{
    if (row < 0 || row >= m_sentRecords.size())
        return;
    const SentRecord& rec = m_sentRecords.at(row);

    // Find the current message with this name for the field breakdown (best-effort;
    // an empty definition still shows the protocol tree + hex dump).
    MessageDefinition message;
    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (m_messages.at(i).messageName == rec.messageName)
        {
            message = m_messages.at(i);
            break;
        }
    }

    PacketInspectorDialog dlg(rec.transport, rec.srcIp, rec.srcPort, rec.dstIp, rec.dstPort,
                              rec.payload, rec.messageName, message, this);
    dlg.exec();
}

void SimulatorWindow::refreshMessagesTable()
{
    m_refreshingTable = true;
    ui->tblMessages->setRowCount(0);

    for (int i = 0; i < m_messages.size(); ++i)
    {
        const MessageDefinition& message = m_messages.at(i);
        const int row = ui->tblMessages->rowCount();
        ui->tblMessages->insertRow(row);

        QTableWidgetItem* sendItem = new QTableWidgetItem();
        sendItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        sendItem->setCheckState(message.sendEnabled ? Qt::Checked : Qt::Unchecked);
        sendItem->setToolTip("Tick to include this message when Send starts.");
        ui->tblMessages->setItem(row, MSG_COL_SEND, sendItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(message.messageName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->tblMessages->setItem(row, MSG_COL_NAME, nameItem);

        const bool nmea = (message.dataFormat == "NMEA");
        QTableWidgetItem* formatItem = new QTableWidgetItem(
            nmea ? QString("NMEA (%1)").arg(message.nmeaSentenceType) : QString("HEX"));
        formatItem->setFlags(formatItem->flags() & ~Qt::ItemIsEditable);
        ui->tblMessages->setItem(row, MSG_COL_FORMAT, formatItem);

        QTableWidgetItem* lengthItem = new QTableWidgetItem(
            nmea ? QString("-") : QString::number(message.payloadLengthBytes));
        lengthItem->setFlags(lengthItem->flags() & ~Qt::ItemIsEditable);
        ui->tblMessages->setItem(row, MSG_COL_LENGTH, lengthItem);

        QTableWidgetItem* rateItem = new QTableWidgetItem(QString::number(message.sendFrequencyHz));
        rateItem->setFlags(rateItem->flags() & ~Qt::ItemIsEditable);
        ui->tblMessages->setItem(row, MSG_COL_RATE, rateItem);

        QTableWidgetItem* fieldsItem = new QTableWidgetItem(
            message.fields.isEmpty() ? QString("No fields") : QString::number(message.fields.size()));
        fieldsItem->setFlags(fieldsItem->flags() & ~Qt::ItemIsEditable);
        ui->tblMessages->setItem(row, MSG_COL_FIELDS, fieldsItem);

        QPushButton* configureButton = new QPushButton("Configure Fields", ui->tblMessages);
        configureButton->setProperty("messageIndex", i);
        connect(configureButton, SIGNAL(clicked()), this, SLOT(onConfigureFieldsButtonClicked()));
        ui->tblMessages->setCellWidget(row, MSG_COL_CONFIGURE, configureButton);

        // Per-message connection binding (multi-select). Tick every connection
        // this message should be transmitted to; none ticked = the default
        // (first) connection. The combo lists every send connection.
        CheckableComboBox* connCombo = new CheckableComboBox(ui->tblMessages);
        for (int c = 0; c < m_connections.size(); ++c)
        {
            const ConnectionDefinition& conn = m_connections.at(c);
            const QString label = (c == 0)
                ? QString("%1 (default)").arg(conn.name) : conn.name;
            connCombo->addCheckItem(label, conn.id, false);
        }
        // Tick the message's explicit destinations (a legacy single binding shows
        // as one tick); an empty list shows "Default (first)".
        connCombo->setCheckedData(messageConnectionIds(message));
        connCombo->setToolTip("Tick every connection this message is sent to "
                              "(it can go to several at once). None ticked = the "
                              "default (first) connection.");
        connCombo->setProperty("messageIndex", i);
        connect(connCombo, SIGNAL(checkedItemsChanged()), this, SLOT(onMessageConnectionsChanged()));
        ui->tblMessages->setCellWidget(row, MSG_COL_CONNECTION, connCombo);
    }

    m_refreshingTable = false;

    ui->tblMessages->resizeColumnsToContents();
    ui->tblMessages->horizontalHeader()->setStretchLastSection(true);
}

int SimulatorWindow::selectedMessageRow() const
{
    QList<QTableWidgetItem*> selectedItems = ui->tblMessages->selectedItems();
    if (!selectedItems.isEmpty())
        return selectedItems.first()->row();
    return ui->tblMessages->currentRow();
}

bool SimulatorWindow::messageNameInUse(const QString& name, int ignoreIndex) const
{
    const QString normalized = name.trimmed().toLower();
    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (i == ignoreIndex)
            continue;
        if (m_messages.at(i).messageName.trimmed().toLower() == normalized)
            return true;
    }
    return false;
}

void SimulatorWindow::onMessageConnectionsChanged()
{
    if (m_refreshingTable)
        return;
    CheckableComboBox* combo = qobject_cast<CheckableComboBox*>(sender());
    if (!combo)
        return;
    const int row = combo->property("messageIndex").toInt();
    if (row < 0 || row >= m_messages.size())
        return;
    // Store the explicit ticked set as the message's destinations. None ticked =
    // empty list = the default (first) connection. connectionId is kept synced to
    // the first id so the single-binding field and old readers stay consistent.
    const QStringList ids = combo->checkedData();
    m_messages[row].connectionIds = ids;
    m_messages[row].connectionId = ids.isEmpty() ? QString() : ids.first();
    // Connection changes take effect on the next Start (mid-stream we cannot open
    // a brand-new sender for a freshly added destination); the others keep going.
}

void SimulatorWindow::onMessagesItemChanged(QTableWidgetItem* item)
{
    if (m_refreshingTable || !item)
        return;

    if (item->column() == MSG_COL_SEND)
    {
        const int row = item->row();
        if (row >= 0 && row < m_messages.size())
        {
            m_messages[row].sendEnabled = (item->checkState() == Qt::Checked);
            if (m_sending)
                rebuildActiveSend(row); // start/stop just this stream, live
        }
    }
}

void SimulatorWindow::onAddMessageClicked()
{
    MessageDefinitionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (messageNameInUse(dlg.messageName(), -1))
    {
        QMessageBox::warning(this, "Duplicate Message",
            QString("A message named '%1' already exists.\nSolution: give every message a unique name.")
                .arg(dlg.messageName()));
        return;
    }

    MessageDefinition message;
    message.messageName = dlg.messageName();
    message.payloadLengthBytes = dlg.payloadLengthBytes();
    message.dataFormat = dlg.dataFormat();
    message.nmeaSentenceType = dlg.nmeaSentenceType();
    message.nmeaTalker = dlg.nmeaTalker();
    message.sendFrequencyHz = dlg.sendFrequencyHz();
    message.sendEnabled = true;

    m_messages.append(message);
    refreshMessagesTable();
}

void SimulatorWindow::onEditMessageClicked()
{
    const int row = selectedMessageRow();
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Edit Message", "Select a message row first.");
        return;
    }

    const MessageDefinition& existing = m_messages.at(row);

    MessageDefinitionDialog dlg(this);
    dlg.setMessageName(existing.messageName);
    dlg.setPayloadLength(existing.payloadLengthBytes);
    dlg.setNmeaSentenceType(existing.nmeaSentenceType);
    dlg.setNmeaTalker(existing.nmeaTalker);
    dlg.setSendFrequencyHz(existing.sendFrequencyHz);
    dlg.setDataFormat(existing.dataFormat);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (messageNameInUse(dlg.messageName(), row))
    {
        QMessageBox::warning(this, "Duplicate Message",
            QString("A message named '%1' already exists.\nSolution: give every message a unique name.")
                .arg(dlg.messageName()));
        return;
    }

    MessageDefinition& message = m_messages[row];
    message.messageName = dlg.messageName();
    message.payloadLengthBytes = dlg.payloadLengthBytes();
    message.dataFormat = dlg.dataFormat();
    message.nmeaSentenceType = dlg.nmeaSentenceType();
    message.nmeaTalker = dlg.nmeaTalker();
    message.sendFrequencyHz = dlg.sendFrequencyHz();

    refreshMessagesTable();
    if (m_sending)
        rebuildActiveSend(row); // apply the edited definition to the live stream
}

void SimulatorWindow::onRemoveMessageClicked()
{
    const int row = selectedMessageRow();
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Remove Message", "Select a message row first.");
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(this,
        "Remove Message",
        QString("Remove message '%1' and its fields?").arg(m_messages.at(row).messageName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_messages.removeAt(row);
    refreshMessagesTable();
}

void SimulatorWindow::onImportIcdClicked()
{
    if (m_sending)
    {
        QMessageBox::information(this, "Import ICD",
            "Stop sending before importing an ICD. Solution: press Stop, then import.");
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this,
        "Import ICD (Word .docx)", QString(),
        "Word documents (*.docx);;All files (*.*)");
    if (path.isEmpty())
        return;

    IcdDocument doc;
    QString error;
    if (!IcdDocxImporter::extract(path, doc, error))
    {
        QMessageBox::warning(this, "Import ICD",
            QString("Could not read the ICD:\n\n%1").arg(error));
        return;
    }
    if (doc.tables.isEmpty())
    {
        QMessageBox::information(this, "Import ICD",
            "No tables were found in this .docx. Solution: make sure the ICD's field "
            "definitions are laid out in Word tables.");
        return;
    }

    IcdImportDialog dlg(this);
    dlg.setDocument(doc);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QList<MessageDefinition> imported = dlg.selectedMessages();
    if (imported.isEmpty())
        return;

    // Append imported messages, auto-renaming any whose name collides with an
    // existing message (or an earlier import in this batch) — ttd -> ttd_1, ...
    QStringList renames;
    for (int i = 0; i < imported.size(); ++i)
    {
        MessageDefinition m = imported.at(i);
        if (messageNameInUse(m.messageName, -1))
        {
            const QString base = m.messageName;
            QString unique = base;
            for (int n = 1; messageNameInUse(unique, -1); ++n)
                unique = QString("%1_%2").arg(base).arg(n);
            renames << QString("'%1' → '%2'").arg(base).arg(unique);
            m.messageName = unique;
        }
        m.sendEnabled = true;
        m_messages.append(m);
    }
    refreshMessagesTable();

    QString summary = QString("Imported %1 message(s) from the ICD. Review each message's "
                              "fields and fill in the values to transmit before sending.")
                          .arg(imported.size());
    if (!renames.isEmpty())
        summary += QString("\n\nRenamed %1 message(s) to avoid duplicate names:\n%2")
                       .arg(renames.size()).arg(renames.join("\n"));
    QMessageBox::information(this, "Import ICD", summary);
}

void SimulatorWindow::onExportMessagesJsonClicked()
{
    if (m_messages.isEmpty())
    {
        QMessageBox::information(this, "Export Messages (JSON)",
            "There are no messages to export. Solution: add or import at least one message first.");
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
    file.write(MessageJsonCodec::messagesToJson(m_messages).toUtf8());
    file.close();

    QMessageBox::information(this, "Export Messages (JSON)",
        QString("Exported %1 message(s) to:\n%2").arg(m_messages.size()).arg(path));
}

void SimulatorWindow::onImportMessagesJsonClicked()
{
    if (m_sending)
    {
        QMessageBox::information(this, "Import Messages (JSON)",
            "Stop sending before importing. Solution: press Stop, then import.");
        return;
    }

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

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Import Messages (JSON)");
    box.setText(QString("Imported %1 message(s). Replace the current list, or append?")
                    .arg(imported.size()));
    QPushButton* replaceBtn = box.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton* appendBtn  = box.addButton("Append",  QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(appendBtn);
    box.exec();

    if (box.clickedButton() == replaceBtn)
        m_messages.clear();
    else if (box.clickedButton() != appendBtn)
        return;

    QStringList renames;
    for (int i = 0; i < imported.size(); ++i)
    {
        MessageDefinition m = imported.at(i);
        const QString base = m.messageName.trimmed().isEmpty() ? QString("message") : m.messageName;
        if (messageNameInUse(base, -1))
        {
            QString unique = base;
            for (int n = 1; messageNameInUse(unique, -1); ++n)
                unique = QString("%1_%2").arg(base).arg(n);
            renames << QString("'%1' → '%2'").arg(m.messageName).arg(unique);
            m.messageName = unique;
        }
        else
        {
            m.messageName = base;
        }
        m_messages.append(m);
    }
    refreshMessagesTable();

    QString summary = QString("Imported %1 message(s).").arg(imported.size());
    if (!renames.isEmpty())
        summary += QString("\n\nRenamed %1 to avoid duplicate names:\n%2")
                       .arg(renames.size()).arg(renames.join("\n"));
    QMessageBox::information(this, "Import Messages (JSON)", summary);
}

void SimulatorWindow::onConfigureFieldsButtonClicked()
{
    QObject* senderObject = sender();
    if (!senderObject)
        return;

    const int index = senderObject->property("messageIndex").toInt();
    if (index < 0 || index >= m_messages.size())
        return;

    MessageDefinition& message = m_messages[index];

    if (message.dataFormat == "NMEA")
    {
        NmeaFieldConfigurationDialog dlg(this);
        dlg.setSentenceType(message.nmeaSentenceType);
        dlg.setExistingConfig(message.fields);
        if (dlg.exec() == QDialog::Accepted)
        {
            message.fields = dlg.fieldConfig();
            refreshMessagesTable();
            if (m_sending)
                rebuildActiveSend(index); // live-update the stream with the new values
        }
        return;
    }

    SimFieldConfigurationDialog dlg(this);
    dlg.setPayloadLength(message.payloadLengthBytes);
    dlg.setOffsetUnit(message.offsetUnit);
    dlg.setFields(message.fields);
    if (dlg.exec() == QDialog::Accepted)
    {
        message.fields = dlg.fields();
        message.offsetUnit = dlg.offsetUnit();
        refreshMessagesTable();
        if (m_sending)
            rebuildActiveSend(index); // live-update the stream with the new values
    }
}

// Verify one message and build its payload, appending any problems (each with a
// reason + solution). Shared by the pre-send verify and the live-edit rebuild.
bool SimulatorWindow::buildOneMessage(int messageIndex, QByteArray& payload, QStringList& problems)
{
    payload.clear();
    if (messageIndex < 0 || messageIndex >= m_messages.size())
        return false;

    const MessageDefinition& message = m_messages.at(messageIndex);
    const int problemCountBefore = problems.size();
    QList<ConnectionDefinition> destinations;
    connectionsForMessage(messageIndex, destinations);
    bool udpDestination = false;
    for (int d = 0; d < destinations.size(); ++d)
    {
        if (destinations.at(d).transport == "UDP")
        {
            udpDestination = true;
            break;
        }
    }

    if (message.fields.isEmpty())
        problems.append(QString("Message '%1': no fields are defined. Solution: press Configure Fields and add at least one field with a value.")
                            .arg(message.messageName));

    if (message.sendFrequencyHz <= 0.0 || message.sendFrequencyHz > 1000.0)
        problems.append(QString("Message '%1': the send rate %2 Hz is out of range. Solution: use 0.001 to 1000 Hz (the timer cannot fire faster than once per millisecond).")
                            .arg(message.messageName).arg(message.sendFrequencyHz));

    if (message.dataFormat == "NMEA")
        PayloadBuilder::buildNmeaSentence(message, payload, problems);
    else
        PayloadBuilder::buildHexPayload(message, payload, problems);

    if (udpDestination && payload.size() > UdpDataSender::maxDatagramBytes())
        problems.append(QString("Message '%1': the payload is %2 bytes but a single UDP datagram can carry at most %3 bytes. Solution: reduce the message's Payload Length.")
                            .arg(message.messageName).arg(payload.size()).arg(UdpDataSender::maxDatagramBytes()));

    return problems.size() == problemCountBefore;
}

// "Verify everything before sending the data": every problem is collected
// with a reason + solution; nothing is transmitted unless ALL pass. Payloads
// are pre-built here (and refreshed live by rebuildActiveSend while streaming).
bool SimulatorWindow::verifyBeforeSend(QList<ActiveSend>& plan, QStringList& problems)
{
    plan.clear();

    if (m_connections.isEmpty())
        problems.append("No send connections are defined. Solution: press Configure… on the Connection bar and add at least one UDP / TCP / Serial connection.");

    QList<int> tickedIndices;
    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (m_messages.at(i).sendEnabled)
            tickedIndices.append(i);
    }
    if (tickedIndices.isEmpty())
        problems.append("No message is ticked for sending. Solution: tick the Send? box of at least one message.");

    for (int t = 0; t < tickedIndices.size(); ++t)
    {
        const int index = tickedIndices.at(t);
        QByteArray payload;
        if (buildOneMessage(index, payload, problems))
        {
            ActiveSend send;
            send.messageIndex = index;
            send.payload = payload;
            plan.append(send);
        }
    }

    return problems.isEmpty();
}

void SimulatorWindow::onStartSendingClicked()
{
    if (m_sending)
        return;

    QList<ActiveSend> plan;
    QStringList problems;
    if (!verifyBeforeSend(plan, problems))
    {
        showProblems("Cannot Send", problems);
        return;
    }

    // Open (and health-check) every connection the plan needs before streaming.
    if (!openSendersForPlan(plan, problems))
    {
        showProblems("Cannot Send", problems);
        return;
    }

    m_activeSends = plan;
    m_sending = true;
    m_totalFramesSent = 0;
    setBarDot("green");
    setSendingUiState(true);
    ui->lblSendStats->setText(QString("Sending %1 message(s)...").arg(m_activeSends.size()));

    for (int i = 0; i < m_activeSends.size(); ++i)
    {
        const MessageDefinition& message = m_messages.at(m_activeSends.at(i).messageIndex);

        QTimer* timer = new QTimer(this);
        timer->setTimerType(Qt::PreciseTimer);
        timer->setInterval(qMax(1, qRound(1000.0 / message.sendFrequencyHz)));
        // The stable message index (not the plan position) so live add/remove of
        // streams cannot invalidate a running timer's target.
        timer->setProperty("messageIndex", m_activeSends.at(i).messageIndex);
        connect(timer, SIGNAL(timeout()), this, SLOT(onSendTimerTick()));
        m_activeSends[i].timer = timer;
    }

    // First frame of every message goes out immediately; the timers handle
    // the re-sends at each message's own rate.
    for (int i = 0; i < m_activeSends.size(); ++i)
    {
        if (!sendActive(i))
            return; // sendActive already stopped everything and reported
    }
    for (int i = 0; i < m_activeSends.size(); ++i)
    {
        if (m_activeSends.at(i).timer)
            m_activeSends[i].timer->start();
    }
}

void SimulatorWindow::onStopSendingClicked()
{
    if (!m_sending)
        return;

    stopAllSendTimers();
    m_sending = false;
    setSendingUiState(false);
    closeAllSenders();
    setBarDot("gray");
    ui->lblSendStats->setText(QString("Stopped — %1 frame(s) sent.").arg(m_totalFramesSent));
}

void SimulatorWindow::onSendTimerTick()
{
    QObject* senderObject = sender();
    if (!senderObject)
        return;
    const int planIndex = activeIndexForMessage(senderObject->property("messageIndex").toInt());
    if (planIndex >= 0)
        sendActive(planIndex);
}

int SimulatorWindow::activeIndexForMessage(int messageIndex) const
{
    for (int i = 0; i < m_activeSends.size(); ++i)
        if (m_activeSends.at(i).messageIndex == messageIndex)
            return i;
    return -1;
}

// Apply an edit to a message that is (or should be) streaming, without stopping
// the other streams. Called from the field/message editors and the Send? tick.
void SimulatorWindow::rebuildActiveSend(int messageIndex)
{
    if (!m_sending || messageIndex < 0 || messageIndex >= m_messages.size())
        return;

    const MessageDefinition& message = m_messages.at(messageIndex);
    const int planIdx = activeIndexForMessage(messageIndex);

    // Unticked while streaming → stop just this message's stream.
    if (!message.sendEnabled)
    {
        if (planIdx >= 0)
        {
            if (m_activeSends[planIdx].timer)
            {
                m_activeSends[planIdx].timer->stop();
                m_activeSends[planIdx].timer->deleteLater();
            }
            m_activeSends.removeAt(planIdx);
        }
        return;
    }

    QByteArray payload;
    QStringList problems;
    if (!buildOneMessage(messageIndex, payload, problems))
    {
        // Keep the previously-good payload streaming; just surface the problem.
        showProblems("Live update skipped — the message keeps streaming its last valid values", problems);
        return;
    }

    const int interval = qMax(1, qRound(1000.0 / message.sendFrequencyHz));

    if (planIdx >= 0)
    {
        // Already streaming → swap the frozen payload in place (no gap) and
        // retune the rate if it changed.
        m_activeSends[planIdx].payload = payload;
        if (m_activeSends[planIdx].timer && m_activeSends[planIdx].timer->interval() != interval)
            m_activeSends[planIdx].timer->setInterval(interval);
    }
    else
    {
        // Newly ticked while streaming → start a stream for it immediately.
        ActiveSend send;
        send.messageIndex = messageIndex;
        send.payload = payload;
        QTimer* timer = new QTimer(this);
        timer->setTimerType(Qt::PreciseTimer);
        timer->setInterval(interval);
        timer->setProperty("messageIndex", messageIndex);
        connect(timer, SIGNAL(timeout()), this, SLOT(onSendTimerTick()));
        send.timer = timer;
        m_activeSends.append(send);
        if (sendActive(m_activeSends.size() - 1)) // first frame now
            timer->start();
    }
}

bool SimulatorWindow::connectionsForMessage(int messageIndex, QList<ConnectionDefinition>& out) const
{
    out.clear();
    if (m_connections.isEmpty() || messageIndex < 0 || messageIndex >= m_messages.size())
        return false;

    const QStringList ids = messageConnectionIds(m_messages.at(messageIndex));
    QSet<QString> seen;
    for (int k = 0; k < ids.size(); ++k)
    {
        const QString id = ids.at(k);
        if (id.isEmpty() || seen.contains(id))
            continue;
        for (int i = 0; i < m_connections.size(); ++i)
        {
            if (m_connections.at(i).id == id)
            {
                out.append(m_connections.at(i));
                seen.insert(id);
                break;
            }
        }
    }

    // Unbound, or every bound id was stale → the first (default) connection.
    if (out.isEmpty())
        out.append(m_connections.first());
    return true;
}

bool SimulatorWindow::openSendersForPlan(const QList<ActiveSend>& plan, QStringList& problems)
{
    // Open exactly one sender per distinct connection the plan references (a
    // single message may target several), and health-check each. One failure
    // aborts and closes everything opened.
    for (int i = 0; i < plan.size(); ++i)
    {
        QList<ConnectionDefinition> conns;
        if (!connectionsForMessage(plan.at(i).messageIndex, conns) || conns.isEmpty())
        {
            problems.append("A ticked message could not be matched to a connection. Solution: define at least one connection.");
            closeAllSenders();
            return false;
        }

        for (int c = 0; c < conns.size(); ++c)
        {
            const ConnectionDefinition& conn = conns.at(c);
            if (m_openSenders.contains(conn.id))
                continue;   // already opened for an earlier message/destination

            DataSender* sender = SimConnectionsDialog::buildSender(conn, this);
            connect(sender, SIGNAL(linkError(QString)), this, SLOT(onSenderLinkError(QString)));
            QString error;
            if (!sender->open(error) || !sender->send(DataSender::healthMessage(), error))
            {
                problems.append(QString("Connection '%1' could not open: %2").arg(conn.name).arg(error));
                sender->close();
                sender->deleteLater();
                closeAllSenders();
                return false;
            }
            m_openSenders.insert(conn.id, sender);
        }
    }
    return true;
}

void SimulatorWindow::closeAllSenders()
{
    for (QMap<QString, DataSender*>::iterator it = m_openSenders.begin(); it != m_openSenders.end(); ++it)
    {
        if (it.value())
        {
            it.value()->close();
            it.value()->deleteLater();
        }
    }
    m_openSenders.clear();
}

bool SimulatorWindow::sendActive(int planIndex)
{
    if (planIndex < 0 || planIndex >= m_activeSends.size())
        return false;

    const int messageIndex = m_activeSends.at(planIndex).messageIndex;
    QList<ConnectionDefinition> conns;
    if (!connectionsForMessage(messageIndex, conns) || conns.isEmpty())
        return false;

    const QByteArray payload = m_activeSends.at(planIndex).payload;
    bool anySent = false;

    // Transmit one frame to every bound destination; each counts as a frame and
    // gets its own history line. A failure on any link stops the whole run.
    for (int c = 0; c < conns.size(); ++c)
    {
        DataSender* sender = m_openSenders.value(conns.at(c).id, 0);
        if (!sender)
            continue;   // a destination added mid-stream that was never opened

        QString error;
        if (!sender->send(payload, error))
        {
            stopAllSendTimers();
            m_sending = false;
            setSendingUiState(false);
            closeAllSenders();
            setBarDot("red");
            ui->lblSendStats->setText("Stopped (send error).");
            QMessageBox::warning(this, "Send Failed", error);
            return false;
        }

        // m_activeSends may have been reallocated by a re-entrant edit; re-find.
        const int pi = activeIndexForMessage(messageIndex);
        if (pi >= 0)
            m_activeSends[pi].count += 1;
        m_totalFramesSent += 1;
        pushPreviewLine(messageIndex, payload, conns.at(c));
        anySent = true;
    }
    return anySent;
}

void SimulatorWindow::stopAllSendTimers()
{
    for (int i = 0; i < m_activeSends.size(); ++i)
    {
        if (m_activeSends.at(i).timer)
        {
            m_activeSends[i].timer->stop();
            m_activeSends[i].timer->deleteLater();
            m_activeSends[i].timer = 0;
        }
    }
    m_activeSends.clear();
}

void SimulatorWindow::setSendingUiState(bool sending)
{
    ui->btnStartSending->setEnabled(!sending);
    ui->btnStopSending->setEnabled(sending);
    // The Messages group stays interactive while streaming so values, rates and
    // the Send? tick can be edited live (rebuildActiveSend applies them without a
    // gap). Only Add / Remove / Import are locked, because they would renumber the
    // message indices that the running timers target.
    ui->grpMessages->setEnabled(true);
    ui->btnAddMessage->setEnabled(!sending);
    ui->btnRemoveMessage->setEnabled(!sending);
    ui->btnImportIcd->setEnabled(!sending);
    ui->actImportIcd->setEnabled(!sending);
    ui->actOpenSetup->setEnabled(!sending);
}

void SimulatorWindow::pushPreviewLine(int messageIndex, const QByteArray& payload,
                                     const ConnectionDefinition& conn)
{
    // Build the full sent-packet record (raw bytes + the resolved destination) so
    // it can later be exported to pcapng and inspected. The 200 ms flush moves
    // these to the table so a 1000 Hz stream cannot choke the GUI thread.
    SentRecord rec;
    rec.timeText = QTime::currentTime().toString("hh:mm:ss.zzz");
    rec.messageName = (messageIndex >= 0 && messageIndex < m_messages.size())
                          ? m_messages.at(messageIndex).messageName : QString();
    rec.payload = payload;
    rec.transport = conn.transport;
    rec.dstIp = conn.host;
    rec.dstPort = conn.port;
    rec.srcIp = conn.adapterAddress.isEmpty() ? QString("0.0.0.0") : conn.adapterAddress;
    rec.srcPort = 49152;   // synthetic ephemeral source port for the capture

    m_historyPending.append(rec);
    m_previewDirty = true;
}

void SimulatorWindow::onPreviewFlushTick()
{
    if (!m_previewDirty)
        return;
    m_previewDirty = false;

    if (!m_historyPending.isEmpty())
    {
        QTableWidget* t = ui->tblHistory;
        for (int i = 0; i < m_historyPending.size(); ++i)
        {
            const SentRecord& rec = m_historyPending.at(i);
            const int shownBytes = qMin(rec.payload.size(), PREVIEW_MAX_SHOWN_BYTES);
            QString hex = QString::fromLatin1(rec.payload.left(shownBytes).toHex(' ').toUpper());
            if (rec.payload.size() > shownBytes)
                hex += QString(" … (+%1 more bytes)").arg(rec.payload.size() - shownBytes);

            const int r = t->rowCount();
            t->insertRow(r);
            t->setItem(r, 0, new QTableWidgetItem(rec.timeText));
            t->setItem(r, 1, new QTableWidgetItem(rec.messageName));
            t->setItem(r, 2, new QTableWidgetItem(QString::number(rec.payload.size())));
            t->setItem(r, 3, new QTableWidgetItem(hex));

            // Keep the record buffer (pcapng export + inspector) in lockstep.
            m_sentRecords.append(rec);
        }
        m_historyPending.clear();

        // Trim to the configured maximum (drop oldest rows from the top), in both
        // the table and the parallel record buffer.
        const int maxRows = ui->spinHistoryMax->value();
        while (t->rowCount() > maxRows)
        {
            t->removeRow(0);
            if (!m_sentRecords.isEmpty())
                m_sentRecords.removeFirst();
        }

        if (ui->chkAutoScroll->isChecked())
            t->scrollToBottom();
        ui->lblHistoryCount->setText(QString("%1 frames").arg(static_cast<qulonglong>(m_totalFramesSent)));
    }

    if (m_sending)
        ui->lblSendStats->setText(QString("Sending %1 message(s) — %2 frame(s) sent.")
                                      .arg(m_activeSends.size())
                                      .arg(static_cast<qulonglong>(m_totalFramesSent)));
}

void SimulatorWindow::showProblems(const QString& title, const QStringList& problems)
{
    if (problems.size() <= 4)
    {
        QMessageBox::warning(this, title, problems.join("\n\n"));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(QString("%1 problem(s) found. Press Show Details for the full list with solutions.")
                    .arg(problems.size()));
    box.setDetailedText(problems.join("\n\n"));
    box.exec();
}

SimSetup SimulatorWindow::captureSetup() const
{
    SimSetup setup;
    setup.connections = m_connections;
    // Mirror the first connection into the legacy single-destination fields so an
    // older build of the simulator can still open the setup.
    if (!m_connections.isEmpty())
    {
        const ConnectionDefinition& c = m_connections.first();
        setup.destinationType = c.transport;
        if (c.transport == "SERIAL")
        {
            setup.serialPortName = c.serialPortName;
            setup.serialBaud = c.serialBaud;
            setup.serialDataBits = c.serialDataBits;
            setup.serialParity = c.serialParity;
            setup.serialStopBits = c.serialStopBits;
        }
        else if (c.transport == "TCP")
        {
            setup.tcpHost = c.host;
            setup.tcpPort = c.port;
        }
        else
        {
            setup.udpIp = c.host;
            setup.udpPort = c.port;
        }
    }
    setup.messages = m_messages;
    return setup;
}

void SimulatorWindow::applySetup(const SimSetup& setup)
{
    // SimSetupFile guarantees at least one connection (it synthesizes one from the
    // legacy destination when an old setup has no connections array).
    m_connections = setup.connections;
    m_messages = setup.messages;
    refreshConnectionBar();
    refreshMessagesTable();
}

void SimulatorWindow::saveSetupToPath(const QString& path, bool silent)
{
    QString error;
    if (!SimSetupFile::save(captureSetup(), path, error))
    {
        if (!silent)
            QMessageBox::warning(this, "Save Setup", error);
        return;
    }

    if (!silent)
        statusBar()->showMessage(QString("Setup saved to %1").arg(path), 4000);
}

bool SimulatorWindow::loadSetupFromPath(const QString& path, bool silent)
{
    SimSetup setup;
    QString error;
    if (!SimSetupFile::load(path, setup, error))
    {
        if (!silent)
            QMessageBox::warning(this, "Open Setup", error);
        return false;
    }

    applySetup(setup);

    if (!silent)
        statusBar()->showMessage(QString("Setup loaded from %1").arg(path), 4000);
    return true;
}

void SimulatorWindow::onOpenSetupClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Open Simulator Setup",
        QString(),
        "Simulator Setup (*.json);;All Files (*.*)");
    if (path.isEmpty())
        return;

    if (loadSetupFromPath(path, false))
        m_setupPath = path;
}

void SimulatorWindow::onSaveSetupClicked()
{
    if (m_setupPath.isEmpty())
    {
        onSaveSetupAsClicked();
        return;
    }
    saveSetupToPath(m_setupPath, false);
}

void SimulatorWindow::onSaveSetupAsClicked()
{
    const QString path = QFileDialog::getSaveFileName(this,
        "Save Simulator Setup",
        QDir(AppPaths::projectsDir()).filePath("simulator_setup.json"),
        "Simulator Setup (*.json)");
    if (path.isEmpty())
        return;

    m_setupPath = path;
    saveSetupToPath(path, false);
}

void SimulatorWindow::onToggleThemeClicked()
{
    const Themes::Mode next = (Themes::currentMode() == Themes::Dark) ? Themes::Light : Themes::Dark;
    Themes::setMode(next);
    Themes::applyToAllTopLevels();
    ui->btnTheme->setText(next == Themes::Dark ? "Light Theme" : "Dark Theme");
    refreshConnectionBar(); // re-apply the bar dot (its stylesheet outranks the theme)
}

void SimulatorWindow::onShowUserManual()
{
    // Modeless so the manual can stay open while configuring/sending.
    HelpManualDialog* dlg = new HelpManualDialog(":/manual/simulator_manual.html",
        "User Manual — Universal Data Simulator", this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
}

void SimulatorWindow::onShowShortcutsHelp()
{
    QMessageBox box(this);
    box.setWindowTitle("Keyboard Shortcuts");
    box.setTextFormat(Qt::RichText);
    box.setText(
        "<b>Main window</b><br>"
        "Ctrl+O — Open setup &nbsp;|&nbsp; Ctrl+S — Save setup &nbsp;|&nbsp; Ctrl+Shift+S — Save setup as<br>"
        "Ctrl+I — Import ICD (.docx) &nbsp;|&nbsp; F5 — Send &nbsp;|&nbsp; Shift+F5 — Stop<br>"
        "Ctrl+T — Toggle theme &nbsp;|&nbsp; Ctrl+Q — Quit &nbsp;|&nbsp; F1 — This help<br>"
        "<i>You can edit field values, rates and the Send? tick while streaming — the stream updates live.</i><br><br>"
        "<b>Field Configuration dialog</b><br>"
        "Insert — Add field &nbsp;|&nbsp; Ctrl+E — Edit field name &nbsp;|&nbsp; Ctrl+Delete — Remove selected field(s)<br>"
        "Alt+Up / Alt+Down — Move the selected field &nbsp;|&nbsp; drag a row to reorder &nbsp;|&nbsp; Ctrl/Shift-click to multi-select");
    box.exec();
}
