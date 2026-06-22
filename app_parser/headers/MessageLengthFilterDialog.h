#ifndef MESSAGELENGTHFILTERDIALOG_H
#define MESSAGELENGTHFILTERDIALOG_H

#include "ConnectionTypes.h"
#include "MessageDefinition.h"

#include <QDialog>
#include <QList>
#include <QtGlobal>

namespace Ui
{
class MessageLengthFilterDialog;
}

class MessageLengthFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MessageLengthFilterDialog(QWidget* parent = 0);
    ~MessageLengthFilterDialog();

    void setPort(quint16 port);
    void setMessages(const QList<MessageDefinition>& messages);
    QList<MessageDefinition> messages() const;

    // Multi-connection (live mode only): when set non-empty, a Connection column
    // appears so each message can be bound to one of these live connections. Other
    // modes (port/header) leave this empty and the column is hidden.
    void setConnections(const QList<ConnectionDefinition>& connections);

private slots:
    void onAddMessageClicked();
    void onEditMessageClicked();
    void onRemoveMessageClicked();
    void onConfigureSelectedFieldsClicked();
    void onConfigureFieldButtonClicked();
    void onSaveClicked();
    // v13: per-row Compare Options button
    void onCompareOptionsButtonClicked();
    // Multi-connection: a per-row connection combo changed.
    void onConnectionComboChanged(int index);
    // JSON import/export of the message list, in-dialog.
    void onImportMessagesJsonClicked();
    void onExportMessagesJsonClicked();

private:
    int selectedMessageRow() const;
    bool hasDuplicateName(const QString& name, int ignoreIndex) const;
    bool hasDuplicateLength(int payloadLengthBytes, int ignoreIndex) const;
    bool hasDuplicateSignature(const MessageDefinition& message, int ignoreIndex) const;
    bool validateMessage(const MessageDefinition& message, int ignoreIndex, QString& errorMessage) const;
    bool validateFieldsFitPayload(const MessageDefinition& message, QString& errorMessage) const;
    QString fieldStatusText(const MessageDefinition& message) const;
    void configureMessageAt(int row);
    void refreshTable();

    quint16 m_port;
    QList<MessageDefinition> m_messages;
    QList<ConnectionDefinition> m_connections;
    Ui::MessageLengthFilterDialog* ui;
};

#endif // MESSAGELENGTHFILTERDIALOG_H
