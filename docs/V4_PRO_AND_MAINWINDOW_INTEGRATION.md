# V4 — .pro and MainWindow Integration

The four new files (`LiveUdpReceiver.h/.cpp`, `CsvStreamWriter.h/.cpp`) are
complete and standalone. The `.pro` file and `MainWindow` are existing files.
The `.pro` file has already been updated to include `QT += network` and the
new V4 files.

The `MainWindow` integration still has project-specific points that must be
wired to the current extraction, filtering, preview-table, and bitfield logic.
Those points are marked `>>> ADAPT <<<` below.

---

## 1. PcapUdpExtractor.pro

Required entries:

```qmake
QT += core gui widgets network

HEADERS += headers/LiveUdpReceiver.h \
           headers/CsvStreamWriter.h

SOURCES += sources/LiveUdpReceiver.cpp \
           sources/CsvStreamWriter.cpp
```

After this change: Clean -> Run qmake -> Rebuild. Adding a Qt module requires
re-running qmake.

---

## 2. MainWindow.ui

Add without removing existing widgets:

```text
Input mode selector:
  QRadioButton radioFileMode      text: File Mode
  QRadioButton radioLiveMode      text: Live Mode

Live Mode group box:
  QGroupBox    groupLiveMode
  QSpinBox     spinLivePort       range: 1..65535, default: 5005
  QPushButton  btnStartLiveCapture
  QPushButton  btnStopLiveCapture
  QLabel       lblLiveStatus
  QLabel       lblPacketsReceived
  QLabel       lblPacketsMatched
  QLabel       lblRowsWritten
  QLabel       lblShortPackets
  QLabel       lblLastLiveError
```

Keep all existing file-mode controls. File Mode and Live Mode must not run at
the same time.

---

## 3. MainWindow.h additions

```cpp
#include "LiveUdpReceiver.h"
#include "CsvStreamWriter.h"

#include <QTimer>
#include <QStringList>
#include <QVector>

private slots:
    void onInputModeChanged();
    void startLiveCapture();
    void stopLiveCapture();
    void onLiveDatagramReceived(const QByteArray &payload,
                                const QHostAddress &sender,
                                quint16 senderPort,
                                const QDateTime &arrivalTimeUtc);
    void onLiveSocketError(const QString &message);
    void refreshLivePreview();

private:
    void setLiveControlsEnabled(bool capturing);
    QStringList buildLiveCsvHeaders() const;
    QStringList extractLiveRowValues(const QByteArray &payload, bool &shortPacket);
    bool liveHeaderMatches(const QByteArray &payload) const;

    LiveUdpReceiver *m_liveReceiver = nullptr;
    CsvStreamWriter m_liveWriter;
    QTimer *m_livePreviewTimer = nullptr;

    bool m_liveRunning = false;
    qint64 m_packetsReceived = 0;
    qint64 m_packetsMatched = 0;
    qint64 m_shortPackets = 0;

    QVector<QStringList> m_livePreviewRows;
```

If your existing `MainWindow.h` already has equivalent members or naming style,
adapt the names but keep the behavior.

---

## 4. Constructor wiring

```cpp
m_liveReceiver = new LiveUdpReceiver(this);
m_livePreviewTimer = new QTimer(this);
m_livePreviewTimer->setInterval(250);

connect(ui->radioFileMode, &QRadioButton::toggled,
        this, &MainWindow::onInputModeChanged);
connect(ui->radioLiveMode, &QRadioButton::toggled,
        this, &MainWindow::onInputModeChanged);
connect(ui->btnStartLiveCapture, &QPushButton::clicked,
        this, &MainWindow::startLiveCapture);
connect(ui->btnStopLiveCapture, &QPushButton::clicked,
        this, &MainWindow::stopLiveCapture);
connect(m_liveReceiver, &LiveUdpReceiver::datagramReceived,
        this, &MainWindow::onLiveDatagramReceived);
connect(m_liveReceiver, &LiveUdpReceiver::socketError,
        this, &MainWindow::onLiveSocketError);
connect(m_livePreviewTimer, &QTimer::timeout,
        this, &MainWindow::refreshLivePreview);

ui->btnStopLiveCapture->setEnabled(false);
ui->groupLiveMode->setVisible(false);
```

---

## 5. Mode switching

```cpp
void MainWindow::onInputModeChanged()
{
    const bool live = ui->radioLiveMode->isChecked();

    ui->groupLiveMode->setVisible(live);

    // >>> ADAPT <<< hide or disable only the file-upload/file-export controls.
    // Do not hide the field table, filter config, or Bitfield Decoder config;
    // those are shared by both modes.
}
```

---

## 6. Start live capture

```cpp
void MainWindow::startLiveCapture()
{
    if (m_liveRunning)
        return;

    const int port = ui->spinLivePort->value();
    if (port < 1 || port > 65535) {
        QMessageBox::warning(this, tr("Live Capture"), tr("Port must be in range 1..65535."));
        return;
    }

    if (port < 1024) {
        QMessageBox::information(this, tr("Live Capture"),
            tr("Ports below 1024 may require administrator/root privileges."));
    }

    // >>> ADAPT <<< validate at least one field is defined using the same rule as file mode.
    // If not valid, show message and return.

    const QStringList headers = buildLiveCsvHeaders();
    if (headers.isEmpty()) {
        QMessageBox::warning(this, tr("Live Capture"), tr("No output fields are defined."));
        return;
    }

    // >>> ADAPT <<< choose the same writable output folder used by file mode.
    const QString outputDir = QDir::currentPath();
    const QString fileName = QStringLiteral("liveCapture_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QDir(outputDir).filePath(fileName);

    QString err;
    if (!m_liveWriter.open(path, headers, true, err)) {
        QMessageBox::critical(this, tr("Live Capture"), tr("Could not open live CSV:\n%1").arg(err));
        return;
    }

    if (!m_liveReceiver->start(static_cast<quint16>(port), err)) {
        m_liveWriter.close();
        QMessageBox::critical(this, tr("Live Capture"), tr("Could not bind UDP socket:\n%1").arg(err));
        return;
    }

    m_packetsReceived = 0;
    m_packetsMatched = 0;
    m_shortPackets = 0;
    m_livePreviewRows.clear();
    m_liveRunning = true;

    setLiveControlsEnabled(true);
    ui->lblLiveStatus->setText(tr("Listening"));
    m_livePreviewTimer->start();
}
```

