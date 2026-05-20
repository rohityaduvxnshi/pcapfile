#include "FieldConfigurationDialog.h"
#include "ui_FieldConfigurationDialog.h"

#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"
#include "ConditionalBitfieldDecoder.h"
#include "ConditionalBitfieldDecoderDialog.h"
#include "InputValidator.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidgetItem>
#include <QVariant>

namespace
{
const int FIELD_COL_NAME = 0;
const int FIELD_COL_BYTE = 1;
const int FIELD_COL_LENGTH = 2;
const int FIELD_COL_RESOLUTION = 3;
const int FIELD_COL_BIT_DECODER = 4;
const int FIELD_COL_COND_DECODER = 5;

QString resolutionTextForField(const FieldDefinition& field)
{
    if (!field.resolutionExpression.trimmed().isEmpty())
        return field.resolutionExpression.trimmed();
    return QString::number(field.resolution, 'g', 15);
}
}

FieldConfigurationDialog::FieldConfigurationDialog(QWidget* parent)
    : QDialog(parent),
      m_payloadLengthBytes(0),
      ui(new Ui::FieldConfigurationDialog)
{
    ui->setupUi(this);

    ui->tblFields->setColumnCount(6);
    ui->tblFields->setHorizontalHeaderLabels(QStringList() << "Field Name" << "Byte Offset" << "Length" << "Resolution" << "Bit Decoder" << "Cond. Decoder");
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
    ui->tblFields->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblFields->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(ui->btnAddField, SIGNAL(clicked()), this, SLOT(onAddFieldClicked()));
    connect(ui->btnEditField, SIGNAL(clicked()), this, SLOT(onEditFieldClicked()));
    connect(ui->btnRemoveField, SIGNAL(clicked()), this, SLOT(onRemoveFieldClicked()));
    connect(ui->btnBitfieldDecoder, SIGNAL(clicked()), this, SLOT(onBitfieldDecoderClicked()));
    connect(ui->btnConditionalDecoder, SIGNAL(clicked()), this, SLOT(onConditionalDecoderClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

FieldConfigurationDialog::~FieldConfigurationDialog()
{
    delete ui;
}

void FieldConfigurationDialog::setPayloadLength(int payloadLengthBytes)
{
    m_payloadLengthBytes = payloadLengthBytes;
    if (m_payloadLengthBytes > 0)
    {
        ui->lblInfo->setText(QString("Configure fields for UDP Payload Length (bytes): %1").arg(m_payloadLengthBytes));
    }
    else
    {
        ui->lblInfo->setText("Configure fields. No fixed payload length is available for this filter mode.");
    }
}

void FieldConfigurationDialog::setFields(const QList<FieldDefinition>& fields)
{
    m_fields = fields;
    refreshFieldTable();
}

QList<FieldDefinition> FieldConfigurationDialog::fields() const
{
    return m_fields;
}

QString FieldConfigurationDialog::tableText(int row, int column) const
{
    QTableWidgetItem* item = ui->tblFields->item(row, column);
    return item ? item->text().trimmed() : QString();
}

int FieldConfigurationDialog::selectedFieldRow() const
{
    QList<QTableWidgetItem*> selectedItems = ui->tblFields->selectedItems();
    if (!selectedItems.isEmpty()) return selectedItems.first()->row();
    return ui->tblFields->currentRow();
}

void FieldConfigurationDialog::setDecoderCell(int row, const QList<BitDecodeRule>& rules)
{
    QTableWidgetItem* decoderItem = ui->tblFields->item(row, FIELD_COL_BIT_DECODER);
    if (!decoderItem)
    {
        decoderItem = new QTableWidgetItem();
        decoderItem->setFlags(decoderItem->flags() & ~Qt::ItemIsEditable);
        ui->tblFields->setItem(row, FIELD_COL_BIT_DECODER, decoderItem);
    }

    if (rules.isEmpty())
    {
        decoderItem->setText("No");
        decoderItem->setToolTip(QString());
    }
    else
    {
        decoderItem->setText(QString("Yes (%1 rules)").arg(rules.size()));
        decoderItem->setToolTip("Bitfield decoder configured");
    }
}

void FieldConfigurationDialog::setConditionalDecoderCell(int row, const ConditionalBitfieldDecoderConfig& decoder)
{
    QTableWidgetItem* condItem = ui->tblFields->item(row, FIELD_COL_COND_DECODER);
    if (!condItem)
    {
        condItem = new QTableWidgetItem();
        condItem->setFlags(condItem->flags() & ~Qt::ItemIsEditable);
        ui->tblFields->setItem(row, FIELD_COL_COND_DECODER, condItem);
    }

    if (decoder.profiles.isEmpty())
    {
        condItem->setText("No");
        condItem->setToolTip(QString());
    }
    else
    {
        condItem->setText(QString("Yes (%1 profiles)").arg(decoder.profiles.size()));
        condItem->setToolTip(QString("Controller: %1").arg(decoder.controllerFieldName));
    }
}

void FieldConfigurationDialog::refreshFieldTable()
{
    ui->tblFields->setRowCount(0);

    for (int i = 0; i < m_fields.size(); ++i)
    {
        const FieldDefinition& field = m_fields.at(i);
        const int row = ui->tblFields->rowCount();
        ui->tblFields->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(field.name);
        if (field.hasBitfieldDecoder && !field.bitDecodeRules.isEmpty())
            nameItem->setData(Qt::UserRole, BitfieldDecoder::rulesToJson(field.bitDecodeRules));
        if (field.hasConditionalBitfieldDecoder && !field.conditionalDecoder.profiles.isEmpty())
            nameItem->setData(Qt::UserRole + 1, ConditionalBitfieldDecoder::toJson(field.conditionalDecoder));

        ui->tblFields->setItem(row, FIELD_COL_NAME, nameItem);
        ui->tblFields->setItem(row, FIELD_COL_BYTE, new QTableWidgetItem(QString::number(field.byteOffset)));
        ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem(QString::number(field.length)));
        ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem(resolutionTextForField(field)));
        setDecoderCell(row, field.bitDecodeRules);
        setConditionalDecoderCell(row, field.conditionalDecoder);
    }

    ui->tblFields->resizeColumnsToContents();
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
}

void FieldConfigurationDialog::onAddFieldClicked()
{
    const int row = ui->tblFields->rowCount();
    ui->tblFields->insertRow(row);
    ui->tblFields->setItem(row, FIELD_COL_NAME, new QTableWidgetItem(QString("Field%1").arg(row + 1)));
    ui->tblFields->setItem(row, FIELD_COL_BYTE, new QTableWidgetItem("0"));
    ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem("2"));
    ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem("1"));
    setDecoderCell(row, QList<BitDecodeRule>());
    setConditionalDecoderCell(row, ConditionalBitfieldDecoderConfig());
    ui->tblFields->selectRow(row);
}

