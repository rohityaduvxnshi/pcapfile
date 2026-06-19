#ifndef DATASENDER_H
#define DATASENDER_H

// Abstract outgoing link for the Universal Data Simulator. Concrete senders:
// UdpDataSender (Ethernet/UDP) and SerialDataSender (COM port). The window
// owns exactly one sender at a time; Connect = open() + send(healthMessage()).
//
// open()/send() report failures through an errorMessage out-parameter that
// already contains both the reason and a suggested fix, ready for a warning
// box. linkError() carries asynchronous failures (e.g. a serial cable pulled
// mid-stream).

#include <QByteArray>
#include <QObject>
#include <QString>

class DataSender : public QObject
{
    Q_OBJECT

public:
    explicit DataSender(QObject* parent = 0);
    virtual ~DataSender();

    virtual bool open(QString& errorMessage) = 0;
    virtual bool send(const QByteArray& payload, QString& errorMessage) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Human description for the status label, e.g. "UDP 192.168.1.50:5000"
    // or "Serial COM5 @ 115200, 8-N-1".
    virtual QString description() const = 0;

    // The demo message Connect transmits to prove the link works.
    static QByteArray healthMessage();

signals:
    void linkError(const QString& message);
};

#endif // DATASENDER_H
