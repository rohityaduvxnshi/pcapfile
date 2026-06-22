#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AppTypes.h"
#include "CompareOptionsEngine.h"
#include "ConnectionTypes.h"
#include "FilterTypes.h"
#include "LiveTcpReceiver.h"
#include "LiveUdpReceiver.h"
#include "MessageDefinition.h"
#include "ExcelStreamWriter.h"
#include "ProjectFile.h"

#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStringList>
#include <QVector>

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onBrowseClicked();
    void onStartClicked();
    void onFilterCountChanged(int count);
    void onFilterModeChanged();
    void onPortValueChanged(int value);
    void onManageLengthFiltersClicked();
    void onConfigureMessageFieldsClicked();
    void onConfigureHeaderFieldsClicked();

    // V4 live UDP slots
    void onInputModeChanged();
    void startLiveCapture();
    void stopLiveCapture();
    void onLiveTransportChanged();
    void onLiveDatagramReceived(const QByteArray& payload,
                                const QHostAddress& sender,
                                quint16 senderPort,
                                const QDateTime& arrivalTimeUtc);
    void onLiveSocketError(const QString& message);
    void refreshLivePreview();

    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();

    // v12: theme + length-filter routing slots
    void onToggleThemeClicked();
    void onManageHeaderLengthFiltersClicked();
    void onManageLiveLengthFiltersClicked();

    // Multi-connection live capture: open the connection manager.
    void onConfigureConnectionsClicked();

    // v13: per-row Configure Fields slot for the live configured-messages table.
    void onConfigureLiveMessageFieldsClicked();

    // ICD import: File menu -> Import ICD (.docx). Reads a Word ICD, opens the
    // review/selection dialog, and routes the chosen messages into the active mode.
    void onImportIcdClicked();

    // Shared suite JSON: export the current mode's messages / import messages
    // (fields + bits + compare options) from a JSON file the simulator can read.
    void onImportMessagesJsonClicked();
    void onExportMessagesJsonClicked();

    // Keyboard shortcuts (see Help > Keyboard Shortcuts / F1).
    void onSelectFileMode();
    void onSelectLiveMode();
    void onShortcutStart();
    void onShortcutStop();
    void onShowShortcutsHelp();
    void onShowUserManual();

