#ifndef MESSAGEDEFINITIONDIALOG_H
#define MESSAGEDEFINITIONDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui
{
class MessageDefinitionDialog;
}

// Simulator message editor: name, payload length, data format (HEX/NMEA with
// sentence picker + talker) and the per-message send rate in Hz. The parser's
// optional-header disambiguator does not exist here — the simulator is the
// source of the data, nothing needs to be matched.
class MessageDefinitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MessageDefinitionDialog(QWidget* parent = 0);
    ~MessageDefinitionDialog();

    void setMessageName(const QString& name);
    void setPayloadLength(int payloadLengthBytes);
    void setDataFormat(const QString& format);
    void setNmeaSentenceType(const QString& formatter);
    void setNmeaTalker(const QString& talker);
    void setSendFrequencyHz(double hz);

    QString messageName() const;
    int payloadLengthBytes() const;
    QString dataFormat() const;
    QString nmeaSentenceType() const;
    QString nmeaTalker() const;
    double sendFrequencyHz() const;

private slots:
    void onSaveClicked();
    // NMEA: open the sentence picker when the user switches to NMEA.
    void onDataFormatChanged(int index);

private:
    bool promptForNmeaSentence();
    void applyFormatVisibility();

    QString m_nmeaSentenceType;
    Ui::MessageDefinitionDialog* ui;
};

#endif // MESSAGEDEFINITIONDIALOG_H
