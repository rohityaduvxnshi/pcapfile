#include "ConditionalBitfieldDecoderDialog.h"

#include "ConditionalBitfieldDecoder.h"
#include "ConditionalProfileDialog.h"
#include "ui_ConditionalBitfieldDecoderDialog.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace
{
const int PROF_COL_VALUE = 0;
const int PROF_COL_NAME = 1;
const int PROF_COL_RULES = 2;
}

ConditionalBitfieldDecoderDialog::ConditionalBitfieldDecoderDialog(const QString& dependentFieldName,
                                                                     int dependentFieldLengthBytes,
                                                                     const QList<FieldDefinition>& allFields,
                                                                     const ConditionalBitfieldDecoderConfig& existing,
                                                                     QWidget* parent)
    : QDialog(parent),
      m_dependentFieldName(dependentFieldName),
      m_dependentFieldLengthBytes(dependentFieldLengthBytes),
      m_allFields(allFields),
      m_decoder(existing),
      ui(new Ui::ConditionalBitfieldDecoderDialog),
      m_controllerCombo(0),
      m_unknownCombo(0),
      m_profileTable(0)
{
    ui->setupUi(this);
    setWindowTitle(QString("Conditional Decoder - %1").arg(dependentFieldName));
    setMinimumSize(700, 520);

    ui->lblInfo->setText(QString("Dependent field: <b>%1</b> (%2 byte(s)). "
                                 "Select the controller field and add profiles for each controller value.")
                             .arg(dependentFieldName)
                             .arg(dependentFieldLengthBytes));

    m_controllerCombo = ui->cmbControllerField;
    m_unknownCombo = ui->cmbUnknownBehavior;
    m_profileTable = ui->tblProfiles;

    for (int i = 0; i < allFields.size(); ++i)
    {
        if (allFields.at(i).name != dependentFieldName)
            m_controllerCombo->addItem(allFields.at(i).name);
    }
    if (!existing.controllerFieldName.isEmpty())
    {
        const int idx = m_controllerCombo->findText(existing.controllerFieldName);
        if (idx >= 0)
            m_controllerCombo->setCurrentIndex(idx);
    }

    if (existing.unknownBehavior.toUpper() == "BLANK")
        m_unknownCombo->setCurrentIndex(1);

    m_profileTable->setColumnCount(3);
    m_profileTable->setHorizontalHeaderLabels(QStringList() << "Controller Value" << "Profile Name" << "Rules");
    m_profileTable->horizontalHeader()->setStretchLastSection(true);
    m_profileTable->setAlternatingRowColors(true);
    m_profileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profileTable->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(ui->btnAddProfile, SIGNAL(clicked()), this, SLOT(onAddProfileClicked()));
    connect(ui->btnEditProfile, SIGNAL(clicked()), this, SLOT(onEditProfileClicked()));
    connect(ui->btnRemoveProfile, SIGNAL(clicked()), this, SLOT(onRemoveProfileClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    refreshProfileTable();
}

ConditionalBitfieldDecoderDialog::~ConditionalBitfieldDecoderDialog()
{
    delete ui;
}

ConditionalBitfieldDecoderConfig ConditionalBitfieldDecoderDialog::decoder() const
{
    return m_decoder;
}

void ConditionalBitfieldDecoderDialog::refreshProfileTable()
{
    m_profileTable->setRowCount(0);
    for (int p = 0; p < m_decoder.profiles.size(); ++p)
    {
        const ConditionalBitDecodeProfile& profile = m_decoder.profiles.at(p);
        const int row = m_profileTable->rowCount();
        m_profileTable->insertRow(row);

        QTableWidgetItem* valItem = new QTableWidgetItem(
            QString("0x%1").arg(profile.controllerValue, 0, 16).toUpper());
        valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem* nameItem = new QTableWidgetItem(profile.profileName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);

        int ruleCount = profile.bitDecodeRules.size();
        int exCount = profile.exclusionRules.size();
        QString ruleText = QString("%1 rule(s)").arg(ruleCount);
        if (exCount > 0)
            ruleText += QString(", %1 exclusion(s)").arg(exCount);

        QTableWidgetItem* ruleItem = new QTableWidgetItem(ruleText);
        ruleItem->setFlags(ruleItem->flags() & ~Qt::ItemIsEditable);

        m_profileTable->setItem(row, PROF_COL_VALUE, valItem);
        m_profileTable->setItem(row, PROF_COL_NAME, nameItem);
        m_profileTable->setItem(row, PROF_COL_RULES, ruleItem);
    }
    m_profileTable->resizeColumnsToContents();
    m_profileTable->horizontalHeader()->setStretchLastSection(true);
}

int ConditionalBitfieldDecoderDialog::selectedProfileRow() const
{
    QList<QTableWidgetItem*> sel = m_profileTable->selectedItems();
    if (!sel.isEmpty()) return sel.first()->row();
    return m_profileTable->currentRow();
}

void ConditionalBitfieldDecoderDialog::onAddProfileClicked()
{
    ConditionalBitDecodeProfile empty;
    ConditionalProfileDialog dlg(m_dependentFieldLengthBytes, empty, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_decoder.profiles.append(dlg.profile());
        refreshProfileTable();
        m_profileTable->selectRow(m_decoder.profiles.size() - 1);
    }
}

void ConditionalBitfieldDecoderDialog::onEditProfileClicked()
{
    const int row = selectedProfileRow();
    if (row < 0 || row >= m_decoder.profiles.size())
    {
        QMessageBox::warning(this, "Edit Profile", "Select a profile to edit.");
        return;
    }

    ConditionalProfileDialog dlg(m_dependentFieldLengthBytes, m_decoder.profiles.at(row), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_decoder.profiles[row] = dlg.profile();
        refreshProfileTable();
        m_profileTable->selectRow(row);
    }
}

void ConditionalBitfieldDecoderDialog::onRemoveProfileClicked()
{
    const int row = selectedProfileRow();
    if (row < 0 || row >= m_decoder.profiles.size())
    {
        QMessageBox::warning(this, "Remove Profile", "Select a profile to remove.");
        return;
    }
    m_decoder.profiles.removeAt(row);
    refreshProfileTable();
}

void ConditionalBitfieldDecoderDialog::onSaveClicked()
{
    m_decoder.controllerFieldName = m_controllerCombo->currentText().trimmed();
    m_decoder.unknownBehavior = m_unknownCombo->currentText().trimmed();

    QString error;
    if (!ConditionalBitfieldDecoder::validate(m_decoder, m_allFields, m_dependentFieldName,
                                               m_dependentFieldLengthBytes, error))
    {
        QMessageBox::warning(this, "Invalid Conditional Decoder", error);
        return;
    }

    accept();
}
