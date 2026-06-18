#ifndef SIMFIELDCONFIGURATIONDIALOG_H
#define SIMFIELDCONFIGURATIONDIALOG_H

// Simulator field editor for HEX messages. Columns:
//   Field Name | Byte Offset | Type | Length | Resolution | Value | Hex (auto) | Bits
//
// The Value cell is typed in the field's own type; the read-only Hex cell
// next to it shows the exact transmitted bytes IN REAL TIME (red + tooltip
// reason when the value does not fit). The Bits button opens the two-way
// BitValueEditorDialog for integer/raw/bool fields of 1..8 bytes.
//
// CSV ▾ / JSON ▾ menus import/export the field list (Value column included);
// dropping a .csv/.json file on the dialog imports it. Errors are collected
// and shown in ONE dialog, each with a reason and a suggested solution.

#include "AppTypes.h"

#include <QDialog>
#include <QList>
#include <QStringList>

class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QObject;
class QTableWidgetItem;
class QWidget;

namespace Ui
{
class SimFieldConfigurationDialog;
}

class SimFieldConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SimFieldConfigurationDialog(QWidget* parent = 0);
    ~SimFieldConfigurationDialog();

    void setPayloadLength(int payloadLengthBytes);
    void setFields(const QList<FieldDefinition>& fields);
    QList<FieldDefinition> fields() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    // Intercepts the drop on the field table's viewport so rows can be reordered
    // at the data level (cell widgets make Qt's built-in InternalMove unreliable).
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onAddFieldClicked();
    void onEditFieldClicked();
    void onRemoveFieldClicked();
    void onMoveFieldUpClicked();
    void onMoveFieldDownClicked();
    void onSaveClicked();
    void onImportCsvClicked();
    void onExportCsvClicked();
    void onTemplateCsvClicked();
    void onImportJsonClicked();
    void onExportJsonClicked();
    void onImportIcdClicked();
    void onFieldCellChanged(QTableWidgetItem* item);
    void onBitsRowClicked();

private:
    QString tableText(int row, int column) const;
    QString valueText(int row) const; // untrimmed — spaces matter for strings
    int selectedFieldRow() const;
    bool collectFields(QList<FieldDefinition>& fields, QStringList& problems) const;
    void refreshFieldTable();
    void setTypeCell(int row, FieldDataType dataType);
    void setEndianCell(int row, FieldEndianness endianness);
    FieldDataType dataTypeForRow(int row) const;
    FieldEndianness endiannessForRow(int row) const;
    void applyLengthStateForType(int row, FieldDataType dataType);
    int rowForTypeCombo(const QWidget* combo) const;
    int rowForEndianCombo(const QWidget* combo) const;
    int rowForBitsButton(const QWidget* button) const;
    // Lenient snapshot of every row to FieldDefinition (never fails) used for
    // drag-reorder and Move Up/Down so partially-edited rows survive a move.
    QList<FieldDefinition> snapshotAllRows() const;
    void reorderRows(QList<int> sourceRows, int targetRow);
    void moveSelectedRows(int delta); // -1 = up, +1 = down
    void setBitsCell(int row);
    void refreshHexCell(int row);
    bool fieldFromRow(int row, FieldDefinition& field, QString& problem) const;
    void importCsvFromPath(const QString& path);
    void importJsonFromPath(const QString& path);
    void applyImportedFields(const QList<FieldDefinition>& imported,
                             const QStringList& warnings,
                             const QString& sourceLabel);
    void showProblems(const QString& title, const QStringList& problems);

    int m_payloadLengthBytes;
    bool m_refreshing;
    QList<FieldDefinition> m_fields;
    Ui::SimFieldConfigurationDialog* ui;
};

#endif // SIMFIELDCONFIGURATIONDIALOG_H
