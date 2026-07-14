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
    ui->spinPayloadLength->setRange(1, 1000000);
    ui->spinPayloadLength->setValue(1);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    // NMEA: react when the user switches between HEX and NMEA.
    connect(ui->cmbDataFormat, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onDataFormatChanged(int)));
}

MessageDefinitionDialog::~MessageDefinitionDialog()
{
    delete ui;
}

void MessageDefinitionDialog::setMessageName(const QString& name)
{
    ui->txtMessageName->setText(name);
}

void MessageDefinitionDialog::setPort(int port)
{
    ui->spinPort->setValue((port >= 1 && port <= 65535) ? port : 5000);
}

void MessageDefinitionDialog::setPayloadLength(int payloadLengthBytes)
{
    ui->spinPayloadLength->setValue(payloadLengthBytes > 0 ? payloadLengthBytes : 1);
}

void MessageDefinitionDialog::setOptionalHeaderHex(const QString& hex)
{
    ui->txtOptionalHeader->setText(hex);
}

QString MessageDefinitionDialog::messageName() const
{
    return ui->txtMessageName->text().trimmed();
}

int MessageDefinitionDialog::port() const
{
    return ui->spinPort->value();
}

int MessageDefinitionDialog::payloadLengthBytes() const
{
    return ui->spinPayloadLength->value();
}

QString MessageDefinitionDialog::optionalHeaderHex() const
{
    return ui->txtOptionalHeader->text().trimmed();
}

// NMEA: setters / getters for the data format selection.
void MessageDefinitionDialog::setDataFormat(const QString& format)
{
    const QString upper = format.trimmed().toUpper();
    const int target = (upper == "NMEA") ? 1 : 0;
    ui->cmbDataFormat->blockSignals(true);
    ui->cmbDataFormat->setCurrentIndex(target);
    ui->cmbDataFormat->blockSignals(false);
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

QString MessageDefinitionDialog::dataFormat() const
{
    return (ui->cmbDataFormat->currentIndex() == 1) ? QString("NMEA") : QString("HEX");
}

QString MessageDefinitionDialog::nmeaSentenceType() const
{
    return m_nmeaSentenceType;
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
        QMessageBox::warning(this, "Invalid Message", "Message name cannot be empty.");
        return;
    }

    if (payloadLengthBytes() <= 0)
    {
        QMessageBox::warning(this, "Invalid Message", "Payload length must be greater than 0.");
        return;
    }

    // NMEA: a sentence formatter must be chosen, and the byte-oriented optional
    // header check does not apply (matching is by sentence formatter).
    if (dataFormat() == "NMEA")
    {
        if (m_nmeaSentenceType.isEmpty())
        {
            QMessageBox::warning(this, "Invalid Message",
                "Data Format is NMEA but no sentence has been picked. Choose a sentence before saving.");
            return;
        }
        accept();
        return;
    }

    // v12: validate optional header is even-length hex if provided
    const QString headerHex = optionalHeaderHex();
    if (!headerHex.isEmpty())
    {
        if (headerHex.size() > 8)
        {
            QMessageBox::warning(this, "Invalid Header",
                "Optional header must be 0 to 8 hex characters (up to 4 bytes).");
            return;
        }
        if ((headerHex.size() % 2) != 0)
        {
            QMessageBox::warning(this, "Invalid Header",
                "Optional header must have an even number of hex characters.");
            return;
        }
        for (int i = 0; i < headerHex.size(); ++i)
        {
            const QChar c = headerHex.at(i);
            if (!(c.isDigit()
                  || (c >= 'a' && c <= 'f')
                  || (c >= 'A' && c <= 'F')))
            {
                QMessageBox::warning(this, "Invalid Header",
                    "Optional header may only contain hex characters (0-9, a-f).");
                return;
            }
        }
    }

    accept();
}
