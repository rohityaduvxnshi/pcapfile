#include "SimulatorWindow.h"
#include "ui_SimulatorWindow.h"

#include "ConnectionSettingsDialog.h"
#include "DataSender.h"
#include "HelpManualDialog.h"
#include "IcdDocxImporter.h"
#include "IcdImportDialog.h"
#include "MessageDefinitionDialog.h"
#include "NmeaFieldConfigurationDialog.h"
#include "PayloadBuilder.h"
#include "SerialDataSender.h"
#include "SimFieldConfigurationDialog.h"
#include "TcpDataSender.h"
#include "Themes.h"
#include "UdpDataSender.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
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

const int PREVIEW_MAX_SHOWN_BYTES = 96;
} // namespace

SimulatorWindow::SimulatorWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_connDialog(0)
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

    ui->tblMessages->setColumnCount(7);
    ui->tblMessages->setHorizontalHeaderLabels(QStringList()
        << "Send?" << "Message Name" << "Format" << "Length"
        << "Rate (Hz)" << "Fields" << "Configure Fields");
    ui->tblMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblMessages->setSelectionMode(QAbstractItemView::SingleSelection);

    // Outgoing-data history table.
    ui->tblHistory->setColumnCount(4);
    ui->tblHistory->setHorizontalHeaderLabels(QStringList() << "Time" << "Message" << "Bytes" << "Hex");
    ui->tblHistory->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblHistory->verticalHeader()->setVisible(false);

    // Connection lives in a pop-out that owns the link (survives closing it).
    m_connDialog = new ConnectionSettingsDialog(this);
    connect(ui->btnConfigureConnection, SIGNAL(clicked()), this, SLOT(onConfigureConnectionClicked()));
    connect(m_connDialog, SIGNAL(connectionChanged()), this, SLOT(onConnectionChanged()));
    connect(ui->btnClearHistory, SIGNAL(clicked()), this, SLOT(onClearHistoryClicked()));

    connect(ui->btnAddMessage, SIGNAL(clicked()), this, SLOT(onAddMessageClicked()));
    connect(ui->btnEditMessage, SIGNAL(clicked()), this, SLOT(onEditMessageClicked()));
    connect(ui->btnRemoveMessage, SIGNAL(clicked()), this, SLOT(onRemoveMessageClicked()));
    connect(ui->btnImportIcd, SIGNAL(clicked()), this, SLOT(onImportIcdClicked()));
    connect(ui->actImportIcd, SIGNAL(triggered()), this, SLOT(onImportIcdClicked()));
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

    onConnectionChanged();   // initialise the connection bar
    refreshMessagesTable();

    // Silent auto-restore of the last session's setup (the close event saves it).
    if (QFile::exists(SimSetupFile::autoSavePath()))
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

    QString error;
    SimSetupFile::save(captureSetup(), SimSetupFile::autoSavePath(), error); // silent

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
    m_connDialog->show();
    m_connDialog->raise();
    m_connDialog->activateWindow();
}

void SimulatorWindow::onConnectionChanged()
{
    // Mirror the pop-out's state on the compact bar.
    setBarDot(m_connDialog->dotState());
    ui->lblConnName->setText(m_connDialog->connectionName());

    // A link drop / disconnect while streaming must stop the stream.
    if (m_sending && !m_connDialog->isConnected())
    {
        onStopSendingClicked();
        ui->lblSendStats->setText("Stopped — the connection was lost.");
    }
}

void SimulatorWindow::onClearHistoryClicked()
{
    ui->tblHistory->setRowCount(0);
    m_historyPending.clear();
    ui->lblHistoryCount->setText("0 frames");
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
    dlg.setFields(message.fields);
    if (dlg.exec() == QDialog::Accepted)
    {
        message.fields = dlg.fields();
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
    const bool udpDestination = (qobject_cast<UdpDataSender*>(m_connDialog->activeSender()) != 0);

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

    if (!m_connDialog->isConnected())
        problems.append("Not connected to a destination. Solution: press Configure… on the Connection bar, choose UDP / TCP / Serial and Connect (the dot must be green).");

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

    m_activeSends = plan;
    m_sending = true;
    m_totalFramesSent = 0;
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

bool SimulatorWindow::sendActive(int planIndex)
{
    DataSender* sender = m_connDialog->activeSender();
    if (planIndex < 0 || planIndex >= m_activeSends.size() || !sender)
        return false;

    ActiveSend& send = m_activeSends[planIndex];

    QString error;
    if (!sender->send(send.payload, error))
    {
        stopAllSendTimers();
        m_sending = false;
        setSendingUiState(false);
        setBarDot("red");
        ui->lblSendStats->setText("Stopped (send error).");
        QMessageBox::warning(this, "Send Failed", error);
        return false;
    }

    send.count += 1;
    m_totalFramesSent += 1;
    pushPreviewLine(m_messages.at(send.messageIndex).messageName, send.payload);
    return true;
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

void SimulatorWindow::pushPreviewLine(const QString& messageName, const QByteArray& payload)
{
    const int shownBytes = qMin(payload.size(), PREVIEW_MAX_SHOWN_BYTES);
    QString hex = QString::fromLatin1(payload.left(shownBytes).toHex(' ').toUpper());
    if (payload.size() > shownBytes)
        hex += QString(" … (+%1 more bytes)").arg(payload.size() - shownBytes);

    // Queue a history row; the 200 ms flush appends them so a 1000 Hz stream
    // cannot choke the GUI thread.
    QStringList row;
    row << QTime::currentTime().toString("hh:mm:ss.zzz")
        << messageName
        << QString::number(payload.size())
        << hex;
    m_historyPending.append(row);
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
            const QStringList& row = m_historyPending.at(i);
            const int r = t->rowCount();
            t->insertRow(r);
            for (int c = 0; c < 4 && c < row.size(); ++c)
                t->setItem(r, c, new QTableWidgetItem(row.at(c)));
        }
        m_historyPending.clear();

        // Trim to the configured maximum (drop oldest rows from the top).
        const int maxRows = ui->spinHistoryMax->value();
        while (t->rowCount() > maxRows)
            t->removeRow(0);

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
    m_connDialog->captureDestination(setup);   // destination lives in the pop-out
    setup.messages = m_messages;
    return setup;
}

void SimulatorWindow::applySetup(const SimSetup& setup)
{
    m_connDialog->applyDestination(setup);
    onConnectionChanged();   // refresh the bar in case the name changed

    m_messages = setup.messages;
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
        "simulator_setup.json",
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
    onConnectionChanged(); // re-apply the bar dot (its stylesheet outranks the theme)
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