private:
    void captureProjectState(ProjectState& state) const;
    void applyProjectState(const ProjectState& state);
    void tryRestoreProjectForPcap(const QString& pcapPath);
    void autoSaveProjectOnClose();
    void loadProjectFromPath(const QString& path);

    QList<FieldDefinition> defaultFields() const;
    QString fieldStatusText(const QList<FieldDefinition>& fields) const;
    bool collectFilterConfiguration(FilterConfiguration& config, QString& errorMessage) const;
    QList<MessageDefinition> collectMessageDefinitions() const;
    // The active mode's messages (live / header / port) for JSON export.
    QList<MessageDefinition> collectMessagesForJsonExport() const;
    bool validateMessageDefinitions(const QList<MessageDefinition>& messages, QString& errorMessage) const;
    bool validateMessagesExistInCapture(const QList<MessageDefinition>& messages, QString& errorMessage);
    bool exportByMessageDefinitions(const QList<MessageDefinition>& messages, QString& errorMessage);

    QStringList buildOutputHeaders(const QList<FieldDefinition>& fields) const;
    QStringList buildPreviewHeaders(const QList<FieldDefinition>& fields) const;
    QStringList buildLiveFieldHeaders(const QList<FieldDefinition>& fields) const;
    QStringList buildPortMessagePreviewHeaders() const;
    void prepareOutputTable(const QStringList& headers);
    void appendPreviewRow(const QStringList& row);

    void rebuildFilterInputs();
    void refreshPortFilterTable();
    void refreshConfiguredMessagesTable();
    void openLengthFilterDialogForPortRow(int row);
    void openFieldConfigurationForMessage(int messageIndex);
    // offsetUnit (optional) carries the message's BYTES/WORDS field-offset
    // display unit in and back out; null (e.g. header fields) keeps it in bytes.
    bool configureFieldList(QList<FieldDefinition>& fields, int payloadLengthBytes, const QString& title,
                            QString* offsetUnit = 0);
    void clearPortFilterBoxes();
    void clearHeaderFilterBoxes();
    int matchingFilterIndex(const ParsedUdpPacket& parsed, const FilterConfiguration& config) const;

    QString buildPartitionExportPath(const QString& baseExportPath,
                                     const QString& modeText,
                                     const QString& filterLabel) const;
    QString buildMessageExportPath(const QString& outputDirectory,
                                   const MessageDefinition& message,
                                   const QString& timestampText) const;

    void setBusy(bool busy);
    void setStatus(const QString& message);
    void setLiveUiState(bool running);
    void refreshStandaloneFieldStatus();

    Ui::MainWindow* ui;
    QList<QSpinBox*> m_portFilterBoxes;
    QList<QLineEdit*> m_headerFilterBoxes;
    QList< QList<MessageDefinition> > m_portMessagesByRow;
    QList<FieldDefinition> m_headerFields;

    // V4 live UDP state.
    LiveUdpReceiver* m_liveReceiver;
    LiveTcpReceiver* m_liveTcpReceiver;
    bool m_liveTransportTcp;   // which receiver the active session uses

    // Multi-connection live capture. m_liveConnections is the user-defined list
    // (empty = legacy single Transport/Port path using m_liveReceiver/m_liveTcpReceiver).
    // When non-empty, startLiveCaptureWithMessages spins up one receiver per
    // connection into m_liveSessionReceivers; m_receiverConnectionId maps each
    // receiver object back to the connection id so a datagram is only routed
    // against messages bound to that connection.
    QList<ConnectionDefinition> m_liveConnections;
    QList<QObject*> m_liveSessionReceivers;
    QMap<QObject*, QString> m_receiverConnectionId;
    QTimer* m_livePreviewTimer;
    QVector<QStringList> m_livePreviewRows;
    bool m_liveRunning;
    quint64 m_livePacketsReceived;
    quint64 m_livePacketsMatched;
    quint64 m_liveShortPackets;

    QString m_projectPath;

    // v12: per-row length filters for header mode + global length filters for live mode.
    // The export path routes per-message (analogous to port-mode's m_portMessagesByRow).
    QList< QList<MessageDefinition> > m_headerMessagesByRow;
    QList<QPushButton*> m_headerLengthFilterButtons;
    QList<MessageDefinition> m_liveMessages;

    // v12 live mode: per-message writers created at startLiveCapture. m_activeLiveMessages
    // snapshots m_liveMessages at start so changes mid-capture don't desync open writers.
    QList<ExcelStreamWriter*> m_liveMessageWriters;
    QList<MessageDefinition> m_activeLiveMessages;
    QList<quint64> m_liveMessageRowCounts;

    // v13: per-message refresh-rate trackers for live mode. Parallel to m_activeLiveMessages.
    QList<RefreshRateTracker> m_liveCompareTrackers;

    void openHeaderLengthFilterDialogForRow(int row);
    void openLiveLengthFilterDialog();
    void refreshHeaderLengthFilterStatus();
    void refreshLiveLengthFilterStatus();
    bool anyHeaderRowHasMessages() const;
    QList<MessageDefinition> collectHeaderModeMessageDefinitions(int commonPort) const;
    bool tryRouteLivePacketByMessage(const QByteArray& payload,
                                     quint16 senderPort,
                                     const QHostAddress& sender,
                                     const QDateTime& arrivalTimeUtc,
                                     const QString& connectionId);
    void closeLiveMessageWriters();
    bool startLiveCaptureWithMessages(int bindPort, QString& errorMessage);

    // Multi-connection helpers.
    void refreshLiveConnectionSummary();
    // Start one receiver per defined connection (m_liveConnections). Fills
    // errorMessage and tears down any partially-started receivers on failure.
    bool startSessionReceivers(QString& errorMessage);
    void stopSessionReceivers();
    // Frame length for a TCP connection: a single bound message's length gives
    // exact framing, otherwise 0 (per-chunk). Considers messages bound to connId
    // plus unbound messages (which match any connection).
    int frameLengthForConnection(const QString& connId) const;

    // v13: render m_liveMessages into the new tblLiveConfiguredMessages widget.
    void refreshLiveConfiguredMessagesTable();

    // ICD import: append imported messages into the active mode's message list
    // (port row / header row 0 / live) and refresh the relevant table.
    void applyImportedMessages(const QList<MessageDefinition>& messages);

    static const int PREVIEW_ROW_LIMIT = 5000;
    static const int LIVE_PREVIEW_ROW_LIMIT = 200;
};

#endif // MAINWINDOW_H
