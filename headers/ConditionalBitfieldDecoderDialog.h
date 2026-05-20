#ifndef CONDITIONALBITFIELDDECODERDIALOG_H
#define CONDITIONALBITFIELDDECODERDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

namespace Ui
{
class ConditionalBitfieldDecoderDialog;
}

class ConditionalBitfieldDecoderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConditionalBitfieldDecoderDialog(const QString& dependentFieldName,
                                              int dependentFieldLengthBytes,
                                              const QList<FieldDefinition>& allFields,
                                              const ConditionalBitfieldDecoderConfig& existing,
                                              QWidget* parent = 0);
    ~ConditionalBitfieldDecoderDialog();

    ConditionalBitfieldDecoderConfig decoder() const;

private slots:
    void onAddProfileClicked();
    void onEditProfileClicked();
    void onRemoveProfileClicked();
    void onSaveClicked();

private:
    void refreshTable();
    int selectedProfileRow() const;
    ConditionalBitfieldDecoderConfig collectDecoder() const;

    QString m_dependentFieldName;
    int m_dependentFieldLengthBytes;
    QList<FieldDefinition> m_allFields;
    QList<ConditionalBitDecodeProfile> m_profiles;

    Ui::ConditionalBitfieldDecoderDialog* ui;
};

#endif // CONDITIONALBITFIELDDECODERDIALOG_H
