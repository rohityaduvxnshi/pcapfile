#ifndef TCPDATASENDER_H
#define TCPDATASENDER_H

#include "DataSender.h"

#include <QAbstractSocket>

class QTcpSocket;

// TCP client sender: connects out to a host:port and writes message payloads
// over the stream. The socket is recreated on every open() (like
// SerialDataSender) so a previous failure never leaves a sticky state. Async
// errors / a server-side disconnect are forwarded through linkError() so the
// link dot can turn red mid-stream.
class TcpDataSender : public DataSender
{
    Q_OBJECT

public:
    explicit TcpDataSender(QObject* parent = 0);
    ~TcpDataSender();

    // Set before open(). host may be an IPv4 address or a hostname.
    void setDestination(const QString& host, int port);

    bool open(QString& errorMessage) override;
    bool send(const QByteArray& payload, QString& errorMessage) override;
    void close() override;
    bool isOpen() const override;
    QString description() const override;

private slots:
    void onSocketError(QAbstractSocket::SocketError error);
    void onDisconnected();

private:
    QTcpSocket* m_socket;
    QString m_host;
    quint16 m_port;
    bool m_open;
    bool m_closing;   // suppress linkError() during our own teardown
};

#endif // TCPDATASENDER_H
