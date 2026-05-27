#ifndef ASTERIXCATEGORYPICKERDIALOG_H
#define ASTERIXCATEGORYPICKERDIALOG_H

// v15: small modal that lets the user pick one supported ASTERIX category.
// Populated from AsterixUapRegistry::supportedCategories(); shows the human
// name from categoryDisplayName(). Opens with the current selection (if any)
// pre-selected.

#include <QDialog>

namespace Ui
{
class AsterixCategoryPickerDialog;
}

class AsterixCategoryPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AsterixCategoryPickerDialog(QWidget* parent = 0);
    ~AsterixCategoryPickerDialog();

    void setSelectedCategory(int category);
    int  selectedCategory() const;

private:
    Ui::AsterixCategoryPickerDialog* ui;
};

#endif // ASTERIXCATEGORYPICKERDIALOG_H
