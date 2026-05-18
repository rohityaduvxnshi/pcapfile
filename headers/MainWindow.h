#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AppTypes.h"
#include "FilterTypes.h"
#include "LiveUdpReceiver.h"
#include "CsvStreamWriter.h"

#include <QList>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QCloseEvent;
class QLineEdit;
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
    void onAddFieldClicked();
    void onRemoveFieldClicked();
    void onBitfieldDecoderClicked();
    void onStartClicked();
    void onFilterCountChanged(int count);
    void onFilterModeChanged();

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

private:
    QString tableText(int row, int column) const;
    bool collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const;
    bool collectFilterConfiguration(FilterConfiguration& config, QString& errorMessage) const;

    QStringList buildOutputHeaders(const QList<FieldDefinition>& fields) const;
    QStringList buildPreviewHeaders(const QList<FieldDefinition>& fields) const;
    QStringList buildLiveFieldHeaders(const QList<FieldDefinition>& fields) const;
    void prepareOutputTable(const QStringList& headers);
    void appendPreviewRow(const QStringList& row);

    void rebuildFilterInputs();
    void clearPortFilterBoxes();
    void clearHeaderFilterBoxes();
    int matchingFilterIndex(const ParsedUdpPacket& parsed, const FilterConfiguration& config) const;
    bool liveHeaderMatches(const QByteArray& payload) const;
    QStringList extractLiveRowValues(const QByteArray& payload, bool& shortPacket) const;

    QString buildPartitionCsvPath(const QString& baseCsvPath,
                                  const QString& modeText,
                                  const QString& filterLabel) const;

    void setBusy(bool busy);
    void setStatus(const QString& message);
    void setLiveUiState(bool running);

    Ui::MainWindow* ui;
    QList<QSpinBox*> m_portFilterBoxes;
    QList<QLineEdit*> m_headerFilterBoxes;

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

    static const int PREVIEW_ROW_LIMIT = 5000;
    static const int LIVE_PREVIEW_ROW_LIMIT = 200;
};

#endif // MAINWINDOW_H
