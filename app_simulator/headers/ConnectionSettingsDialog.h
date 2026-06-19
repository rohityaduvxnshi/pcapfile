#ifndef CONNECTIONSETTINGSDIALOG_H
#define CONNECTIONSETTINGSDIALOG_H

// Pop-out connection manager for the simulator. Owns the destination settings
// (UDP / TCP / Serial) AND the live link (the DataSender), so the connection
// survives closing the pop-out — the main window only shows a compact bar
// (name + status dot + Configure...) and queries this dialog for the active
// sender when streaming. Settings persist via the SimSetup destination fields.

#include <QDialog>
#include <QString>

#include "SimSetupFile.h"

class DataSender;

namespace Ui
{
class ConnectionSettingsDialog;
}

class ConnectionSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionSettingsDialog(QWidget* parent = 0);
    ~ConnectionSettingsDialog();

    bool isConnected() const;
    DataSender* activeSender() const;     // null when not connected
    QString connectionName() const;       // sender description, or "Not connected"
    QString dotState() const;             // "green" / "red" / "gray"

    // Destination settings <-> setup file (the messages live elsewhere).
    void applyDestination(const SimSetup& setup);
    void captureDestination(SimSetup& setup) const;

    void refreshSerialPorts();

protected:
    void showEvent(QShowEvent* event) override;

signals:
    // Emitted whenever the connection state changes (connect / disconnect / a
    // link error dropped the connection). The main window mirrors the bar and,
    // if it was streaming, stops.
    void connectionChanged();

private slots:
    void onDestinationTypeChanged(int index);
    void onRefreshSerialPortsClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onSenderLinkError(const QString& message);

private:
    DataSender* buildSenderFromUi();
    void dropSender();
    void setLinkDot(const QString& state);
    void updateUiState();

    DataSender* m_sender;   // owned; null when disconnected
    QString m_dotState;
    Ui::ConnectionSettingsDialog* ui;
};

#endif // CONNECTIONSETTINGSDIALOG_H
