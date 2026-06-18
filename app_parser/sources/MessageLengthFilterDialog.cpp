#include "MessageLengthFilterDialog.h"
#include "ui_MessageLengthFilterDialog.h"

#include "CompareOptionsDialog.h"
#include "FieldConfigurationDialog.h"
#include "MessageDefinitionDialog.h"
#include "NmeaFieldConfigurationDialog.h"
#include "Themes.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidgetItem>

namespace
{
const int MESSAGE_COL_NAME = 0;
const int MESSAGE_COL_LENGTH = 1;
const int MESSAGE_COL_HEADER = 2;
const int MESSAGE_COL_FIELDS = 3;
const int MESSAGE_COL_CONFIGURE = 4;
const int MESSAGE_COL_COMPARE = 5;  // v13
}

MessageLengthFilterDialog::MessageLengthFilterDialog(QWidget* parent)
    : QDialog(parent),
      m_port(0),
      ui(new Ui::MessageLengthFilterDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblMessages->setColumnCount(6);
    ui->tblMessages->setHorizontalHeaderLabels(QStringList()
        << "Message Name"
        << "Payload Length (bytes)"
        << "Optional Header (hex)"
        << "Fields"
        << "Configure Fields"
        << "Compare Options");
    ui->tblMessages->horizontalHeader()->setStretchLastSection(true);
    ui->tblMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblMessages->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->btnAddLengthFilter, SIGNAL(clicked()), this, SLOT(onAddMessageClicked()));
    connect(ui->btnEditFilter, SIGNAL(clicked()), this, SLOT(onEditMessageClicked()));
    connect(ui->btnRemoveFilter, SIGNAL(clicked()), this, SLOT(onRemoveMessageClicked()));
    connect(ui->btnConfigureFields, SIGNAL(clicked()), this, SLOT(onConfigureSelectedFieldsClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

MessageLengthFilterDialog::~MessageLengthFilterDialog()
{
    delete ui;
}

void MessageLengthFilterDialog::setPort(quint16 port)
{
    m_port = port;
    ui->lblHeading->setText(QString("Length Filters for Port %1").arg(m_port));
}

void MessageLengthFilterDialog::setMessages(const QList<MessageDefinition>& messages)
{
    m_messages = messages;
    for (int i = 0; i < m_messages.size(); ++i)
        m_messages[i].port = m_port;
    refreshTable();
}

QList<MessageDefinition> MessageLengthFilterDialog::messages() const
{
    return m_messages;
}

int MessageLengthFilterDialog::selectedMessageRow() const
{
    QList<QTableWidgetItem*> selectedItems = ui->tblMessages->selectedItems();
    if (!selectedItems.isEmpty()) return selectedItems.first()->row();
    return ui->tblMessages->currentRow();
}

bool MessageLengthFilterDialog::hasDuplicateName(const QString& name, int ignoreIndex) const
{
    const QString normalizedName = name.trimmed().toLower();
    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (i == ignoreIndex) continue;
        if (m_messages.at(i).messageName.trimmed().toLower() == normalizedName)
            return true;
    }
    return false;
}

bool MessageLengthFilterDialog::hasDuplicateLength(int payloadLengthBytes, int ignoreIndex) const
{
    // Kept for backward source compatibility; superseded by hasDuplicateSignature when
    // optional headers are in play. Two messages with the same length collide ONLY if
    // neither has a distinguishing optional header. (validateMessage uses the new
    // signature-aware check below.)
    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (i == ignoreIndex) continue;
        if (m_messages.at(i).payloadLengthBytes == payloadLengthBytes)
            return true;
    }
    return false;
}

