#include "MessageDefinitionDialog.h"
#include "ui_MessageDefinitionDialog.h"

#include "Themes.h"

#include <QMessageBox>

MessageDefinitionDialog::MessageDefinitionDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::MessageDefinitionDialog)
{
    ui->setupUi(this);
    Themes::apply(this);
    ui->spinPayloadLength->setRange(1, 1000000);
    ui->spinPayloadLength->setValue(1);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
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

    accept();
}
