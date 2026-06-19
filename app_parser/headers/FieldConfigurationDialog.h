#ifndef FIELDCONFIGURATIONDIALOG_H
#define FIELDCONFIGURATIONDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

class QDragEnterEvent;
class QDropEvent;

namespace Ui
{
class FieldConfigurationDialog;
}

class FieldConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FieldConfigurationDialog(QWidget* parent = 0);
    ~FieldConfigurationDialog();

    void setPayloadLength(int payloadLengthBytes);
    void setFields(const QList<FieldDefinition>& fields);
    QList<FieldDefinition> fields() const;

    // Offset display unit ("BYTES" / "WORDS", 1 word = 2 bytes). The offset
    // column shows / accepts this unit; field.byteOffset is always in bytes.
    void setOffsetUnit(const QString& unit);
    QString offsetUnit() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddFieldClicked();
    void onEditFieldClicked();
    void onRemoveFieldClicked();
    void onBitfieldDecoderClicked();
    void onConditionalDecoderClicked();
    void onSaveClicked();
    void onImportCsvClicked();
    void onExportCsvClicked();
    void onTemplateCsvClicked();
    void onImportJsonClicked();
    void onExportJsonClicked();
    void onOffsetUnitChanged();

    // v12: per-row Edit buttons for the Bit Decoder / Cond. Decoder columns.
    // These resolve the clicked row via cellWidget lookup, select it, then delegate
    // to the existing onBitfieldDecoderClicked / onConditionalDecoderClicked slots.
    void onBitfieldEditRowClicked();
    void onConditionalEditRowClicked();

private:
    QString tableText(int row, int column) const;
    int selectedFieldRow() const;
    bool collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const;
    QList<FieldDefinition> peekFields() const;
    void refreshFieldTable();
    void setTypeCell(int row, FieldDataType dataType);
    FieldDataType dataTypeForRow(int row) const;
    void applyLengthStateForType(int row, FieldDataType dataType);
    int rowForTypeCombo(const QWidget* combo) const;
    void setDecoderCell(int row, const QList<BitDecodeRule>& rules);
    void setConditionalDecoderCell(int row, const ConditionalBitfieldDecoderConfig& decoder);

    void importCsvFromPath(const QString& path);
    void importJsonFromPath(const QString& path);
    void updateOffsetColumnHeader();

    int m_payloadLengthBytes;
    QString m_offsetUnit; // "BYTES" (default) or "WORDS"
    QList<FieldDefinition> m_fields;
    Ui::FieldConfigurationDialog* ui;
};

#endif // FIELDCONFIGURATIONDIALOG_H
