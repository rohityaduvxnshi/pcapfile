#ifndef CONDITIONALPROFILEDIALOG_H
#define CONDITIONALPROFILEDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QList>

class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QDialogButtonBox;

class ConditionalProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConditionalProfileDialog(int dependentFieldLengthBytes,
                                       const ConditionalBitDecodeProfile& existing,
                                       QWidget* parent = 0);

    ConditionalBitDecodeProfile profile() const;

private slots:
    void onConfigureRulesClicked();
    void onAddExclusionClicked();
    void onRemoveExclusionClicked();
    void onSaveClicked();

private:
    bool collectProfile(ConditionalBitDecodeProfile& out, QString& errorMessage) const;
    void refreshRulesLabel();

    int m_dependentFieldLengthBytes;
    ConditionalBitDecodeProfile m_profile;

    QLineEdit* m_valueEdit;
    QLineEdit* m_nameEdit;
    QLabel* m_rulesLabel;
    QPushButton* m_configureRulesBtn;
    QTableWidget* m_exclusionTable;
    QDialogButtonBox* m_buttonBox;
};

#endif // CONDITIONALPROFILEDIALOG_H
