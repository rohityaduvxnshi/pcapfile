#ifndef SIMCONNECTIONSDIALOG_H
#define SIMCONNECTIONSDIALOG_H

// Simulator connection manager. Edits the list of SEND destinations (each a
// UDP / TCP / Serial endpoint). Messages reference a connection by id
// (MessageDefinition::connectionId); unbound messages use the first connection.
// Also exposes a static factory that turns a ConnectionDefinition into the
// matching DataSender, reused by both this dialog's Test button and the main
// window's send loop so the two never disagree on how a connection is opened.

#include "ConnectionTypes.h"

#include <QDialog>
#include <QList>

class DataSender;

namespace Ui
{
class SimConnectionsDialog;
}

class SimConnectionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SimConnectionsDialog(QWidget* parent = 0);
    ~SimConnectionsDialog() override;

    void setConnections(const QList<ConnectionDefinition>& connections);
    QList<ConnectionDefinition> connections() const;

    // Build the DataSender for a connection (caller owns it). Never null.
    static DataSender* buildSender(const ConnectionDefinition& c, QObject* parent);

private slots:
    void onAddConnection();
    void onRemoveConnection();
    void onSelectionChanged();
    void onTransportChanged();
    void onEditorChanged();
    void onRefreshSerialPorts();
    void onTestConnection();
    void onAccept();

private:
    void refreshTable();
    void loadEditor(int row);
    void commitEditor();
    int selectedRow() const;
    void updateEditorEnabled();
    void populateSerialPorts(const QString& keep);
    // Fill the UDP "Send via adapter" combo from the machine's interfaces and
    // select the one whose bind address matches `keepAddress`.
    void populateUdpAdapters(const QString& keepAddress);

    Ui::SimConnectionsDialog* ui;
    QList<ConnectionDefinition> m_connections;
    bool m_loading;
};

#endif // SIMCONNECTIONSDIALOG_H
