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
    // NMEA: data format selection.
    void setDataFormat(const QString& format);
    void setNmeaSentenceType(const QString& formatter);

    QString messageName() const;
    int payloadLengthBytes() const;
    QString optionalHeaderHex() const;
    // NMEA accessors.
    QString dataFormat() const;
    QString nmeaSentenceType() const;

private slots:
    void onSaveClicked();
    // NMEA: open the sentence picker when the user switches to NMEA.
    void onDataFormatChanged(int index);

private:
    bool promptForNmeaSentence();

    QString m_nmeaSentenceType;
    Ui::MessageDefinitionDialog* ui;
};

#endif // MESSAGEDEFINITIONDIALOG_H
