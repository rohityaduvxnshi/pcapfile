#ifndef BITFIELDRULEDIALOG_H
#define BITFIELDRULEDIALOG_H

#include "AppTypes.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QTableWidget;

class BitfieldRuleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BitfieldRuleDialog(int maxBitCount, QWidget* parent = 0);
    explicit BitfieldRuleDialog(int maxBitCount, const BitDecodeRule& rule, QWidget* parent = 0);

    BitDecodeRule rule() const;

private slots:
    void onGenerateMappingsClicked();
    void onAddMappingClicked();
    void onRemoveMappingClicked();
    void onSaveClicked();

private:
    void buildUi();
    void loadRule(const BitDecodeRule& rule);
    bool collectRule(BitDecodeRule& outRule, QString& errorMessage) const;
    void generateMappingRows();
    QString currentTypeCode() const;

    int m_maxBitCount;
    BitDecodeRule m_rule;
    QLineEdit* m_labelEdit;
    QLineEdit* m_bitsEdit;
    QComboBox* m_typeCombo;
    QComboBox* m_unknownCombo;
    QTableWidget* m_mappingTable;
};

#endif // BITFIELDRULEDIALOG_H
