#ifndef FIELDCONFIGURATIONDIALOG_H
#define FIELDCONFIGURATIONDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

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

private slots:
    void onAddFieldClicked();
    void onEditFieldClicked();
    void onRemoveFieldClicked();
    void onBitfieldDecoderClicked();
    void onConditionalDecoderClicked();
    void onSaveClicked();

private:
    QString tableText(int row, int column) const;
    int selectedFieldRow() const;
    bool collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const;
    QList<FieldDefinition> peekFields() const;
    void refreshFieldTable();
    void setDecoderCell(int row, const QList<BitDecodeRule>& rules);
    void setConditionalDecoderCell(int row, const ConditionalBitfieldDecoderConfig& decoder);

    int m_payloadLengthBytes;
    QList<FieldDefinition> m_fields;
    Ui::FieldConfigurationDialog* ui;
};

#endif // FIELDCONFIGURATIONDIALOG_H
