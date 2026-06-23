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

#include "ConnectionTypes.h"
#include "MessageDefinition.h"
#include "SimSetupFile.h"

#include <QByteArray>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStringList>

class DataSender;
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
    void onSenderLinkError(const QString& message);
    void onClearHistoryClicked();
    void onAddMessageClicked();
    void onEditMessageClicked();
    void onRemoveMessageClicked();
    void onImportIcdClicked();
    void onImportMessagesJsonClicked();
    void onExportMessagesJsonClicked();
    void onExportPcapngClicked();
    void onConfigureFieldsButtonClicked();
    void onMessageConnectionsChanged();
    void onMessagesItemChanged(QTableWidgetItem* item);
    void onHistoryDoubleClicked(int row, int column);
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

    // One transmitted packet, kept in lockstep with a history table row so it can
    // be exported to pcapng (item 11) and inspected on double-click (item 14).
    struct SentRecord
    {
        QString timeText;
        QString messageName;
        QString transport;   // "UDP" / "TCP" / "SERIAL"
        QString srcIp;
        quint16 srcPort;
        QString dstIp;
        quint16 dstPort;
        QByteArray payload;
        SentRecord() : srcPort(0), dstPort(0) {}
    };

    void setBarDot(const QString& state);  // "green" / "red" / "gray" on the connection bar
    void refreshConnectionBar();           // count + dot summary on the bar
    // Resolve ALL of a message's send destinations (every ticked connection, in
    // order, de-duplicated; the default/first connection when none are bound).
    // Returns false if no connections exist.
    bool connectionsForMessage(int messageIndex, QList<ConnectionDefinition>& out) const;
    // Open one sender per distinct connection referenced by the plan, health-check
    // each, and store them in m_openSenders. On any failure, closes everything and
    // fills problems.
    bool openSendersForPlan(const QList<ActiveSend>& plan, QStringList& problems);
    void closeAllSenders();
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
    // Build a SentRecord for a transmitted frame against a specific destination
    // connection and queue it for the 200 ms GUI flush. One history line is
    // pushed per destination, so a fanned-out message shows each location.
    void pushPreviewLine(int messageIndex, const QByteArray& payload,
                         const ConnectionDefinition& conn);
    void showProblems(const QString& title, const QStringList& problems);
    SimSetup captureSetup() const;
    void applySetup(const SimSetup& setup);
    void saveSetupToPath(const QString& path, bool silent);
    bool loadSetupFromPath(const QString& path, bool silent);

    QList<MessageDefinition> m_messages;
    // Multi-connection send destinations + the senders open during a send run
    // (keyed by connection id). m_connections is edited via SimConnectionsDialog.
    QList<ConnectionDefinition> m_connections;
    QMap<QString, DataSender*> m_openSenders;
    QList<ActiveSend> m_activeSends;
    bool m_sending;
    bool m_refreshingTable;
    QTimer* m_previewTimer;            // 200 ms GUI flush — never per-packet
    QList<SentRecord> m_historyPending;  // queued sent packets awaiting the flush
    QList<SentRecord> m_sentRecords;     // table-synced records (pcapng + inspector)
    bool m_previewDirty;
    QString m_setupPath;
    quint64 m_totalFramesSent;

    Ui::SimulatorWindow* ui;
};

#endif // SIMULATORWINDOW_H
