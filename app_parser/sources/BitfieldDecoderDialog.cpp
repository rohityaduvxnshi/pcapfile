#include "BitfieldDecoderDialog.h"

#include "BitfieldDecoder.h"
#include "BitfieldRuleDialog.h"
#include "Themes.h"
#include "ui_BitfieldDecoderDialog.h"

#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QIODevice>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

BitfieldDecoderDialog::BitfieldDecoderDialog(const QString& fieldName,
                                             int fieldLengthBytes,
                                             const QList<BitDecodeRule>& existingRules,
                                             QWidget* parent)
    : QDialog(parent),
      m_fieldName(fieldName),
      m_fieldLengthBytes(fieldLengthBytes),
      m_rules(existingRules),
      ui(new Ui::BitfieldDecoderDialog),
      m_ruleTable(0)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->lblInfo->setText(QString("Field: %1 | Length: %2 byte(s) | Available bits: 0-%3")
                             .arg(m_fieldName)
                             .arg(m_fieldLengthBytes)
                             .arg((m_fieldLengthBytes * 8) - 1));

    m_ruleTable = ui->tblRules;
    m_ruleTable->setColumnCount(5);
    m_ruleTable->setHorizontalHeaderLabels(QStringList() << "Label" << "Bits" << "Type" << "Mapping Summary" << "Unknown Behavior");
    m_ruleTable->horizontalHeader()->setStretchLastSection(true);

    connect(ui->btnAddRule, SIGNAL(clicked()), this, SLOT(onAddRuleClicked()));
    connect(ui->btnEditRule, SIGNAL(clicked()), this, SLOT(onEditRuleClicked()));
    connect(ui->btnRemoveRule, SIGNAL(clicked()), this, SLOT(onRemoveRuleClicked()));
    connect(ui->btnImportJson, SIGNAL(clicked()), this, SLOT(onImportJsonClicked()));
    connect(ui->btnExport, SIGNAL(clicked()), this, SLOT(onExportClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    refreshTable();
}

BitfieldDecoderDialog::~BitfieldDecoderDialog()
{
    delete ui;
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

void BitfieldDecoderDialog::onImportJsonClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import Bit Decoder Rules from JSON",
        QString(),
        "JSON Files (*.json);;All Files (*.*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Cannot open file:\n%1").arg(file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QList<BitDecodeRule> imported;
    QString error;
    if (!BitfieldDecoder::rulesFromJson(QString::fromUtf8(bytes), m_fieldLengthBytes, imported, error))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Import failed:\n\n%1").arg(error));
        return;
    }
    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import JSON", "JSON contained no rules.");
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Import JSON");
    box.setText(QString("Imported %1 rule(s). Replace the current rule list, or append?")
                    .arg(imported.size()));
    QPushButton* replaceBtn = box.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton* appendBtn  = box.addButton("Append",  QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(replaceBtn);
    box.exec();

    QList<BitDecodeRule> proposed;
    if (box.clickedButton() == replaceBtn)
        proposed = imported;
    else if (box.clickedButton() == appendBtn)
    {
        proposed = m_rules;
        proposed.append(imported);
    }
    else
        return;

    QString validateErr;
    if (!BitfieldDecoder::validateRules(proposed, m_fieldLengthBytes, validateErr))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Imported rules conflict with existing rules:\n\n%1").arg(validateErr));
        return;
    }

    m_rules = proposed;
    refreshTable();
    QMessageBox::information(this, "Import JSON",
        QString("Imported %1 rule(s).").arg(imported.size()));
}

void BitfieldDecoderDialog::onExportClicked()
{
    if (m_rules.isEmpty())
    {
        QMessageBox::information(this, "Export", "No rules to export.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        "Export Bit Decoder Rules",
        QString("bit_rules.json"),
        "JSON (*.json)");
    if (path.isEmpty()) return;

    const QString jsonText = BitfieldDecoder::rulesToJson(m_rules);
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, "Export",
            QString("Cannot open file for writing:\n%1").arg(out.errorString()));
        return;
    }
    const QByteArray bytes = jsonText.toUtf8();
    const qint64 written = out.write(bytes);
    out.close();
    if (written != bytes.size())
    {
        QMessageBox::warning(this, "Export", "Write incomplete.");
        return;
    }

    QMessageBox::information(this, "Export",
        QString("Exported %1 rule(s) to:\n%2").arg(m_rules.size()).arg(path));
}
