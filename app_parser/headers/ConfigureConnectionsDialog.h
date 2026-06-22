#ifndef CONFIGURECONNECTIONSDIALOG_H
#define CONFIGURECONNECTIONSDIALOG_H

// Parser live-mode connection manager. Edits the list of receive connections the
// app binds when live capture starts. Each connection is one adapter + port (and,
// for TCP, a Listen/Connect role + remote host). Messages reference a connection
// by id (MessageDefinition::connectionId) so traffic from different adapters/ports
// is never decoded against the wrong message set.

#include "ConnectionTypes.h"
#include "NetworkAdapterList.h"

#include <QDialog>
#include <QList>

namespace Ui
{
class ConfigureConnectionsDialog;
}

class ConfigureConnectionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigureConnectionsDialog(QWidget* parent = 0);
    ~ConfigureConnectionsDialog() override;

    void setConnections(const QList<ConnectionDefinition>& connections);
    QList<ConnectionDefinition> connections() const;

private slots:
    void onAddConnection();
    void onRemoveConnection();
    void onSelectionChanged();
    void onRefreshAdapters();
    void onEditorChanged();
    void onTransportChanged();
    void onAccept();

private:
    void refreshTable();
    void reloadAdapterCombo(const QString& selectAddress);
    void loadEditor(int row);
    void commitEditor();           // pull editor widgets back into the selected row
    int selectedRow() const;
    void updateEditorEnabled();

    Ui::ConfigureConnectionsDialog* ui;
    QList<ConnectionDefinition> m_connections;
    QList<NetworkAdapter> m_adapters;
    bool m_loading;                // guard so loadEditor() doesn't echo back as edits
};

#endif // CONFIGURECONNECTIONSDIALOG_H
