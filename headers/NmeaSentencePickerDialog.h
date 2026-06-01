#ifndef NMEASENTENCEPICKERDIALOG_H
#define NMEASENTENCEPICKERDIALOG_H

// Small modal to pick an NMEA sentence formatter from the registry. Mirrors
// the old AsterixCategoryPickerDialog. Used by MessageDefinitionDialog when the
// user switches the Data Format to NMEA.

#include <QDialog>
#include <QString>

namespace Ui
{
class NmeaSentencePickerDialog;
}

class NmeaSentencePickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NmeaSentencePickerDialog(QWidget* parent = 0);
    ~NmeaSentencePickerDialog();

    void setSelectedFormatter(const QString& formatter);

    // 3-char formatter mnemonic, e.g. "GGA". Empty if nothing valid is chosen.
    QString selectedFormatter() const;

private:
    Ui::NmeaSentencePickerDialog* ui;
};

#endif // NMEASENTENCEPICKERDIALOG_H
