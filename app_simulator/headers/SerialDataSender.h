#ifndef SERIALDATASENDER_H
#define SERIALDATASENDER_H

#include "DataSender.h"

#include <QSerialPort>

// Serial COM-port sender. The QSerialPort object is recreated on every
// open() and destroyed on close() so a previous failure can never leave a
// sticky error state behind (same lifecycle the old SerialPortReceiver used).
class SerialDataSender : public DataSender
{
    Q_OBJECT

public:
    explicit SerialDataSender(QObject* parent = 0);
    ~SerialDataSender();

    // Set before open(). dataBits 5..8; parity "None"/"Even"/"Odd"/"Mark"/
    // "Space"; stopBits "1"/"1.5"/"2" (all case-insensitive).
    void configure(const QString& portName,
                   int baudRate,
                   int dataBits,
                   const QString& parity,
                   const QString& stopBits);

    bool open(QString& errorMessage) override;
    bool send(const QByteArray& payload, QString& errorMessage) override;
    void close() override;
    bool isOpen() const override;
    QString description() const override;

    // Block until the queued bytes were handed to the driver, or msecs
    // elapsed. Only used by the Connect health check — never while
    // streaming, so the GUI thread is never blocked during a send run.
    bool waitForSent(int msecs, QString& errorMessage);

private slots:
    void onPortErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort* m_port;
    QString m_portName;
    int m_baudRate;
    int m_dataBits;
    QString m_parity;
    QString m_stopBits;
};

#endif // SERIALDATASENDER_H
