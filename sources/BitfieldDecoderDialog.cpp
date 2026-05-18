#include "BitfieldDecoderDialog.h"

#include "BitfieldDecoder.h"
#include "BitfieldRuleDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

BitfieldDecoderDialog::BitfieldDecoderDialog(const QString& fieldName,
                                             int fieldLengthBytes,
                                             const QList<BitDecodeRule>& existingRules,
                                             QWidget* parent)
    : QDialog(parent),
      m_fieldName(fieldName),
      m_fieldLengthBytes(fieldLengthBytes),
      m_rules(existingRules),
      m_infoLabel(0),
      m_ruleTable(0)
{
    setWindowTitle("Bitfield Decoder");
    setModal(true);
    resize(850, 520);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_infoLabel = new QLabel(QString("Field: %1 | Length: %2 byte(s) | Available bits: 0-%3")
                                 .arg(m_fieldName)
                                 .arg(m_fieldLengthBytes)
                                 .arg((m_fieldLengthBytes * 8) - 1), this);
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("Add Rule", this);
    QPushButton* btnEdit = new QPushButton("Edit Rule", this);
    QPushButton* btnRemove = new QPushButton("Remove Rule", this);
    buttonLayout->addWidget(btnAdd);
    buttonLayout->addWidget(btnEdit);
    buttonLayout->addWidget(btnRemove);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    m_ruleTable = new QTableWidget(this);
    m_ruleTable->setColumnCount(5);
    m_ruleTable->setHorizontalHeaderLabels(QStringList() << "Label" << "Bits" << "Type" << "Mapping Summary" << "Unknown Behavior");
    m_ruleTable->horizontalHeader()->setStretchLastSection(true);
    m_ruleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ruleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ruleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_ruleTable, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    buttons->addButton("Save", QDialogButtonBox::AcceptRole);
    buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
    mainLayout->addWidget(buttons);

    connect(btnAdd, SIGNAL(clicked()), this, SLOT(onAddRuleClicked()));
    connect(btnEdit, SIGNAL(clicked()), this, SLOT(onEditRuleClicked()));
    connect(btnRemove, SIGNAL(clicked()), this, SLOT(onRemoveRuleClicked()));
    connect(buttons, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    refreshTable();
}

QList<BitDecodeRule> BitfieldDecoderDialog::rules() const
{
    return m_rules;
}

void BitfieldDecoderDialog::refreshTable()
{
    m_ruleTable->setRowCount(0);

    for (int i = 0; i < m_rules.size(); ++i)
    {
        const BitDecodeRule& rule = m_rules.at(i);
        const int row = m_ruleTable->rowCount();
        m_ruleTable->insertRow(row);
        m_ruleTable->setItem(row, 0, new QTableWidgetItem(rule.label));
        m_ruleTable->setItem(row, 1, new QTableWidgetItem(BitfieldDecoder::bitsText(rule.bitPositions)));
        m_ruleTable->setItem(row, 2, new QTableWidgetItem(BitfieldDecoder::ruleTypeText(rule)));
        m_ruleTable->setItem(row, 3, new QTableWidgetItem(BitfieldDecoder::mappingSummary(rule)));
        m_ruleTable->setItem(row, 4, new QTableWidgetItem(rule.unknownBehavior));
    }

    m_ruleTable->resizeColumnsToContents();
    m_ruleTable->horizontalHeader()->setStretchLastSection(true);
}

int BitfieldDecoderDialog::selectedRuleRow() const
{
    QList<QTableWidgetItem*> selectedItems = m_ruleTable->selectedItems();
    if (!selectedItems.isEmpty()) return selectedItems.first()->row();
    return m_ruleTable->currentRow();
}

void BitfieldDecoderDialog::onAddRuleClicked()
{
    BitfieldRuleDialog dlg(m_fieldLengthBytes * 8, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_rules << dlg.rule();
        refreshTable();
    }
}

void BitfieldDecoderDialog::onEditRuleClicked()
{
    const int row = selectedRuleRow();
    if (row < 0 || row >= m_rules.size())
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Select one rule to edit.");
        return;
    }

    BitfieldRuleDialog dlg(m_fieldLengthBytes * 8, m_rules.at(row), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_rules[row] = dlg.rule();
        refreshTable();
        m_ruleTable->selectRow(row);
    }
}

void BitfieldDecoderDialog::onRemoveRuleClicked()
{
    const int row = selectedRuleRow();
    if (row < 0 || row >= m_rules.size())
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Select one rule to remove.");
        return;
    }

    m_rules.removeAt(row);
    refreshTable();
}

void BitfieldDecoderDialog::onSaveClicked()
{
    QString error;
    if (!BitfieldDecoder::validateRules(m_rules, m_fieldLengthBytes, error))
    {
        QMessageBox::warning(this, "Invalid Bitfield Decoder", error);
        return;
    }

    accept();
}
