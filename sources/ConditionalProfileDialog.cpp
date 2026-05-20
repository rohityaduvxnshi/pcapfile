#include "ConditionalProfileDialog.h"
#include "ui_ConditionalProfileDialog.h"

#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"

#include <QMessageBox>

ConditionalProfileDialog::ConditionalProfileDialog(int dependentFieldLengthBytes,
                                                   const ConditionalBitDecodeProfile& existing,
                                                   QWidget* parent)
    : QDialog(parent),
      m_dependentFieldLengthBytes(dependentFieldLengthBytes),
      m_profile(existing),
      ui(new Ui::ConditionalProfileDialog)
{
    ui->setupUi(this);

    ui->lblInfo->setText(QString("Dependent field length: %1 byte(s) | Available bits: 0-%2")
                             .arg(m_dependentFieldLengthBytes)
                             .arg((m_dependentFieldLengthBytes * 8) - 1));

    ui->edtControllerValue->setText(
        existing.profileName.isEmpty() ? QString() : QString("0x%1").arg(QString::number(existing.controllerValue, 16).toUpper()));
    ui->edtProfileName->setText(existing.profileName);

    updateRuleCountLabel();

    connect(ui->btnConfigureRules, SIGNAL(clicked()), this, SLOT(onConfigureRulesClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onAccepted()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

ConditionalProfileDialog::~ConditionalProfileDialog()
{
    delete ui;
}

ConditionalBitDecodeProfile ConditionalProfileDialog::profile() const
{
    return m_profile;
}

void ConditionalProfileDialog::updateRuleCountLabel()
{
    const int count = m_profile.bitDecodeRules.size();
    if (count == 0)
        ui->lblRuleCount->setText("No rules configured");
    else
        ui->lblRuleCount->setText(QString("%1 rule(s) configured").arg(count));
}

void ConditionalProfileDialog::onConfigureRulesClicked()
{
    const QString profileName = ui->edtProfileName->text().trimmed();
    const QString displayName = profileName.isEmpty() ? "(profile)" : profileName;

    BitfieldDecoderDialog dlg(displayName, m_dependentFieldLengthBytes, m_profile.bitDecodeRules, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_profile.bitDecodeRules = dlg.rules();
        updateRuleCountLabel();
    }
}

void ConditionalProfileDialog::onAccepted()
{
    const QString valueText = ui->edtControllerValue->text().trimmed();
    const QString profileName = ui->edtProfileName->text().trimmed();

    if (valueText.isEmpty())
    {
        QMessageBox::warning(this, "Profile Configuration", "Controller value cannot be empty.");
        return;
    }

    bool ok = false;
    const quint64 controllerValue = valueText.toULongLong(&ok, 0);
    if (!ok)
    {
        QMessageBox::warning(this, "Profile Configuration",
                             "Invalid controller value. Use decimal (e.g. 1, 255) or hex (e.g. 0x01, 0xFF).");
        return;
    }

    if (profileName.isEmpty())
    {
        QMessageBox::warning(this, "Profile Configuration", "Profile name cannot be empty.");
        return;
    }

    QString ruleError;
    if (!BitfieldDecoder::validateRules(m_profile.bitDecodeRules, m_dependentFieldLengthBytes, ruleError))
    {
        QMessageBox::warning(this, "Profile Configuration", "Bit rules are invalid:\n" + ruleError);
        return;
    }

    m_profile.controllerValue = controllerValue;
    m_profile.profileName = profileName;
    accept();
}