bool MessageLengthFilterDialog::hasDuplicateSignature(const MessageDefinition& message, int ignoreIndex) const
{
    // NMEA: messages are matched by sentence formatter, not length+header. Two
    // NMEA messages collide only when they decode the same sentence type; a
    // NMEA message never collides with a HEX (byte-length) message.
    if (message.dataFormat == "NMEA")
    {
        for (int i = 0; i < m_messages.size(); ++i)
        {
            if (i == ignoreIndex) continue;
            const MessageDefinition& other = m_messages.at(i);
            if (other.dataFormat != "NMEA") continue;
            if (other.nmeaSentenceType.toUpper() == message.nmeaSentenceType.toUpper())
                return true;
        }
        return false;
    }

    for (int i = 0; i < m_messages.size(); ++i)
    {
        if (i == ignoreIndex) continue;
        const MessageDefinition& other = m_messages.at(i);
        if (other.dataFormat == "NMEA") continue;   // NMEA never collides with HEX
        if (other.payloadLengthBytes != message.payloadLengthBytes) continue;
        // Same length: only a collision if both share the same optional header bytes
        // (including both being empty, the pre-v12 "no header" case).
        if (other.optionalHeader == message.optionalHeader)
            return true;
    }
    return false;
}

bool MessageLengthFilterDialog::validateFieldsFitPayload(const MessageDefinition& message, QString& errorMessage) const
{
    // NMEA: fields are addressed by comma position, not byte offset — the
    // payload-length fit check does not apply.
    if (message.dataFormat == "NMEA")
        return true;

    for (int i = 0; i < message.fields.size(); ++i)
    {
        const FieldDefinition& field = message.fields.at(i);
        if (field.byteOffsetcorrect < 0
            || field.byteOffsetcorrect + field.length > message.payloadLengthBytes)
        {
            errorMessage = QString("Field '%1' exceeds payload length %2 bytes.")
                               .arg(field.name)
                               .arg(message.payloadLengthBytes);
            return false;
        }
    }
    return true;
}

bool MessageLengthFilterDialog::validateMessage(const MessageDefinition& message, int ignoreIndex, QString& errorMessage) const
{
    if (message.messageName.trimmed().isEmpty())
    {
        errorMessage = "Message name cannot be empty.";
        return false;
    }

    if (message.payloadLengthBytes <= 0)
    {
        errorMessage = "Payload length must be greater than 0.";
        return false;
    }

    if (hasDuplicateSignature(message, ignoreIndex))
    {
        const QString headerNote = message.optionalHeader.isEmpty()
            ? QString("Set a distinct optional header on at least one of them to disambiguate.")
            : QString("Both messages share length %1 and the same optional header bytes.").arg(message.payloadLengthBytes);
        errorMessage = QString("Another message with payload length %1 bytes and the same optional header already exists for port %2. %3")
                           .arg(message.payloadLengthBytes)
                           .arg(m_port)
                           .arg(headerNote);
        return false;
    }

    if (hasDuplicateName(message.messageName, ignoreIndex))
    {
        errorMessage = QString("Another message named '%1' already exists for port %2.")
                           .arg(message.messageName)
                           .arg(m_port);
        return false;
    }

    return validateFieldsFitPayload(message, errorMessage);
}

QString MessageLengthFilterDialog::fieldStatusText(const MessageDefinition& message) const
{
    if (message.fields.isEmpty())
        return "No fields";

    int decoderCount = 0;
    for (int i = 0; i < message.fields.size(); ++i)
    {
        if (message.fields.at(i).hasBitfieldDecoder)
            ++decoderCount;
    }

    QString text = (message.fields.size() == 1)
        ? "1 field"
        : QString("%1 fields").arg(message.fields.size());

    if (decoderCount > 0)
        text += QString(", %1 decoder%2").arg(decoderCount).arg(decoderCount == 1 ? "" : "s");

    return text;
}

