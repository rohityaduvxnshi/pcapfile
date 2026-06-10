#ifndef SERIALPORTRECEIVER_H
#define SERIALPORTRECEIVER_H

// Serial Mode — owns a single QSerialPort and frames the incoming byte stream
// into LINES (terminated by \n, with \r trimmed). Serial links have no datagram
// boundaries, so one line = one message record. A line is either:
//   - an ASCII NMEA sentence ("$GPGGA,...*hh"), or
//   - hex text of one binary message ("AA 55 01 02 ..." / "AA550102..."),
// and MainWindow::serialLineToPayload() converts it to the payload bytes that
// flow through the same message matching / extraction as live UDP.
//
// Modeled on LiveUdpReceiver: start/stop lifecycle, an error signal, and one
// signal per framed record.

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>

class QSerialPort;

class SerialPortReceiver : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortReceiver(QObject *parent = nullptr);
    ~SerialPortReceiver() override;

    // Port settings, set before start(). Baud is a plain int (e.g. 115200);
    // dataBits 5..8; parity "None"/"Even"/"Odd"; stopBits 1 or 2.
    void configure(const QString &portName, int baudRate,
                   int dataBits, const QString &parity, int stopBits);

    bool start(QString &errorOut);
    void stop();
    bool isRunning() const;

    quint64 bytesReceived() const;

signals:
    // One complete line, CR/LF stripped, with its arrival time.
    void lineReceived(const QByteArray &line, const QDateTime &arrivalTimeUtc);

    // Open-time or run-time port error.
    void portError(const QString &message);

private slots:
    void onReadyRead();

private:
    QSerialPort *m_port = nullptr;
    QByteArray m_buffer;
    QString m_portName;
    int m_baudRate = 115200;
    int m_dataBits = 8;
    QString m_parity = QStringLiteral("None");
    int m_stopBits = 1;
    bool m_running = false;
    quint64 m_bytesReceived = 0;
};

#endif // SERIALPORTRECEIVER_H
