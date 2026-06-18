#ifndef COMPAREOPTIONSDIALOG_H
#define COMPAREOPTIONSDIALOG_H

#include "AppTypes.h"

#include <QDialog>
#include <QString>

namespace Ui
{
class CompareOptionsDialog;
}

class CompareOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CompareOptionsDialog(QWidget* parent = 0);
    ~CompareOptionsDialog();

    void setPayloadLength(int payloadLengthBytes);
    void setConfig(const CompareOptionsConfig& cfg);
    void setHasCompareOptions(bool enabled);

    CompareOptionsConfig config() const;
    bool hasCompareOptions() const;

private slots:
    void onSaveClicked();

private:
    Ui::CompareOptionsDialog* ui;
    int m_payloadLengthBytes;
};

#endif // COMPAREOPTIONSDIALOG_H
