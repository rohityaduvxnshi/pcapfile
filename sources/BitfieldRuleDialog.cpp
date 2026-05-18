#include "BitfieldRuleDialog.h"

#include "BitfieldDecoder.h"
#include "ui_BitfieldRuleDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>

BitfieldRuleDialog::BitfieldRuleDialog(int maxBitCount, QWidget* parent)
    : QDialog(parent),
      m_maxBitCount(maxBitCount),
      ui(new Ui::BitfieldRuleDialog),
      m_labelEdit(0),
      m_bitsEdit(0),
      m_typeCombo(0),
      m_unknownCombo(0),
      m_mappingTable(0)
{
    buildUi();
}

BitfieldRuleDialog::BitfieldRuleDialog(int maxBitCount, const BitDecodeRule& rule, QWidget* parent)
    : QDialog(parent),
      m_maxBitCount(maxBitCount),
      ui(new Ui::BitfieldRuleDialog),
      m_labelEdit(0),
      m_bitsEdit(0),
      m_typeCombo(0),
      m_unknownCombo(0),
      m_mappingTable(0)
{
    buildUi();
    loadRule(rule);
}

BitfieldRuleDialog::~BitfieldRuleDialog()
{
    delete ui;
}

BitDecodeRule BitfieldRuleDialog::rule() const
{
    return m_rule;
}