void MessageLengthFilterDialog::refreshTable()
{
    ui->tblMessages->setRowCount(0);

    for (int i = 0; i < m_messages.size(); ++i)
    {
        const MessageDefinition& message = m_messages.at(i);
        const int row = ui->tblMessages->rowCount();
        ui->tblMessages->insertRow(row);
        ui->tblMessages->setItem(row, MESSAGE_COL_NAME, new QTableWidgetItem(message.messageName));
        ui->tblMessages->setItem(row, MESSAGE_COL_LENGTH, new QTableWidgetItem(QString::number(message.payloadLengthBytes)));
        ui->tblMessages->setItem(row, MESSAGE_COL_HEADER, new QTableWidgetItem(
            message.optionalHeader.isEmpty()
                ? QString("-")
                : QString::fromLatin1(message.optionalHeader.toHex()).toUpper()));
        ui->tblMessages->setItem(row, MESSAGE_COL_FIELDS, new QTableWidgetItem(fieldStatusText(message)));

        QPushButton* button = new QPushButton("Configure Fields", ui->tblMessages);
        button->setProperty("messageRow", row);
        connect(button, SIGNAL(clicked()), this, SLOT(onConfigureFieldButtonClicked()));
        ui->tblMessages->setCellWidget(row, MESSAGE_COL_CONFIGURE, button);

        // v13: per-row Compare Options button
        int activeChecks = 0;
        if (message.hasCompareOptions)
        {
            const CompareOptionsConfig& c = message.compareOptions;
            if (c.checkHeader)       ++activeChecks;
            if (c.checkTerminator)   ++activeChecks;
            if (c.checkChecksum)     ++activeChecks;
            if (c.checkRefreshRate)  ++activeChecks;
            if (c.checkEndianness)   ++activeChecks;
        }
        const QString compareLabel = activeChecks > 0
            ? QString("Edit (%1 check%2)").arg(activeChecks).arg(activeChecks == 1 ? "" : "s")
            : QString("Configure");
        QPushButton* compareBtn = new QPushButton(compareLabel, ui->tblMessages);
        compareBtn->setProperty("messageRow", row);
        connect(compareBtn, SIGNAL(clicked()), this, SLOT(onCompareOptionsButtonClicked()));
        ui->tblMessages->setCellWidget(row, MESSAGE_COL_COMPARE, compareBtn);
    }

    ui->tblMessages->resizeColumnsToContents();
    ui->tblMessages->horizontalHeader()->setStretchLastSection(true);
}

void MessageLengthFilterDialog::onAddMessageClicked()
{
    MessageDefinitionDialog dlg(this);
    dlg.setWindowTitle("Add Length Filter");
    if (dlg.exec() != QDialog::Accepted)
        return;

    MessageDefinition message;
    message.messageName = dlg.messageName();
    message.port = m_port;
    message.payloadLengthBytes = dlg.payloadLengthBytes();
    message.optionalHeader = QByteArray::fromHex(dlg.optionalHeaderHex().toLatin1());
    // NMEA: carry the data-format selection.
    message.dataFormat = dlg.dataFormat();
    message.nmeaSentenceType = dlg.nmeaSentenceType();

    QString error;
    if (!validateMessage(message, -1, error))
    {
        QMessageBox::warning(this, "Invalid Message", error);
        return;
    }

    m_messages.append(message);
    refreshTable();
    ui->tblMessages->selectRow(m_messages.size() - 1);
}

void MessageLengthFilterDialog::onEditMessageClicked()
{
    const int row = selectedMessageRow();
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Length Filters", "Select one message definition to edit.");
        return;
    }

    MessageDefinition edited = m_messages.at(row);
    MessageDefinitionDialog dlg(this);
    dlg.setWindowTitle("Edit Length Filter");
    dlg.setMessageName(edited.messageName);
    dlg.setPayloadLength(edited.payloadLengthBytes);
    dlg.setOptionalHeaderHex(QString::fromLatin1(edited.optionalHeader.toHex()));
    // NMEA: pre-load the data-format selection.
    dlg.setNmeaSentenceType(edited.nmeaSentenceType);
    dlg.setDataFormat(edited.dataFormat);

    if (dlg.exec() != QDialog::Accepted)
        return;

    edited.messageName = dlg.messageName();
    edited.port = m_port;
    edited.payloadLengthBytes = dlg.payloadLengthBytes();
    edited.optionalHeader = QByteArray::fromHex(dlg.optionalHeaderHex().toLatin1());
    // NMEA: carry the data-format selection back.
    edited.dataFormat = dlg.dataFormat();
    edited.nmeaSentenceType = dlg.nmeaSentenceType();

    QString error;
    if (!validateMessage(edited, row, error))
    {
        QMessageBox::warning(this, "Invalid Message", error);
        return;
    }

    m_messages[row] = edited;
    refreshTable();
    ui->tblMessages->selectRow(row);
}

