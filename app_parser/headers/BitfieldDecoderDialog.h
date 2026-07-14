#ifndef BITFIELDDECODERDIALOG_H
#define BITFIELDDECODERDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

class QTableWidget;

namespace Ui
{
class BitfieldDecoderDialog;
}

class BitfieldDecoderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BitfieldDecoderDialog(const QString& fieldName,
                                   int fieldLengthBytes,
                                   const QList<BitDecodeRule>& existingRules,
                                   QWidget* parent = 0);
    ~BitfieldDecoderDialog();

    QList<BitDecodeRule> rules() const;

private slots:
    void onAddRuleClicked();
    void onEditRuleClicked();
    void onRemoveRuleClicked();
    void onSaveClicked();
    void onImportJsonClicked();
    void onExportClicked();

private:
    void refreshTable();
    int selectedRuleRow() const;

    QString m_fieldName;
    int m_fieldLengthBytes;
    QList<BitDecodeRule> m_rules;

    Ui::BitfieldDecoderDialog* ui;
    QTableWidget* m_ruleTable;
};

#endif // BITFIELDDECODERDIALOG_H
