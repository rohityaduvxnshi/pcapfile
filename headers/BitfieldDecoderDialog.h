#ifndef BITFIELDDECODERDIALOG_H
#define BITFIELDDECODERDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

class QLabel;
class QTableWidget;

class BitfieldDecoderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BitfieldDecoderDialog(const QString& fieldName,
                                   int fieldLengthBytes,
                                   const QList<BitDecodeRule>& existingRules,
                                   QWidget* parent = 0);

    QList<BitDecodeRule> rules() const;

private slots:
    void onAddRuleClicked();
    void onEditRuleClicked();
    void onRemoveRuleClicked();
    void onSaveClicked();

private:
    void refreshTable();
    int selectedRuleRow() const;

    QString m_fieldName;
    int m_fieldLengthBytes;
    QList<BitDecodeRule> m_rules;

    QLabel* m_infoLabel;
    QTableWidget* m_ruleTable;
};

#endif // BITFIELDDECODERDIALOG_H
