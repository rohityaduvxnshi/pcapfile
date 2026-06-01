#ifndef NMEAFIELDCONFIGURATIONDIALOG_H
#define NMEAFIELDCONFIGURATIONDIALOG_H

// Per-message field configurator for NMEA messages. Replaces the Hex
// FieldConfigurationDialog when MessageDefinition.dataFormat == "NMEA".
// Mirrors the old AsterixFieldConfigurationDialog, simplified: NMEA fields are
// ASCII tokens addressed by comma position, so there is no bit decoder and no
// byte offset/length editing — each row is just Enable + Custom Label over the
// registry's positional fields for the chosen sentence.
//
// fieldConfig() returns one FieldDefinition per enabled field, with `name` set
// to the (possibly overridden) column name and `nmeaFieldIndex` set to the
// registry field's comma position. The Hex columns (byteOffset/length/dataType)
// stay at their defaults and are ignored by the NMEA extraction path.

#include "AppTypes.h"

#include <QDialog>
#include <QList>
#include <QString>

namespace Ui
{
class NmeaFieldConfigurationDialog;
}

class NmeaFieldConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NmeaFieldConfigurationDialog(QWidget* parent = 0);
    ~NmeaFieldConfigurationDialog();

    void setSentenceType(const QString& formatter);
    void setExistingConfig(const QList<FieldDefinition>& fields);

    // Predefined sentence: one FieldDefinition per enabled registry field, in
    // registry order. Custom sentence: one FieldDefinition per editor row.
    QList<FieldDefinition> fieldConfig() const;

private slots:
    void onSaveClicked();
    // Custom mode only: add / remove a free-form field row.
    void onAddRowClicked();
    void onRemoveRowClicked();

private:
    void refreshTable();
    // Custom mode helpers.
    void addCustomRow(int fieldIndex, const QString& name, int valueKind);

    QString m_formatter;
    bool    m_customMode;   // true when the formatter is not in the registry

    struct RowState
    {
        bool    enabled;
        QString customLabel;
        RowState() : enabled(true) {}
    };
    QList<RowState> m_rows;   // sized to match the registry field list

    Ui::NmeaFieldConfigurationDialog* ui;
};

#endif // NMEAFIELDCONFIGURATIONDIALOG_H