void FieldConfigurationDialog::onEditFieldClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Field Configuration", "Select one field to edit.");
        return;
    }

    QTableWidgetItem* item = ui->tblFields->item(row, FIELD_COL_NAME);
    if (item)
        ui->tblFields->editItem(item);
}

void FieldConfigurationDialog::onRemoveFieldClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Field Configuration", "Select one field to remove.");
        return;
    }

    ui->tblFields->removeRow(row);
}

void FieldConfigurationDialog::onBitfieldDecoderClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Select exactly one field to configure bitfield decoder rules.");
        return;
    }

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
            nameItem->setData(Qt::UserRole, QVariant());
        else
            nameItem->setData(Qt::UserRole, BitfieldDecoder::rulesToJson(rules));

        setDecoderCell(row, rules);
    }
}

QList<FieldDefinition> FieldConfigurationDialog::peekFields() const
{
    QList<FieldDefinition> fields;
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();
        if (name.isEmpty()) continue;

        bool lengthOk = false;
        const int length = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);

        FieldDefinition field;
        field.name = name;
        field.length = (lengthOk && length > 0) ? length : 1;
        field.byteOffset = tableText(row, FIELD_COL_BYTE).toInt();
        fields.append(field);
    }
    return fields;
}

void FieldConfigurationDialog::onConditionalDecoderClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Conditional Decoder", "Select exactly one field to configure its conditional decoder.");
        return;
    }

    const QString fieldName = tableText(row, FIELD_COL_NAME);
    bool lengthOk = false;
    const int fieldLength = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);

    if (fieldName.isEmpty())
    {
        QMessageBox::warning(this, "Conditional Decoder", "Selected field name cannot be empty.");
        return;
    }

    if (!lengthOk || fieldLength <= 0 || fieldLength > 8)
    {
        QMessageBox::warning(this, "Conditional Decoder", "Selected field length must be between 1 and 8 bytes.");
        return;
    }

    const QList<FieldDefinition> allFields = peekFields();
    if (allFields.size() < 2)
    {
        QMessageBox::warning(this, "Conditional Decoder", "At least two fields are required: the dependent field and a controller field.");
        return;
    }

    QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
    if (!nameItem)
    {
        nameItem = new QTableWidgetItem(fieldName);
        ui->tblFields->setItem(row, FIELD_COL_NAME, nameItem);
    }

    ConditionalBitfieldDecoderConfig existingDecoder;
    const QString storedJson = nameItem->data(Qt::UserRole + 1).toString();
    if (!storedJson.trimmed().isEmpty())
    {
        QString parseError;
        if (!ConditionalBitfieldDecoder::fromJson(storedJson, existingDecoder, parseError))
        {
            QMessageBox::warning(this, "Conditional Decoder", "Existing conditional decoder data is invalid and will be cleared:\n" + parseError);
            existingDecoder = ConditionalBitfieldDecoderConfig();
        }
    }

    ConditionalBitfieldDecoderDialog dlg(fieldName, fieldLength, allFields, existingDecoder, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        const ConditionalBitfieldDecoderConfig decoder = dlg.decoder();
        if (decoder.profiles.isEmpty())
            nameItem->setData(Qt::UserRole + 1, QVariant());
        else
            nameItem->setData(Qt::UserRole + 1, ConditionalBitfieldDecoder::toJson(decoder));

        setConditionalDecoderCell(row, decoder);
    }
}

