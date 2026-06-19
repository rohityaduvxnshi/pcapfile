#include "TcpDataSender.h"

#include <QTcpSocket>

namespace
{
const int kConnectTimeoutMs = 3000;
}

TcpDataSender::TcpDataSender(QObject* parent)
    : DataSender(parent)
    , m_socket(0)
    , m_port(0)
    , m_open(false)
    , m_closing(false)
{
}

TcpDataSender::~TcpDataSender()
{
    close();
}

void TcpDataSender::setDestination(const QString& host, int port)
{
    m_host = host.trimmed();
    m_port = static_cast<quint16>(port);
}

bool TcpDataSender::open(QString& errorMessage)
{
    close();

    if (m_host.isEmpty())
    {
        errorMessage = "No destination host/IP has been entered. "
                       "Solution: type the TCP server's IPv4 address (e.g. 192.168.1.50) or hostname.";
        return false;
    }
    if (m_port == 0)
    {
        errorMessage = "The destination TCP port is 0. Solution: pick the port the server listens on (1-65535).";
        return false;
    }

    m_closing = false;
    m_socket = new QTcpSocket(this);
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

    m_socket->connectToHost(m_host, m_port);
    if (!m_socket->waitForConnected(kConnectTimeoutMs))
    {
        errorMessage = QString("Could not connect to %1:%2 — %3. "
                               "Solution: confirm a TCP server is listening there (e.g. the reader in TCP "
                               "Listen mode), the IP/port are correct, and no firewall blocks the connection.")
                           .arg(m_host).arg(m_port).arg(m_socket->errorString());
        close();
        return false;
    }

    m_open = true;
    return true;
}

bool TcpDataSender::send(const QByteArray& payload, QString& errorMessage)
{
    if (!m_open || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
    {
        errorMessage = "The TCP link is not connected. Solution: press Connect first (a server must be listening).";
        return false;
    }

    const qint64 written = m_socket->write(payload);
    if (written != payload.size())
    {
        errorMessage = QString("Sending to %1:%2 failed: %3. "
                               "Solution: check the connection is still up (the server may have closed it) "
                               "and press Connect again.")
                           .arg(m_host).arg(m_port).arg(m_socket->errorString());
        return false;
    }
    return true;
}

void TcpDataSender::close()
{
    m_closing = true;
    if (m_socket)
    {
        m_socket->disconnect(this);   // stop our slots firing during teardown
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort();
        m_socket->deleteLater();
        m_socket = 0;
    }
    m_open = false;
}

bool TcpDataSender::isOpen() const
{
    return m_open && m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString TcpDataSender::description() const
{
    return QString("TCP %1:%2").arg(m_host).arg(m_port);
}

void TcpDataSender::onSocketError(QAbstractSocket::SocketError)
{
    if (m_closing || !m_socket)
        return;
    emit linkError(QString("TCP link error: %1. "
                           "Solution: check the server is still listening and the network is up, then Connect again.")
                       .arg(m_socket->errorString()));
}

void TcpDataSender::onDisconnected()
{
    if (m_closing)
        return;
    m_open = false;
    emit linkError("The TCP connection was closed by the remote end. "
                   "Solution: restart the server/listener and press Connect again.");
}
