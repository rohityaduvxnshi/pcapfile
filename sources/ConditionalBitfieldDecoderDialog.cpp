#include "ConditionalBitfieldDecoderDialog.h"
#include "ui_ConditionalBitfieldDecoderDialog.h"

#include "ConditionalBitfieldDecoder.h"
#include "ConditionalProfileDialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

ConditionalBitfieldDecoderDialog::ConditionalBitfieldDecoderDialog(const QString& dependentFieldName,
                                                                   int dependentFieldLengthBytes,
                                                                   const QList<FieldDefinition>& allFields,
                                                                   const ConditionalBitfieldDecoderConfig& existing,
                                                                   QWidget* parent)
    : QDialog(parent),
      m_dependentFieldName(dependentFieldName),
      m_dependentFieldLengthBytes(dependentFieldLengthBytes),
      m_allFields(allFields),
      m_profiles(existing.profiles),
      ui(new Ui::ConditionalBitfieldDecoderDialog)
{
    ui->setupUi(this);

    ui->lblInfo->setText(QString("Dependent field: %1 | Length: %2 byte(s) | Available bits: 0-%3")
                             .arg(m_dependentFieldName)
                             .arg(m_dependentFieldLengthBytes)
                             .arg((m_dependentFieldLengthBytes * 8) - 1));

    // Populate controller field combo — exclude the dependent field itself
    for (int i = 0; i < m_allFields.size(); ++i)
    {
        const QString name = m_allFields.at(i).name.trimmed();
        if (name != m_dependentFieldName.trimmed())
            ui->cmbControllerField->addItem(name);
    }

    // Restore previously selected controller field
    if (!existing.controllerFieldName.trimmed().isEmpty())
    {
        const int idx = ui->cmbControllerField->findText(existing.controllerFieldName.trimmed());
        if (idx >= 0)
            ui->cmbControllerField->setCurrentIndex(idx);
    }

    // Restore unknown behavior
    const QString behavior = existing.unknownBehavior.trimmed().toUpper();
    const int behaviorIdx = ui->cmbUnknownBehavior->findText(behavior == "BLANK" ? "BLANK" : "UNKNOWN_CONTROLLER");
    if (behaviorIdx >= 0)
        ui->cmbUnknownBehavior->setCurrentIndex(behaviorIdx);

    ui->tblProfiles->horizontalHeader()->setStretchLastSection(true);

    connect(ui->btnAddProfile, SIGNAL(clicked()), this, SLOT(onAddProfileClicked()));
    connect(ui->btnEditProfile, SIGNAL(clicked()), this, SLOT(onEditProfileClicked()));
    connect(ui->btnRemoveProfile, SIGNAL(clicked()), this, SLOT(onRemoveProfileClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    refreshTable();
}

ConditionalBitfieldDecoderDialog::~ConditionalBitfieldDecoderDialog()
{
    delete ui;
}

ConditionalBitfieldDecoderConfig ConditionalBitfieldDecoderDialog::decoder() const
{
    return collectDecoder();
}

ConditionalBitfieldDecoderConfig ConditionalBitfieldDecoderDialog::collectDecoder() const
{
    ConditionalBitfieldDecoderConfig config;
    config.controllerFieldName = ui->cmbControllerField->currentText().trimmed();
    config.unknownBehavior = ui->cmbUnknownBehavior->currentText().trimmed();
    config.profiles = m_profiles;
    return config;
}

void ConditionalBitfieldDecoderDialog::refreshTable()
{
    ui->tblProfiles->setRowCount(0);

    for (int i = 0; i < m_profiles.size(); ++i)
    {
        const ConditionalBitDecodeProfile& profile = m_profiles.at(i);
        const int row = ui->tblProfiles->rowCount();
        ui->tblProfiles->insertRow(row);

        ui->tblProfiles->setItem(row, 0,
            new QTableWidgetItem(QString("0x%1").arg(QString::number(profile.controllerValue, 16).toUpper())));
        ui->tblProfiles->setItem(row, 1, new QTableWidgetItem(profile.profileName));

        const int ruleCount = profile.bitDecodeRules.size();
        ui->tblProfiles->setItem(row, 2,
            new QTableWidgetItem(ruleCount == 0 ? "No rules" : QString("%1 rule(s)").arg(ruleCount)));
    }

    ui->tblProfiles->resizeColumnsToContents();
    ui->tblProfiles->horizontalHeader()->setStretchLastSection(true);
}

int ConditionalBitfieldDecoderDialog::selectedProfileRow() const
{
    QList<QTableWidgetItem*> selected = ui->tblProfiles->selectedItems();
    if (!selected.isEmpty()) return selected.first()->row();
    return ui->tblProfiles->currentRow();
}

void ConditionalBitfieldDecoderDialog::onAddProfileClicked()
{
    ConditionalBitDecodeProfile empty;
    ConditionalProfileDialog dlg(m_dependentFieldLengthBytes, empty, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_profiles << dlg.profile();
        refreshTable();
        ui->tblProfiles->selectRow(m_profiles.size() - 1);
    }
}

void ConditionalBitfieldDecoderDialog::onEditProfileClicked()
{
    const int row = selectedProfileRow();
    if (row < 0 || row >= m_profiles.size())
    {
        QMessageBox::warning(this, "Conditional Bitfield Decoder", "Select one profile to edit.");
        return;
    }

    ConditionalProfileDialog dlg(m_dependentFieldLengthBytes, m_profiles.at(row), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_profiles[row] = dlg.profile();
        refreshTable();
        ui->tblProfiles->selectRow(row);
    }
}

void ConditionalBitfieldDecoderDialog::onRemoveProfileClicked()
{
    const int row = selectedProfileRow();
    if (row < 0 || row >= m_profiles.size())
    {
        QMessageBox::warning(this, "Conditional Bitfield Decoder", "Select one profile to remove.");
        return;
    }

    m_profiles.removeAt(row);
    refreshTable();
}

void ConditionalBitfieldDecoderDialog::onSaveClicked()
{
    const ConditionalBitfieldDecoderConfig config = collectDecoder();
    QString error;
    if (!ConditionalBitfieldDecoder::validate(config, m_allFields, m_dependentFieldName, m_dependentFieldLengthBytes, error))
    {
        QMessageBox::warning(this, "Invalid Conditional Bitfield Decoder", error);
        return;
    }

    accept();
}
