#include "ConditionalProfileDialog.h"
#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
const int EX_COL_BITS = 0;
const int EX_COL_LABEL = 1;
const int EX_COL_MESSAGE = 2;

const QString kDialogStyle =
    "QWidget{font-family:\"Segoe UI\",\"Noto Sans\",Arial;font-size:12pt;color:#24313f;background-color:#f6f8fb;}"
    "QDialog{background-color:#f6f8fb;}"
    "QGroupBox{background-color:#ffffff;border:1px solid #d8e2ee;border-radius:8px;margin-top:16px;padding:12px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;padding:4px 9px;color:#36536f;"
    "background-color:#edf4fb;border-radius:6px;font-weight:600;}"
    "QLineEdit{background-color:#ffffff;border:1px solid #cbd8e6;border-radius:6px;padding:7px 9px;}"
    "QPushButton{background-color:#dcecf7;border:1px solid #b8d3e7;border-radius:7px;padding:8px 14px;"
    "color:#244660;font-weight:600;}QPushButton:hover{background-color:#cfe4f3;}"
    "QTableWidget{background-color:#ffffff;alternate-background-color:#f3f8fc;gridline-color:#d7e2ed;"
    "border:1px solid #d4dee9;border-radius:8px;selection-background-color:#c8def0;selection-color:#162536;}"
    "QHeaderView::section{background-color:#e6f0f8;color:#263f56;border:0px;border-right:1px solid #cddae7;"
    "border-bottom:1px solid #cddae7;padding:6px;font-weight:600;}";
}