bool FieldConfigurationDialog::collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const
{
    fields.clear();

    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        const QString name = tableText(row, FIELD_COL_NAME);
        const QString byteText = tableText(row, FIELD_COL_BYTE);
        const QString lengthText = tableText(row, FIELD_COL_LENGTH);
        const QString resolutionText = tableText(row, FIELD_COL_RESOLUTION);

        if (name.isEmpty() && byteText.isEmpty() && lengthText.isEmpty() && resolutionText.isEmpty())
            continue;

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
        field.resolutionExpression = resolutionText.trimmed();

        if (m_payloadLengthBytes > 0 && field.byteOffset + field.length > m_payloadLengthBytes)
        {
            errorMessage = QString("Field '%1' exceeds payload length %2 bytes.").arg(field.name).arg(m_payloadLengthBytes);
            return false;
        }

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

            const QString condJson = nameItem->data(Qt::UserRole + 1).toString();
            if (!condJson.trimmed().isEmpty())
            {
                ConditionalBitfieldDecoderConfig condDecoder;
                QString condError;
                if (!ConditionalBitfieldDecoder::fromJson(condJson, condDecoder, condError))
                {
                    errorMessage = QString("Row %1 conditional decoder error: %2").arg(row + 1).arg(condError);
                    return false;
                }
                field.conditionalDecoder = condDecoder;
                field.hasConditionalBitfieldDecoder = !condDecoder.profiles.isEmpty();
            }
        }

        fields.append(field);
    }

    return InputValidator::validateFields(fields, errorMessage);
}

void FieldConfigurationDialog::onSaveClicked()
{
    QList<FieldDefinition> collectedFields;
    QString error;
    if (!collectFields(collectedFields, error))
    {
        QMessageBox::warning(this, "Invalid Field", error);
        return;
    }

    // Re-validate conditional decoders against the final collected field list
    // to catch cases where the controller field was deleted after decoder was configured.
    for (int i = 0; i < collectedFields.size(); ++i)
    {
        const FieldDefinition& field = collectedFields.at(i);
        if (!field.hasConditionalBitfieldDecoder) continue;

        QString condError;
        if (!ConditionalBitfieldDecoder::validate(field.conditionalDecoder, collectedFields,
                                                   field.name, field.length, condError))
        {
            QMessageBox::warning(this, "Invalid Conditional Decoder",
                QString("Field '%1': %2").arg(field.name).arg(condError));
            return;
        }
    }

    m_fields = collectedFields;
    accept();
}
