#include "ConditionalProfileDialog.h"

#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"
#include "ui_ConditionalProfileDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace
{
const int EX_COL_BITS = 0;
const int EX_COL_LABEL = 1;
const int EX_COL_MESSAGE = 2;
}

ConditionalProfileDialog::ConditionalProfileDialog(int dependentFieldLengthBytes,
                                                     const ConditionalBitDecodeProfile& existing,
                                                     QWidget* parent)
    : QDialog(parent),
      m_dependentFieldLengthBytes(dependentFieldLengthBytes),
      m_profile(existing),
      ui(new Ui::ConditionalProfileDialog),
      m_valueEdit(0),
      m_nameEdit(0),
      m_rulesLabel(0),
      m_configureRulesBtn(0),
      m_exclusionTable(0),
      m_buttonBox(0)
{
    ui->setupUi(this);
    setWindowTitle("Configure Profile");
    setMinimumSize(620, 460);

    m_valueEdit = ui->edtControllerValue;
    m_nameEdit = ui->edtProfileName;
    m_rulesLabel = ui->lblRuleCount;
    m_configureRulesBtn = ui->btnConfigureRules;
    m_exclusionTable = ui->tblExclusionRules;
    m_buttonBox = ui->buttonBox;

    if (existing.controllerValue != 0 || !existing.profileName.isEmpty())
        m_valueEdit->setText(QString("0x%1").arg(existing.controllerValue, 0, 16).toUpper());
    m_nameEdit->setText(existing.profileName);
    ui->lblInfo->setText("Configure controller value, bit rules, and optional mutual exclusion rules.");

    m_exclusionTable->setColumnCount(3);
    m_exclusionTable->setHorizontalHeaderLabels(QStringList() << "Bits (e.g. 0,1)" << "Validation Label" << "Invalid Message");
    m_exclusionTable->horizontalHeader()->setStretchLastSection(true);
    m_exclusionTable->setAlternatingRowColors(true);
    m_exclusionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_exclusionTable->setSelectionMode(QAbstractItemView::SingleSelection);

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
    connect(ui->btnAddExclusion, SIGNAL(clicked()), this, SLOT(onAddExclusionClicked()));
    connect(ui->btnRemoveExclusion, SIGNAL(clicked()), this, SLOT(onRemoveExclusionClicked()));
    connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

ConditionalProfileDialog::~ConditionalProfileDialog()
{
    delete ui;
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
