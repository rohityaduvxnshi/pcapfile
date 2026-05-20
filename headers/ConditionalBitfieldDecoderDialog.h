#ifndef CONDITIONALBITFIELDDECODERDIALOG_H
#define CONDITIONALBITFIELDDECODERDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

class QComboBox;
class QLabel;
class QTableWidget;

class ConditionalBitfieldDecoderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConditionalBitfieldDecoderDialog(const QString& dependentFieldName,
                                               int dependentFieldLengthBytes,
                                               const QList<FieldDefinition>& allFields,
                                               const ConditionalBitfieldDecoderConfig& existing,
                                               QWidget* parent = 0);

    ConditionalBitfieldDecoderConfig decoder() const;

private slots:
    void onAddProfileClicked();
    void onEditProfileClicked();
    void onRemoveProfileClicked();
    void onSaveClicked();

private:
    void refreshProfileTable();
    int selectedProfileRow() const;

    QString m_dependentFieldName;
    int m_dependentFieldLengthBytes;
    QList<FieldDefinition> m_allFields;
    ConditionalBitfieldDecoderConfig m_decoder;

    QComboBox* m_controllerCombo;
    QComboBox* m_unknownCombo;
    QTableWidget* m_profileTable;
};

#endif // CONDITIONALBITFIELDDECODERDIALOG_H