ConditionalProfileDialog::ConditionalProfileDialog(int dependentFieldLengthBytes,
                                                     const ConditionalBitDecodeProfile& existing,
                                                     QWidget* parent)
    : QDialog(parent),
      m_dependentFieldLengthBytes(dependentFieldLengthBytes),
      m_profile(existing)
{
    setWindowTitle("Configure Profile");
    setStyleSheet(kDialogStyle);
    setMinimumWidth(620);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // --- Profile identity fields ---
    QGroupBox* identityGroup = new QGroupBox("Profile Identity", this);
    QFormLayout* formLayout = new QFormLayout(identityGroup);

    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setPlaceholderText("e.g. 1  or  0x01");
    if (existing.controllerValue != 0 || !existing.profileName.isEmpty())
        m_valueEdit->setText(QString("0x%1").arg(existing.controllerValue, 0, 16).toUpper());
    formLayout->addRow("Controller Value:", m_valueEdit);

    m_nameEdit = new QLineEdit(existing.profileName, this);
    m_nameEdit->setPlaceholderText("e.g. Live Mode");
    formLayout->addRow("Profile Name:", m_nameEdit);

    mainLayout->addWidget(identityGroup);

    // --- Bit rules ---
    QGroupBox* rulesGroup = new QGroupBox("Bit Decode Rules", this);
    QVBoxLayout* rulesLayout = new QVBoxLayout(rulesGroup);

    m_rulesLabel = new QLabel(this);
    rulesLayout->addWidget(m_rulesLabel);

    m_configureRulesBtn = new QPushButton("Configure Bit Rules", this);
    rulesLayout->addWidget(m_configureRulesBtn);

    mainLayout->addWidget(rulesGroup);

    // --- Exclusion rules ---
    QGroupBox* exclusionGroup = new QGroupBox("Mutual Exclusion Rules (Optional)", this);
    QVBoxLayout* exLayout = new QVBoxLayout(exclusionGroup);

    QLabel* exInfo = new QLabel("Define bit groups where at most one bit should be set. If more than one is set, the invalid message is written.", this);
    exInfo->setWordWrap(true);
    exLayout->addWidget(exInfo);

    QHBoxLayout* exBtnLayout = new QHBoxLayout();
    QPushButton* addExBtn = new QPushButton("Add Rule", this);
    QPushButton* removeExBtn = new QPushButton("Remove Selected", this);
    exBtnLayout->addWidget(addExBtn);
    exBtnLayout->addWidget(removeExBtn);
    exBtnLayout->addStretch();
    exLayout->addLayout(exBtnLayout);

    m_exclusionTable = new QTableWidget(0, 3, this);
    m_exclusionTable->setHorizontalHeaderLabels(QStringList() << "Bits (e.g. 0,1)" << "Validation Label" << "Invalid Message");
    m_exclusionTable->horizontalHeader()->setStretchLastSection(true);
    m_exclusionTable->setAlternatingRowColors(true);
    m_exclusionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_exclusionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_exclusionTable->setMinimumHeight(120);
    exLayout->addWidget(m_exclusionTable);

    mainLayout->addWidget(exclusionGroup);

    // --- Button box ---
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // Populate exclusion rules table from existing
    for (int e = 0; e < existing.exclusionRules.size(); ++e)
    {
        const ConditionalBitExclusionRule& exRule = existing.exclusionRules.at(e);
        const int row = m_exclusionTable->rowCount();
        m_exclusionTable->insertRow(row);

        QStringList bits;
        for (int b = 0; b < exRule.mutuallyExclusiveBits.size(); ++b)
            bits << QString::number(exRule.mutuallyExclusiveBits.at(b));
        m_exclusionTable->setItem(row, EX_COL_BITS, new QTableWidgetItem(bits.join(",")));
        m_exclusionTable->setItem(row, EX_COL_LABEL, new QTableWidgetItem(exRule.validationLabel));
        m_exclusionTable->setItem(row, EX_COL_MESSAGE, new QTableWidgetItem(exRule.invalidMessage));
    }

    refreshRulesLabel();

    connect(m_configureRulesBtn, SIGNAL(clicked()), this, SLOT(onConfigureRulesClicked()));
    connect(addExBtn, SIGNAL(clicked()), this, SLOT(onAddExclusionClicked()));
    connect(removeExBtn, SIGNAL(clicked()), this, SLOT(onRemoveExclusionClicked()));
    connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

ConditionalBitDecodeProfile ConditionalProfileDialog::profile() const
{
    return m_profile;
}

void ConditionalProfileDialog::refreshRulesLabel()
{
    if (m_profile.bitDecodeRules.isEmpty())
        m_rulesLabel->setText("No bit rules configured.");
    else
        m_rulesLabel->setText(QString("%1 bit rule(s) configured.").arg(m_profile.bitDecodeRules.size()));
}

void ConditionalProfileDialog::onConfigureRulesClicked()
{
    const QString profileName = m_nameEdit->text().trimmed().isEmpty()
        ? "Profile"
        : m_nameEdit->text().trimmed();

    BitfieldDecoderDialog dlg(profileName, m_dependentFieldLengthBytes, m_profile.bitDecodeRules, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_profile.bitDecodeRules = dlg.rules();
        refreshRulesLabel();
    }
}

void ConditionalProfileDialog::onAddExclusionClicked()
{
    const int row = m_exclusionTable->rowCount();
    m_exclusionTable->insertRow(row);
    m_exclusionTable->setItem(row, EX_COL_BITS, new QTableWidgetItem("0,1"));
    m_exclusionTable->setItem(row, EX_COL_LABEL, new QTableWidgetItem("Validation"));
    m_exclusionTable->setItem(row, EX_COL_MESSAGE, new QTableWidgetItem("Invalid: mutually exclusive bits both set"));
    m_exclusionTable->selectRow(row);
}

void ConditionalProfileDialog::onRemoveExclusionClicked()
{
    const int row = m_exclusionTable->currentRow();
    if (row >= 0 && row < m_exclusionTable->rowCount())
        m_exclusionTable->removeRow(row);
}

bool ConditionalProfileDialog::collectProfile(ConditionalBitDecodeProfile& out, QString& errorMessage) const
{
    const QString valueText = m_valueEdit->text().trimmed();
    bool ok = false;
    const quint64 controllerValue = valueText.toULongLong(&ok, 0);
    if (!ok)
    {
        errorMessage = QString("Controller value '%1' is not a valid number. Use decimal (e.g. 1) or hex (e.g. 0x01).").arg(valueText);
        return false;
    }

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
    {
        errorMessage = "Profile name cannot be empty.";
        return false;
    }

    out.controllerValue = controllerValue;
    out.profileName = name;
    out.bitDecodeRules = m_profile.bitDecodeRules;
    out.exclusionRules.clear();

    const int maxBits = m_dependentFieldLengthBytes * 8;
    for (int row = 0; row < m_exclusionTable->rowCount(); ++row)
    {
        auto cellText = [&](int col) -> QString {
            QTableWidgetItem* item = m_exclusionTable->item(row, col);
            return item ? item->text().trimmed() : QString();
        };

        const QString bitsText = cellText(EX_COL_BITS);
        const QString label = cellText(EX_COL_LABEL);
        const QString message = cellText(EX_COL_MESSAGE);

        if (bitsText.isEmpty() && label.isEmpty())
            continue;

        ConditionalBitExclusionRule exRule;
        exRule.validationLabel = label;
        exRule.invalidMessage = message;

        const QStringList rawParts = bitsText.split(',');
        QStringList parts;
        for (int pi = 0; pi < rawParts.size(); ++pi)
        {
            if (!rawParts.at(pi).trimmed().isEmpty())
                parts << rawParts.at(pi).trimmed();
        }
        if (parts.size() < 2)
        {
            errorMessage = QString("Exclusion rule row %1: must specify at least 2 comma-separated bit positions (e.g. 0,1).").arg(row + 1);
            return false;
        }

        for (int i = 0; i < parts.size(); ++i)
        {
            bool bitOk = false;
            const int bitPos = parts.at(i).trimmed().toInt(&bitOk);
            if (!bitOk || bitPos < 0 || bitPos >= maxBits)
            {
                errorMessage = QString("Exclusion rule row %1: bit '%2' is not valid (0-%3).").arg(row + 1).arg(parts.at(i).trimmed()).arg(maxBits - 1);
                return false;
            }
            exRule.mutuallyExclusiveBits.append(bitPos);
        }

        if (label.isEmpty())
        {
            errorMessage = QString("Exclusion rule row %1: validation label cannot be empty.").arg(row + 1);
            return false;
        }

        out.exclusionRules.append(exRule);
    }

    return true;
}

void ConditionalProfileDialog::onSaveClicked()
{
    ConditionalBitDecodeProfile collected;
    QString error;
    if (!collectProfile(collected, error))
    {
        QMessageBox::warning(this, "Invalid Profile", error);
        return;
    }
    m_profile = collected;
    accept();
}