void BitfieldRuleDialog::buildUi()
{
    ui->setupUi(this);

    m_labelEdit = ui->txtLabel;
    m_bitsEdit = ui->txtBits;
    m_typeCombo = ui->cboType;
    m_unknownCombo = ui->cboUnknown;
    m_mappingTable = ui->tblMappings;

    m_bitsEdit->setPlaceholderText(QString("Examples: 1, 0-2, 3-4, 5,6. Valid: 0-%1").arg(m_maxBitCount - 1));

    m_typeCombo->clear();
    m_typeCombo->addItem("Single Bit", "SINGLE_BIT");
    m_typeCombo->addItem("Grouped Bits", "GROUPED_BITS");
    m_typeCombo->addItem("Reserved / Spare", "RESERVED");

    m_unknownCombo->clear();
    m_unknownCombo->addItem("UNKNOWN(binary)", "UNKNOWN");
    m_unknownCombo->addItem("Blank", "BLANK");
    m_unknownCombo->addItem("Raw Binary", "RAW_BINARY");

    m_mappingTable->setColumnCount(2);
    m_mappingTable->setHorizontalHeaderLabels(QStringList() << "Binary Pattern" << "Meaning");
    m_mappingTable->horizontalHeader()->setStretchLastSection(true);

    connect(ui->btnGenerateMappings, SIGNAL(clicked()), this, SLOT(onGenerateMappingsClicked()));
    connect(ui->btnAddMapping, SIGNAL(clicked()), this, SLOT(onAddMappingClicked()));
    connect(ui->btnRemoveMapping, SIGNAL(clicked()), this, SLOT(onRemoveMappingClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    generateMappingRows();
}

void BitfieldRuleDialog::loadRule(const BitDecodeRule& rule)
{
    m_labelEdit->setText(rule.label);
    m_bitsEdit->setText(BitfieldDecoder::bitsText(rule.bitPositions));

    const QString type = BitfieldDecoder::ruleTypeText(rule);
    int typeIndex = m_typeCombo->findData(type);
    if (typeIndex >= 0) m_typeCombo->setCurrentIndex(typeIndex);

    int unknownIndex = m_unknownCombo->findData(rule.unknownBehavior);
    if (unknownIndex >= 0) m_unknownCombo->setCurrentIndex(unknownIndex);

    m_mappingTable->setRowCount(0);
    QMap<quint64, QString>::const_iterator it;
    for (it = rule.valueMeanings.constBegin(); it != rule.valueMeanings.constEnd(); ++it)
    {
        const int row = m_mappingTable->rowCount();
        m_mappingTable->insertRow(row);
        m_mappingTable->setItem(row, 0, new QTableWidgetItem(BitfieldDecoder::binaryString(it.key(), rule.bitPositions.size())));
        m_mappingTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
}

QString BitfieldRuleDialog::currentTypeCode() const
{
    return m_typeCombo->itemData(m_typeCombo->currentIndex()).toString();
}

void BitfieldRuleDialog::onGenerateMappingsClicked()
{
    generateMappingRows();
}

void BitfieldRuleDialog::generateMappingRows()
{
    QList<int> bits;
    QString error;
    if (!BitfieldDecoder::parseBitPositions(m_bitsEdit->text(), m_maxBitCount, bits, error))
    {
        if (!m_bitsEdit->text().trimmed().isEmpty())
            QMessageBox::warning(this, "Invalid Bits", error);
        return;
    }

    const int width = bits.size();
    if (width <= 0) return;

    if (width > 8)
    {
        QMessageBox::information(this, "Manual Mapping", "Groups larger than 8 bits are not auto-generated. Add mapping rows manually.");
        return;
    }

    QMap<QString, QString> oldMeanings;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row)
    {
        QTableWidgetItem* binaryItem = m_mappingTable->item(row, 0);
        QTableWidgetItem* meaningItem = m_mappingTable->item(row, 1);
        if (binaryItem)
            oldMeanings.insert(binaryItem->text().trimmed(), meaningItem ? meaningItem->text() : QString());
    }

    m_mappingTable->setRowCount(0);
    const quint64 count = static_cast<quint64>(1) << width;
    for (quint64 value = 0; value < count; ++value)
    {
        const QString binary = BitfieldDecoder::binaryString(value, width);
        const int row = m_mappingTable->rowCount();
        m_mappingTable->insertRow(row);
        m_mappingTable->setItem(row, 0, new QTableWidgetItem(binary));

        QString meaning = oldMeanings.value(binary);
        if (currentTypeCode() == "RESERVED" && meaning.isEmpty()) meaning = "RESERVED";
        m_mappingTable->setItem(row, 1, new QTableWidgetItem(meaning));
    }
}

void BitfieldRuleDialog::onAddMappingClicked()
{
    const int row = m_mappingTable->rowCount();
    m_mappingTable->insertRow(row);
    m_mappingTable->setItem(row, 0, new QTableWidgetItem(QString()));
    m_mappingTable->setItem(row, 1, new QTableWidgetItem(QString()));
}

void BitfieldRuleDialog::onRemoveMappingClicked()
{
    QList<int> rows;
    QList<QTableWidgetItem*> selectedItems = m_mappingTable->selectedItems();
    for (int i = 0; i < selectedItems.size(); ++i)
    {
        const int row = selectedItems.at(i)->row();
        if (!rows.contains(row)) rows.append(row);
    }

    while (!rows.isEmpty())
    {
        int maxIndex = 0;
        for (int i = 1; i < rows.size(); ++i)
            if (rows.at(i) > rows.at(maxIndex)) maxIndex = i;
        m_mappingTable->removeRow(rows.at(maxIndex));
        rows.removeAt(maxIndex);
    }
}

bool BitfieldRuleDialog::collectRule(BitDecodeRule& outRule, QString& errorMessage) const
{
    errorMessage.clear();
    outRule = BitDecodeRule();
    outRule.label = m_labelEdit->text().trimmed();
    outRule.unknownBehavior = m_unknownCombo->itemData(m_unknownCombo->currentIndex()).toString();
    outRule.reserved = (currentTypeCode() == "RESERVED");
    outRule.enabled = true;

    if (outRule.label.isEmpty())
    {
        errorMessage = "Rule label cannot be empty.";
        return false;
    }

    if (!BitfieldDecoder::parseBitPositions(m_bitsEdit->text(), m_maxBitCount, outRule.bitPositions, errorMessage))
        return false;

    if (currentTypeCode() == "SINGLE_BIT" && outRule.bitPositions.size() != 1)
    {
        errorMessage = "Single Bit rule must contain exactly one bit position.";
        return false;
    }

    for (int row = 0; row < m_mappingTable->rowCount(); ++row)
    {
        QTableWidgetItem* binaryItem = m_mappingTable->item(row, 0);
        QTableWidgetItem* meaningItem = m_mappingTable->item(row, 1);
        const QString binary = binaryItem ? binaryItem->text().trimmed() : QString();
        const QString meaning = meaningItem ? meaningItem->text().trimmed() : QString();

        if (binary.isEmpty() && meaning.isEmpty()) continue;

        if (binary.size() != outRule.bitPositions.size())
        {
            errorMessage = QString("Mapping row %1 binary length must be %2.").arg(row + 1).arg(outRule.bitPositions.size());
            return false;
        }

        quint64 value = 0;
        if (!BitfieldDecoder::binaryToValue(binary, value))
        {
            errorMessage = QString("Mapping row %1 has invalid binary pattern.").arg(row + 1);
            return false;
        }

        if (outRule.valueMeanings.contains(value))
        {
            errorMessage = QString("Duplicate mapping pattern: %1").arg(binary);
            return false;
        }

        outRule.valueMeanings.insert(value, meaning);
    }

    if (outRule.valueMeanings.isEmpty())
    {
        errorMessage = "Add at least one mapping row.";
        return false;
    }

    return true;
}

void BitfieldRuleDialog::onSaveClicked()
{
    BitDecodeRule collected;
    QString error;
    if (!collectRule(collected, error))
    {
        QMessageBox::warning(this, "Invalid Decode Rule", error);
        return;
    }

    m_rule = collected;
    accept();
}
