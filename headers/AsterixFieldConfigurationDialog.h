#ifndef ASTERIXFIELDCONFIGURATIONDIALOG_H
#define ASTERIXFIELDCONFIGURATIONDIALOG_H

// v15: per-message field configurator for ASTERIX messages. Replaces the
// Hex FieldConfigurationDialog when MessageDefinition.dataFormat == "ASTERIX".
//
// Each row corresponds to one UAP item for the selected category. The user can:
//   - Enable / disable inclusion in CSV (per item)
//   - Override the column name with a custom label
//   - Attach a bit decoder (reusing BitfieldDecoderDialog and BitfieldDecoder)
//
// Output is QList<FieldDefinition> where each entry carries asterixItemId set
// to the UAP item ID. byteOffset / length / dataType remain unused for these
// fields — the extraction pipeline routes them through AsterixDecoder instead
// of the byte-offset reader.

#include "AppTypes.h"

#include <QDialog>
#include <QList>

class QTableWidgetItem;
class QPushButton;

namespace Ui
{
class AsterixFieldConfigurationDialog;
}

class AsterixFieldConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AsterixFieldConfigurationDialog(QWidget* parent = 0);
    ~AsterixFieldConfigurationDialog();

    void setCategory(int category);
    void setExistingConfig(const QList<FieldDefinition>& fields);

    // Returns one FieldDefinition per enabled UAP item, in UAP order.
    QList<FieldDefinition> fieldConfig() const;

private slots:
    void onBitDecoderClicked();
    void onSaveClicked();
    void onEnabledToggled(bool checked);

private:
    void refreshTable();
    int  rowForBitButton(const QObject* sender) const;

    int m_category;
    // Holds the bit decoder rules + enable flag + custom label per UAP row.
    // Sized to match the UAP item list once setCategory is called.
    struct RowState
    {
        bool                     enabled;
        QString                  customLabel;
        QList<BitDecodeRule>     bitRules;
        bool                     hasBitDecoder;
        RowState() : enabled(true), hasBitDecoder(false) {}
    };
    QList<RowState> m_rows;

    Ui::AsterixFieldConfigurationDialog* ui;
};

#endif // ASTERIXFIELDCONFIGURATIONDIALOG_H
