#include "NmeaSentencePickerDialog.h"
#include "ui_NmeaSentencePickerDialog.h"

#include "NmeaSentenceRegistry.h"
#include "Themes.h"

#include <QMessageBox>

NmeaSentencePickerDialog::NmeaSentencePickerDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::NmeaSentencePickerDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    const QList<QString> formatters = NmeaSentenceRegistry::supportedFormatters();
    for (int i = 0; i < formatters.size(); ++i)
    {
        const QString f = formatters.at(i);
        ui->cmbSentence->addItem(NmeaSentenceRegistry::displayName(f), f);
    }

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

// Validate the custom formatter (if any) before accepting. A custom formatter
// must be exactly 3 alphanumeric characters so it can be matched against the
// 3 characters after the 2-character talker in an incoming sentence.
void NmeaSentencePickerDialog::onAccept()
{
    const QString custom = ui->txtCustom->text().trimmed().toUpper();
    if (!custom.isEmpty())
    {
        if (custom.size() != 3)
        {
            QMessageBox::warning(this, "Custom Sentence",
                "A custom formatter must be exactly 3 characters (e.g. RMC).");
            return;
        }
        for (int i = 0; i < custom.size(); ++i)
        {
            const QChar c = custom.at(i);
            if (!(c.isLetterOrNumber()))
            {
                QMessageBox::warning(this, "Custom Sentence",
                    "A custom formatter may only contain letters and digits.");
                return;
            }
        }
    }
    accept();
}

NmeaSentencePickerDialog::~NmeaSentencePickerDialog()
{
    delete ui;
}

void NmeaSentencePickerDialog::setSelectedFormatter(const QString& formatter)
{
    const QString f = formatter.trimmed().toUpper();
    const int idx = ui->cmbSentence->findData(f);
    if (idx >= 0)
        ui->cmbSentence->setCurrentIndex(idx);
    else if (!f.isEmpty())
        ui->txtCustom->setText(f);   // a previously-saved custom formatter
}

QString NmeaSentencePickerDialog::selectedFormatter() const
{
    // A non-empty custom formatter wins over the predefined selection.
    const QString custom = ui->txtCustom->text().trimmed().toUpper();
    if (!custom.isEmpty())
        return custom;
    return ui->cmbSentence->currentData().toString();
}
