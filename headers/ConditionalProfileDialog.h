#ifndef CONDITIONALPROFILEDIALOG_H
#define CONDITIONALPROFILEDIALOG_H

#include "AppTypes.h"

#include <QDialog>

namespace Ui
{
class ConditionalProfileDialog;
}

class ConditionalProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConditionalProfileDialog(int dependentFieldLengthBytes,
                                      const ConditionalBitDecodeProfile& existing,
                                      QWidget* parent = 0);
    ~ConditionalProfileDialog();

    ConditionalBitDecodeProfile profile() const;

private slots:
    void onConfigureRulesClicked();
    void onAccepted();

private:
    void updateRuleCountLabel();

    int m_dependentFieldLengthBytes;
    ConditionalBitDecodeProfile m_profile;
    Ui::ConditionalProfileDialog* ui;
};

#endif // CONDITIONALPROFILEDIALOG_H