---

## 7. Datagram processing

```cpp
void MainWindow::onLiveDatagramReceived(const QByteArray &payload,
                                        const QHostAddress &sender,
                                        quint16 senderPort,
                                        const QDateTime &arrivalTimeUtc)
{
    ++m_packetsReceived;

    if (!liveHeaderMatches(payload))
        return;

    ++m_packetsMatched;

    bool shortPacket = false;
    const QStringList values = extractLiveRowValues(payload, shortPacket);
    if (shortPacket)
        ++m_shortPackets;

    QString err;
    if (!m_liveWriter.writeRow(arrivalTimeUtc,
                               sender.toString(),
                               senderPort,
                               values,
                               err)) {
        onLiveSocketError(tr("CSV write failed: %1").arg(err));
        return;
    }

    QStringList previewRow;
    previewRow << arrivalTimeUtc.toUTC().toString(Qt::ISODateWithMs)
               << sender.toString()
               << QString::number(senderPort);
    previewRow += values;

    m_livePreviewRows.append(previewRow);
    while (m_livePreviewRows.size() > 200)
        m_livePreviewRows.removeFirst();
}
```

Do not update the preview table inside this function. Only update the rolling
buffer. The timer handles table repaint.

---

## 8. Stop live capture

```cpp
void MainWindow::stopLiveCapture()
{
    if (!m_liveRunning)
        return;

    m_livePreviewTimer->stop();
    m_liveReceiver->stop();

    const QString savedPath = m_liveWriter.filePath();
    const qint64 rows = m_liveWriter.rowsWritten();

    QString err;
    const bool flushed = m_liveWriter.flush(err);
    m_liveWriter.close();

    m_liveRunning = false;
    setLiveControlsEnabled(false);
    ui->lblLiveStatus->setText(tr("Stopped"));

    if (!flushed) {
        QMessageBox::warning(this, tr("Live Capture"),
            tr("Capture stopped, but final CSV flush failed:\n%1\n\nFile:\n%2\nRows written before close: %3")
            .arg(err).arg(savedPath).arg(rows));
    } else {
        QMessageBox::information(this, tr("Live Capture"),
            tr("Saved:\n%1\nRows written: %2").arg(savedPath).arg(rows));
    }
}
```

---

## 9. Socket/write error handler

```cpp
void MainWindow::onLiveSocketError(const QString &message)
{
    ui->lblLastLiveError->setText(message);
    if (m_liveRunning) {
        QMessageBox::critical(this, tr("Live Capture"), message);
        stopLiveCapture();
    }
}
```

---

## 10. Preview refresh

```cpp
void MainWindow::refreshLivePreview()
{
    ui->lblPacketsReceived->setText(QString::number(m_packetsReceived));
    ui->lblPacketsMatched->setText(QString::number(m_packetsMatched));
    ui->lblRowsWritten->setText(QString::number(m_liveWriter.rowsWritten()));
    ui->lblShortPackets->setText(QString::number(m_shortPackets));

    // >>> ADAPT <<< repaint the preview table here using m_livePreviewRows.
    // Preview columns must match:
    // TimestampUtc, SourceIP, SourcePort + buildLiveCsvHeaders()
}
```

---

## 11. Required helper behavior

### buildLiveCsvHeaders()

Must return the same field columns file mode produces, including Bitfield
Decoder expansion columns. `CsvStreamWriter` prepends `TimestampUtc, SourceIP,
SourcePort` itself.

```cpp
QStringList MainWindow::buildLiveCsvHeaders() const
{
    // >>> ADAPT <<< reuse the exact header-building logic from file mode.
    QStringList headers;
    return headers;
}
```

### extractLiveRowValues()

Must call the same extraction and bitfield-decoding path used by file mode, but
on the live UDP payload directly.

Required short-packet behavior:

```text
If offset + length > payload.size():
- write SHORT_PACKET for the affected field
- set shortPacket = true
- continue with other fields where possible
```

The returned QStringList must contain exactly the same number of values as
`buildLiveCsvHeaders()`. `CsvStreamWriter` rejects mismatched rows.

### liveHeaderMatches()

Must apply the existing header-filter logic to the first 0-4 bytes of the UDP
payload. Do not use `UdpPacketParser` here.

---

## 12. closeEvent

```cpp
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_liveRunning)
        stopLiveCapture();
    QMainWindow::closeEvent(event);
}
```

---

## 13. Required includes in MainWindow.cpp

```cpp
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDateTime>
```

---

## 14. Summary

Safe: the four new files are isolated and do not touch existing modules.
`CsvStreamWriter` is separate from `CsvExporter`, so batch export is untouched.
`UdpPacketParser` and `PcapFileReader` are not modified.

Verify every `>>> ADAPT <<<` point before calling V4 complete: field-count
check, output directory, header-match function, file-mode extraction reuse,
preview repaint, UI lock/unlock, and header builder.
