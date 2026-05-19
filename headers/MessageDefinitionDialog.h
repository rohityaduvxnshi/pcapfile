#ifndef MESSAGEDEFINITIONDIALOG_H
#define MESSAGEDEFINITIONDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui
{
class MessageDefinitionDialog;
}

class MessageDefinitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MessageDefinitionDialog(QWidget* parent = 0);
    ~MessageDefinitionDialog();

    void setMessageName(const QString& name);
    void setPayloadLength(int payloadLengthBytes);

    QString messageName() const;
    int payloadLengthBytes() const;

private slots:
    void onSaveClicked();

private:
    Ui::MessageDefinitionDialog* ui;
};

#endif // MESSAGEDEFINITIONDIALOG_H
