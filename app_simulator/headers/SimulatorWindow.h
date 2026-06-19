#ifndef SIMULATORWINDOW_H
#define SIMULATORWINDOW_H

// Main window of the Universal Data Simulator.
//
// Flow: pick a destination (UDP or serial) -> Connect (opens the link and
// transmits a health-check message; green/red/gray dot shows the state) ->
// define messages and their fields/values -> tick the Send? box of the
// messages to stream -> Send verifies EVERYTHING first (all problems in one
// dialog, each with a solution) and then re-sends every ticked message at
// its own rate until Stop. The bottom box shows the last 5 transmitted
// payloads in hex.

#include "MessageDefinition.h"
#include "SimSetupFile.h"

#include <QByteArray>
#include <QList>
#include <QMainWindow>
#include <QStringList>

class ConnectionSettingsDialog;
class QCloseEvent;
class QTableWidgetItem;
class QTimer;

namespace Ui {
class SimulatorWindow;
}

class SimulatorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SimulatorWindow(QWidget* parent = 0);
    ~SimulatorWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onConfigureConnectionClicked();
    void onConnectionChanged();
    void onClearHistoryClicked();
    void onAddMessageClicked();
    void onEditMessageClicked();
    void onRemoveMessageClicked();
    void onImportIcdClicked();
    void onConfigureFieldsButtonClicked();
    void onMessagesItemChanged(QTableWidgetItem* item);
    void onStartSendingClicked();
    void onStopSendingClicked();
    void onSendTimerTick();
    void onPreviewFlushTick();
    void onOpenSetupClicked();
    void onSaveSetupClicked();
    void onSaveSetupAsClicked();
    void onToggleThemeClicked();
    void onShowShortcutsHelp();
    void onShowUserManual();

private:
    struct ActiveSend
    {
        int messageIndex;     // index into m_messages
        QByteArray payload;   // pre-built, frozen while streaming
        QTimer* timer;
        quint64 count;
        ActiveSend() : messageIndex(-1), timer(0), count(0) {}
    };

    void setBarDot(const QString& state);  // "green" / "red" / "gray" on the connection bar
    void refreshMessagesTable();
    int selectedMessageRow() const;
    bool messageNameInUse(const QString& name, int ignoreIndex) const;
    bool buildOneMessage(int messageIndex, QByteArray& payload, QStringList& problems);
    bool verifyBeforeSend(QList<ActiveSend>& plan, QStringList& problems);
    bool sendActive(int planIndex);
    int activeIndexForMessage(int messageIndex) const;
    // Live edit: rebuild (or start/stop) a message's stream while sending,
    // without interrupting the others.
    void rebuildActiveSend(int messageIndex);
    void stopAllSendTimers();
    void setSendingUiState(bool sending);
    void pushPreviewLine(const QString& messageName, const QByteArray& payload);
    void showProblems(const QString& title, const QStringList& problems);
    SimSetup captureSetup() const;
    void applySetup(const SimSetup& setup);
    void saveSetupToPath(const QString& path, bool silent);
    bool loadSetupFromPath(const QString& path, bool silent);

    QList<MessageDefinition> m_messages;
    ConnectionSettingsDialog* m_connDialog;   // pop-out; owns the link/sender
    QList<ActiveSend> m_activeSends;
    bool m_sending;
    bool m_refreshingTable;
    QTimer* m_previewTimer;            // 200 ms GUI flush — never per-packet
    QList<QStringList> m_historyPending; // queued history rows [time,name,bytes,hex]
    bool m_previewDirty;
    QString m_setupPath;
    quint64 m_totalFramesSent;

    Ui::SimulatorWindow* ui;
};

#endif // SIMULATORWINDOW_H
