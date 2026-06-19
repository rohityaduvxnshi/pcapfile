#ifndef LIVETCPRECEIVER_H
#define LIVETCPRECEIVER_H

// Live TCP capture for the reader. TCP is a byte stream with no message
// boundaries, so the stream is sliced into fixed-size frames of `frameLength`
// bytes (the configured message/payload length); leftover bytes buffer until
// the next full frame arrives. Two roles:
//   Listen  - act as a TCP server (bind a port; a client such as the simulator
//             connects in). Mirrors the UDP Live mode.
//   Connect - act as a TCP client (dial an external TCP server/device).
// Emits the SAME datagramReceived() signal as LiveUdpReceiver so the existing
// MainWindow live routing handles each frame as if it were a datagram.

#include <QByteArray>
#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

class LiveTcpReceiver : public QObject
{
    Q_OBJECT
public:
    enum Role { Listen, Connect };

    explicit LiveTcpReceiver(QObject* parent = nullptr);
    ~LiveTcpReceiver() override;

    // frameLength > 0: slice the stream into fixed-size frames; <= 0: emit each
    // received chunk as one frame. host is only used for Role::Connect.
    bool start(Role role, const QString& host, quint16 port, int frameLength, QString& errorOut);
    void stop();
    bool isRunning() const;

signals:
    void datagramReceived(const QByteArray& payload,
                          const QHostAddress& sender,
                          quint16 senderPort,
                          const QDateTime& arrivalTimeUtc);
    void socketError(const QString& message);
    void peerChanged(const QString& description);   // status text (connected/listening)

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void attachClient(QTcpSocket* sock);

    QTcpServer* m_server;
    QTcpSocket* m_socket;
    QByteArray  m_buffer;
    int         m_frameLength;
    bool        m_running;
    Role        m_role;
};

#endif // LIVETCPRECEIVER_H
