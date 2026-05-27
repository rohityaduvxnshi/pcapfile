#include "AsterixCategoryPickerDialog.h"
#include "ui_AsterixCategoryPickerDialog.h"

#include "AsterixUapRegistry.h"
#include "Themes.h"

AsterixCategoryPickerDialog::AsterixCategoryPickerDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::AsterixCategoryPickerDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    const QList<int> categories = AsterixUapRegistry::supportedCategories();
    for (int i = 0; i < categories.size(); ++i)
    {
        const int cat = categories.at(i);
        const QString label = QString("CAT%1 — %2")
                                  .arg(cat, 3, 10, QChar('0'))
                                  .arg(AsterixUapRegistry::categoryDisplayName(cat));
        ui->cmbCategory->addItem(label, cat);
    }

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

AsterixCategoryPickerDialog::~AsterixCategoryPickerDialog()
{
    delete ui;
}

void AsterixCategoryPickerDialog::setSelectedCategory(int category)
{
    for (int i = 0; i < ui->cmbCategory->count(); ++i)
    {
        if (ui->cmbCategory->itemData(i).toInt() == category)
        {
            ui->cmbCategory->setCurrentIndex(i);
            return;
        }
    }
}

int AsterixCategoryPickerDialog::selectedCategory() const
{
    const int idx = ui->cmbCategory->currentIndex();
    if (idx < 0) return 0;
    return ui->cmbCategory->itemData(idx).toInt();
}
