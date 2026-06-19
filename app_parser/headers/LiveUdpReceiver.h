#ifndef LIVEUDPRECEIVER_H
#define LIVEUDPRECEIVER_H

// V4 - Live UDP Capture
// Owns a single QUdpSocket bound to one UDP destination port.
// QUdpSocket::readDatagram() returns the UDP PAYLOAD directly,
// because the OS already strips Ethernet + IPv4 + UDP headers.
// Therefore live datagrams must NOT be passed into UdpPacketParser.

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QByteArray>
#include <QString>

class LiveUdpReceiver : public QObject
{
    Q_OBJECT
public:
    explicit LiveUdpReceiver(QObject *parent = nullptr);
    ~LiveUdpReceiver() override;

    // Optional multicast group to join after binding (e.g. "239.1.5.6"). Empty =
    // plain unicast (unchanged behaviour). Set before start().
    void setMulticastGroup(const QString &group);

    // Bind to the given port. Returns false on failure and fills errorOut.
    bool start(quint16 port, QString &errorOut);

    // Close and delete the socket safely. Safe to call when not running.
    void stop();

    bool isRunning() const;

signals:
    // Emitted once per received UDP datagram. payload is the UDP payload only.
    void datagramReceived(const QByteArray &payload,
                          const QHostAddress &sender,
                          quint16 senderPort,
                          const QDateTime &arrivalTimeUtc);

    // Emitted on a bind-time or run-time socket error.
    void socketError(const QString &message);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket = nullptr;
    bool m_running = false;
    QString m_multicastGroup;
};

#endif // LIVEUDPRECEIVER_H