void MessageLengthFilterDialog::onRemoveMessageClicked()
{
    const int row = selectedMessageRow();
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Length Filters", "Select one message definition to remove.");
        return;
    }

    const int answer = QMessageBox::question(this,
                                             "Remove Message Definition",
                                             "Removing this message definition will also remove all configured fields for it. Continue?",
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_messages.removeAt(row);
    refreshTable();
}

void MessageLengthFilterDialog::configureMessageAt(int row)
{
    if (row < 0 || row >= m_messages.size())
    {
        QMessageBox::warning(this, "Length Filters", "Select one message definition to configure fields.");
        return;
    }

    // NMEA: the registry-driven configurator replaces the Hex field editor.
    if (m_messages.at(row).dataFormat == "NMEA")
    {
        NmeaFieldConfigurationDialog dlg(this);
        dlg.setWindowTitle(QString("NMEA Fields for %1").arg(m_messages.at(row).messageName));
        dlg.setSentenceType(m_messages.at(row).nmeaSentenceType);
        dlg.setExistingConfig(m_messages.at(row).fields);

        if (dlg.exec() == QDialog::Accepted)
        {
            m_messages[row].fields = dlg.fieldConfig();
            refreshTable();
            ui->tblMessages->selectRow(row);
        }
        return;
    }

    FieldConfigurationDialog dlg(this);
    dlg.setWindowTitle(QString("Fields for %1").arg(m_messages.at(row).messageName));
    dlg.setPayloadLength(m_messages.at(row).payloadLengthBytes);
    dlg.setFields(m_messages.at(row).fields);

    if (dlg.exec() == QDialog::Accepted)
    {
        m_messages[row].fields = dlg.fields();
        refreshTable();
        ui->tblMessages->selectRow(row);
    }
}

void MessageLengthFilterDialog::onConfigureSelectedFieldsClicked()
{
    configureMessageAt(selectedMessageRow());
}

void MessageLengthFilterDialog::onConfigureFieldButtonClicked()
{
    QObject* button = sender();
    const int row = button ? button->property("messageRow").toInt() : -1;
    configureMessageAt(row);
}

void MessageLengthFilterDialog::onSaveClicked()
{
    for (int i = 0; i < m_messages.size(); ++i)
    {
        m_messages[i].port = m_port;
        QString error;
        if (!validateMessage(m_messages.at(i), i, error))
        {
            QMessageBox::warning(this, "Invalid Message", error);
            return;
        }
    }

    accept();
}

// v13: open the CompareOptionsDialog for the row indicated by the sending button.
void MessageLengthFilterDialog::onCompareOptionsButtonClicked()
{
    QObject* button = sender();
    const int row = button ? button->property("messageRow").toInt() : -1;
    if (row < 0 || row >= m_messages.size())
        return;

    const MessageDefinition& msg = m_messages.at(row);
    CompareOptionsDialog dlg(this);
    dlg.setWindowTitle(QString("Compare Options for %1").arg(msg.messageName));
    dlg.setPayloadLength(msg.payloadLengthBytes);
    dlg.setConfig(msg.compareOptions);

    if (dlg.exec() != QDialog::Accepted)
        return;

    m_messages[row].compareOptions = dlg.config();
    m_messages[row].hasCompareOptions = dlg.hasCompareOptions();
    refreshTable();
    ui->tblMessages->selectRow(row);
}
