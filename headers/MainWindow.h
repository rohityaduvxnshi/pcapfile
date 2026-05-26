#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AppTypes.h"
#include "FilterTypes.h"
#include "LiveUdpReceiver.h"
#include "MessageDefinition.h"
#include "CsvStreamWriter.h"
#include "ProjectFile.h"

#include <QList>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QCloseEvent;
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

private slots:
    void onBrowseClicked();
    void onStartClicked();
    void onFilterCountChanged(int count);
    void onFilterModeChanged();
    void onPortValueChanged(int value);
    void onManageLengthFiltersClicked();
    void onConfigureMessageFieldsClicked();
    void onConfigureHeaderFieldsClicked();
    void onConfigureLiveFieldsClicked();

    // V4 live UDP slots
    void onInputModeChanged();
    void startLiveCapture();
    void stopLiveCapture();
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

private:
    void captureProjectState(ProjectState& state) const;
    void applyProjectState(const ProjectState& state);
    void tryRestoreProjectForPcap(const QString& pcapPath);
    void autoSaveProjectOnClose();

    QList<FieldDefinition> defaultFields() const;
    QString fieldStatusText(const QList<FieldDefinition>& fields) const;
    bool collectFilterConfiguration(FilterConfiguration& config, QString& errorMessage) const;
    QList<MessageDefinition> collectMessageDefinitions() const;
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
    bool configureFieldList(QList<FieldDefinition>& fields, int payloadLengthBytes, const QString& title);
    void clearPortFilterBoxes();
    void clearHeaderFilterBoxes();
    int matchingFilterIndex(const ParsedUdpPacket& parsed, const FilterConfiguration& config) const;
    bool liveHeaderMatches(const QByteArray& payload) const;
    QStringList extractLiveRowValues(const QByteArray& payload, bool& shortPacket) const;

    QString buildPartitionCsvPath(const QString& baseCsvPath,
                                  const QString& modeText,
                                  const QString& filterLabel) const;
    QString buildMessageCsvPath(const QString& outputDirectory,
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

    // V4 live UDP state
    LiveUdpReceiver* m_liveReceiver;
    QTimer* m_livePreviewTimer;
    CsvStreamWriter m_liveWriter;
    QList<FieldDefinition> m_liveFields;
    FilterConfiguration m_liveFilterConfig;
    QVector<QStringList> m_livePreviewRows;
    bool m_liveRunning;
    quint64 m_livePacketsReceived;
    quint64 m_livePacketsMatched;
    quint64 m_liveShortPackets;

    QString m_projectPath;

    // v12: per-row length filters for header mode + global length filters for live mode.
    // When populated, the corresponding mode's export/live-write routes per-message
    // (analogous to port-mode's m_portMessagesByRow path). When empty, the existing
    // single-field-list flow is unchanged.
    QList< QList<MessageDefinition> > m_headerMessagesByRow;
    QList<QPushButton*> m_headerLengthFilterButtons;
    QList<MessageDefinition> m_liveMessages;

    // v12 live mode: per-message writers created at startLiveCapture when m_liveMessages
    // is non-empty. m_activeLiveMessages is a snapshot of m_liveMessages taken at start
    // so changes to the configured list mid-capture do not desync from open writers.
    QList<CsvStreamWriter*> m_liveMessageWriters;
    QList<MessageDefinition> m_activeLiveMessages;
    QList<quint64> m_liveMessageRowCounts;

    void openHeaderLengthFilterDialogForRow(int row);
    void openLiveLengthFilterDialog();
    void refreshHeaderLengthFilterStatus();
    void refreshLiveLengthFilterStatus();
    bool anyHeaderRowHasMessages() const;
    QList<MessageDefinition> collectHeaderModeMessageDefinitions(int commonPort) const;
    bool tryRouteLivePacketByMessage(const QByteArray& payload,
                                     quint16 senderPort,
                                     const QHostAddress& sender,
                                     const QDateTime& arrivalTimeUtc);
    void closeLiveMessageWriters();
    bool startLiveCaptureWithMessages(int bindPort, QString& errorMessage);

    static const int PREVIEW_ROW_LIMIT = 5000;
    static const int LIVE_PREVIEW_ROW_LIMIT = 200;
};

#endif // MAINWINDOW_H
