#include "MessageDefinitionDialog.h"
#include "ui_MessageDefinitionDialog.h"

#include <QMessageBox>

MessageDefinitionDialog::MessageDefinitionDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::MessageDefinitionDialog)
{
    ui->setupUi(this);
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

QString MessageDefinitionDialog::messageName() const
{
    return ui->txtMessageName->text().trimmed();
}

int MessageDefinitionDialog::payloadLengthBytes() const
{
    return ui->spinPayloadLength->value();
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

    accept();
}
