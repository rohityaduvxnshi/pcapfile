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
    void onSaveClicked();

private:
    QString tableText(int row, int column) const;
    int selectedFieldRow() const;
    bool collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const;
    void refreshFieldTable();
    void setDecoderCell(int row, const QList<BitDecodeRule>& rules);

    int m_payloadLengthBytes;
    QList<FieldDefinition> m_fields;
    Ui::FieldConfigurationDialog* ui;
};

#endif // FIELDCONFIGURATIONDIALOG_H
