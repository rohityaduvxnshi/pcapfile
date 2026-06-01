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

    // 3-char formatter mnemonic, e.g. "GGA", or a custom formatter the user
    // typed. Empty if nothing valid is chosen.
    QString selectedFormatter() const;

private slots:
    void onAccept();

private:
    Ui::NmeaSentencePickerDialog* ui;
};

#endif // NMEASENTENCEPICKERDIALOG_H
