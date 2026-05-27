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
    void setOptionalHeaderHex(const QString& hex);

    // v15: ASTERIX support.
    void setDataFormat(const QString& format);
    void setAsterixCategory(int category);

    QString messageName() const;
    int payloadLengthBytes() const;
    QString optionalHeaderHex() const;
    QString dataFormat() const;
    int asterixCategory() const;

private slots:
    void onSaveClicked();
    // v15: when user picks ASTERIX from cmbDataFormat, pop the category picker.
    void onDataFormatChanged(int index);

private:
    // v15: opens AsterixCategoryPickerDialog; updates m_asterixCategory and the
    // lblAsterixCategory display. Returns true if the user picked a category.
    bool promptForAsterixCategory();
    int m_asterixCategory;

private:
    Ui::MessageDefinitionDialog* ui;
};

#endif // MESSAGEDEFINITIONDIALOG_H
