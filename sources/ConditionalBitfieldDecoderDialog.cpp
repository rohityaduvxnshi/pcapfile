#include "ConditionalBitfieldDecoderDialog.h"
#include "ConditionalBitfieldDecoder.h"
#include "ConditionalProfileDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
const int PROF_COL_VALUE = 0;
const int PROF_COL_NAME = 1;
const int PROF_COL_RULES = 2;

const QString kDialogStyle =
    "QWidget{font-family:\"Segoe UI\",\"Noto Sans\",Arial;font-size:12pt;color:#24313f;background-color:#f6f8fb;}"
    "QDialog{background-color:#f6f8fb;}"
    "QGroupBox{background-color:#ffffff;border:1px solid #d8e2ee;border-radius:8px;margin-top:16px;padding:12px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;padding:4px 9px;color:#36536f;"
    "background-color:#edf4fb;border-radius:6px;font-weight:600;}"
    "QLineEdit,QComboBox{background-color:#ffffff;border:1px solid #cbd8e6;border-radius:6px;padding:7px 9px;}"
    "QPushButton{background-color:#dcecf7;border:1px solid #b8d3e7;border-radius:7px;padding:8px 14px;"
    "color:#244660;font-weight:600;}QPushButton:hover{background-color:#cfe4f3;}"
    "QTableWidget{background-color:#ffffff;alternate-background-color:#f3f8fc;gridline-color:#d7e2ed;"
    "border:1px solid #d4dee9;border-radius:8px;selection-background-color:#c8def0;selection-color:#162536;}"
    "QHeaderView::section{background-color:#e6f0f8;color:#263f56;border:0px;border-right:1px solid #cddae7;"
    "border-bottom:1px solid #cddae7;padding:6px;font-weight:600;}";
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
      m_decoder(existing)
{
    setWindowTitle(QString("Conditional Decoder — %1").arg(dependentFieldName));
    setStyleSheet(kDialogStyle);
    setMinimumSize(700, 520);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Info label
    QLabel* infoLabel = new QLabel(
        QString("Dependent field: <b>%1</b> (%2 byte(s)). "
                "Select the controller field and add profiles for each controller value.")
            .arg(dependentFieldName).arg(dependentFieldLengthBytes), this);
    infoLabel->setWordWrap(true);
    infoLabel->setObjectName("lblInfo");
    mainLayout->addWidget(infoLabel);

    // Controller + unknown behavior
    QGroupBox* settingsGroup = new QGroupBox("Controller Settings", this);
    QFormLayout* formLayout = new QFormLayout(settingsGroup);

    m_controllerCombo = new QComboBox(this);
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
    formLayout->addRow("Controller Field:", m_controllerCombo);

    m_unknownCombo = new QComboBox(this);
    m_unknownCombo->addItem("UNKNOWN_CONTROLLER");
    m_unknownCombo->addItem("BLANK");
    if (existing.unknownBehavior.toUpper() == "BLANK")
        m_unknownCombo->setCurrentIndex(1);
    formLayout->addRow("Unknown Controller Behavior:", m_unknownCombo);

    mainLayout->addWidget(settingsGroup);

    // Profile table
    QGroupBox* profileGroup = new QGroupBox("Profiles", this);
    QVBoxLayout* profileLayout = new QVBoxLayout(profileGroup);

    QHBoxLayout* profileBtnLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("Add Profile", this);
    QPushButton* editBtn = new QPushButton("Edit Profile", this);
    QPushButton* removeBtn = new QPushButton("Remove Profile", this);
    profileBtnLayout->addWidget(addBtn);
    profileBtnLayout->addWidget(editBtn);
    profileBtnLayout->addWidget(removeBtn);
    profileBtnLayout->addStretch();
    profileLayout->addLayout(profileBtnLayout);

    m_profileTable = new QTableWidget(0, 3, this);
    m_profileTable->setHorizontalHeaderLabels(QStringList() << "Controller Value" << "Profile Name" << "Rules");
    m_profileTable->horizontalHeader()->setStretchLastSection(true);
    m_profileTable->setAlternatingRowColors(true);
    m_profileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileTable->setMinimumHeight(200);
    profileLayout->addWidget(m_profileTable);

    mainLayout->addWidget(profileGroup);

    // Button box
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    refreshProfileTable();

    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddProfileClicked()));
    connect(editBtn, SIGNAL(clicked()), this, SLOT(onEditProfileClicked()));
    connect(removeBtn, SIGNAL(clicked()), this, SLOT(onRemoveProfileClicked()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
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
