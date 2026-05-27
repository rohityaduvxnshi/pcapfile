#include "MessageDefinitionDialog.h"
#include "ui_MessageDefinitionDialog.h"

#include "AsterixCategoryPickerDialog.h"
#include "AsterixUapRegistry.h"
#include "Themes.h"

#include <QMessageBox>

MessageDefinitionDialog::MessageDefinitionDialog(QWidget* parent)
    : QDialog(parent),
      m_asterixCategory(0),
      ui(new Ui::MessageDefinitionDialog)
{
    ui->setupUi(this);
    Themes::apply(this);
    ui->spinPayloadLength->setRange(1, 1000000);
    ui->spinPayloadLength->setValue(1);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    // v15: react when the user switches between Hex and ASTERIX.
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

int MessageDefinitionDialog::payloadLengthBytes() const
{
    return ui->spinPayloadLength->value();
}

QString MessageDefinitionDialog::optionalHeaderHex() const
{
    return ui->txtOptionalHeader->text().trimmed();
}

// v15: setters / getters for ASTERIX selection.
void MessageDefinitionDialog::setDataFormat(const QString& format)
{
    const QString upper = format.trimmed().toUpper();
    const int target = (upper == "ASTERIX") ? 1 : 0;
    // Block the signal so setting the dropdown doesn't pop the picker as a
    // side-effect of programmatic init (Edit dialog opening with existing state).
    ui->cmbDataFormat->blockSignals(true);
    ui->cmbDataFormat->setCurrentIndex(target);
    ui->cmbDataFormat->blockSignals(false);
}

void MessageDefinitionDialog::setAsterixCategory(int category)
{
    m_asterixCategory = category;
    if (category > 0)
    {
        const QString display = QString("CAT%1 — %2")
                                    .arg(category, 3, 10, QChar('0'))
                                    .arg(AsterixUapRegistry::categoryDisplayName(category));
        ui->lblAsterixCategory->setText(display);
    }
    else
    {
        ui->lblAsterixCategory->setText("-");
    }
}

QString MessageDefinitionDialog::dataFormat() const
{
    return (ui->cmbDataFormat->currentIndex() == 1) ? QString("ASTERIX") : QString("HEX");
}

int MessageDefinitionDialog::asterixCategory() const
{
    return m_asterixCategory;
}

// v15: open the category picker when the user switches to ASTERIX. If they
// cancel the picker, revert the dropdown back to Hex (and keep m_asterixCategory
// at whatever it was — typically 0). When switching back to Hex, leave the
// category in memory but blank the label so the dialog reflects the new mode.
void MessageDefinitionDialog::onDataFormatChanged(int index)
{
    if (index == 1)
    {
        if (!promptForAsterixCategory())
        {
            ui->cmbDataFormat->blockSignals(true);
            ui->cmbDataFormat->setCurrentIndex(0);
            ui->cmbDataFormat->blockSignals(false);
            ui->lblAsterixCategory->setText("-");
        }
    }
    else
    {
        ui->lblAsterixCategory->setText("-");
    }
}

bool MessageDefinitionDialog::promptForAsterixCategory()
{
    AsterixCategoryPickerDialog dlg(this);
    if (m_asterixCategory > 0)
        dlg.setSelectedCategory(m_asterixCategory);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    const int picked = dlg.selectedCategory();
    if (picked <= 0) return false;
    setAsterixCategory(picked);
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

    // v15: when ASTERIX is selected, a category must be chosen.
    if (dataFormat() == "ASTERIX" && m_asterixCategory <= 0)
    {
        QMessageBox::warning(this, "Invalid Message",
            "Data Format is ASTERIX but no category has been picked. Choose a category before saving.");
        return;
    }

    accept();
}
