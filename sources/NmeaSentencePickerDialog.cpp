#include "NmeaSentencePickerDialog.h"
#include "ui_NmeaSentencePickerDialog.h"

#include "NmeaSentenceRegistry.h"
#include "Themes.h"

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

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

NmeaSentencePickerDialog::~NmeaSentencePickerDialog()
{
    delete ui;
}

void NmeaSentencePickerDialog::setSelectedFormatter(const QString& formatter)
{
    const int idx = ui->cmbSentence->findData(formatter.trimmed().toUpper());
    if (idx >= 0)
        ui->cmbSentence->setCurrentIndex(idx);
}

QString NmeaSentencePickerDialog::selectedFormatter() const
{
    return ui->cmbSentence->currentData().toString();
}
