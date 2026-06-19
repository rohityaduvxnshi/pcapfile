#include "MessageDefinitionDialog.h"
#include "ui_MessageDefinitionDialog.h"

#include "NmeaSentencePickerDialog.h"
#include "NmeaSentenceRegistry.h"
#include "Themes.h"

#include <QMessageBox>

MessageDefinitionDialog::MessageDefinitionDialog(QWidget* parent)
    : QDialog(parent),
      m_nmeaSentenceType(),
      ui(new Ui::MessageDefinitionDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    // NMEA: react when the user switches between HEX and NMEA.
    connect(ui->cmbDataFormat, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onDataFormatChanged(int)));

    applyFormatVisibility();
}

MessageDefinitionDialog::~MessageDefinitionDialog()
{
    delete ui;
}

void MessageDefinitionDialog::setMessageName(const QString& name)
{
    ui->txtMessageName->setText(name);
}

void MessageDefinitionDialog::setPayloadLength(int payloadLengthBytes)
{
    ui->spinPayloadLength->setValue(payloadLengthBytes > 0 ? payloadLengthBytes : 1);
}

QString MessageDefinitionDialog::messageName() const
{
    return ui->txtMessageName->text().trimmed();
}

int MessageDefinitionDialog::payloadLengthBytes() const
{
    return ui->spinPayloadLength->value();
}

void MessageDefinitionDialog::setDataFormat(const QString& format)
{
    const QString upper = format.trimmed().toUpper();
    const int target = (upper == "NMEA") ? 1 : 0;
    ui->cmbDataFormat->blockSignals(true);
    ui->cmbDataFormat->setCurrentIndex(target);
    ui->cmbDataFormat->blockSignals(false);
    applyFormatVisibility();
}

void MessageDefinitionDialog::setNmeaSentenceType(const QString& formatter)
{
    m_nmeaSentenceType = formatter.trimmed().toUpper();
    if (!m_nmeaSentenceType.isEmpty())
    {
        const QString display = NmeaSentenceRegistry::displayName(m_nmeaSentenceType);
        ui->lblNmeaSentence->setText(display.isEmpty() ? m_nmeaSentenceType : display);
    }
    else
    {
        ui->lblNmeaSentence->setText("-");
    }
}

void MessageDefinitionDialog::setNmeaTalker(const QString& talker)
{
    const QString trimmed = talker.trimmed().toUpper();
    ui->txtNmeaTalker->setText(trimmed.isEmpty() ? QString("GP") : trimmed);
}

void MessageDefinitionDialog::setSendFrequencyHz(double hz)
{
    ui->spinSendFrequency->setValue(hz > 0.0 ? hz : 1.0);
}

QString MessageDefinitionDialog::dataFormat() const
{
    return (ui->cmbDataFormat->currentIndex() == 1) ? QString("NMEA") : QString("HEX");
}

QString MessageDefinitionDialog::nmeaSentenceType() const
{
    return m_nmeaSentenceType;
}

QString MessageDefinitionDialog::nmeaTalker() const
{
    return ui->txtNmeaTalker->text().trimmed().toUpper();
}

double MessageDefinitionDialog::sendFrequencyHz() const
{
    return ui->spinSendFrequency->value();
}

// NMEA: open the sentence picker when the user switches to NMEA. If they cancel
// the picker, revert the dropdown back to HEX and clear the sentence label.
void MessageDefinitionDialog::onDataFormatChanged(int index)
{
    if (index == 1)
    {
        if (!promptForNmeaSentence())
        {
            ui->cmbDataFormat->blockSignals(true);
            ui->cmbDataFormat->setCurrentIndex(0);
            ui->cmbDataFormat->blockSignals(false);
            ui->lblNmeaSentence->setText("-");
        }
    }
    else
    {
        ui->lblNmeaSentence->setText("-");
    }

    applyFormatVisibility();
}

// The NMEA sentence/talker rows only apply to NMEA messages; the payload
// length only applies to HEX messages (an NMEA sentence's length follows
// from its tokens).
void MessageDefinitionDialog::applyFormatVisibility()
{
    const bool nmea = (ui->cmbDataFormat->currentIndex() == 1);
    ui->lblNmeaSentenceCaption->setVisible(nmea);
    ui->lblNmeaSentence->setVisible(nmea);
    ui->lblNmeaTalker->setVisible(nmea);
    ui->txtNmeaTalker->setVisible(nmea);
    ui->lblPayloadLength->setEnabled(!nmea);
    ui->spinPayloadLength->setEnabled(!nmea);
}

bool MessageDefinitionDialog::promptForNmeaSentence()
{
    NmeaSentencePickerDialog dlg(this);
    if (!m_nmeaSentenceType.isEmpty())
        dlg.setSelectedFormatter(m_nmeaSentenceType);
    if (dlg.exec() != QDialog::Accepted)
        return false;

    const QString picked = dlg.selectedFormatter();
    if (picked.isEmpty())
        return false;

    setNmeaSentenceType(picked);
    return true;
}

void MessageDefinitionDialog::onSaveClicked()
{
    const QString name = messageName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Message",
            "Message name cannot be empty.\nSolution: give the message a short unique name (e.g. Msg_A).");
        return;
    }

    if (dataFormat() == "NMEA")
    {
        if (m_nmeaSentenceType.isEmpty())
        {
            QMessageBox::warning(this, "Invalid Message",
                "Data Format is NMEA but no sentence has been picked.\n"
                "Solution: choose a sentence formatter (e.g. GGA) before saving.");
            return;
        }

        const QString talker = nmeaTalker();
        bool talkerOk = (talker.size() == 2);
        for (int i = 0; talkerOk && i < talker.size(); ++i)
            talkerOk = talker.at(i).isLetterOrNumber();
        if (!talkerOk)
        {
            QMessageBox::warning(this, "Invalid Message",
                QString("'%1' is not a valid 2-character NMEA talker id.\n"
                        "Solution: use a 2-letter talker such as GP, GN, HE or II.").arg(talker));
            return;
        }

        accept();
        return;
    }

    if (payloadLengthBytes() <= 0)
    {
        QMessageBox::warning(this, "Invalid Message",
            "Payload length must be greater than 0.\nSolution: set the total byte size of the payload this message sends.");
        return;
    }

    accept();
}
