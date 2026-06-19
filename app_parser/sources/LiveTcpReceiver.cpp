#include "LiveTcpReceiver.h"

#include <QTcpServer>
#include <QTcpSocket>

namespace
{
const int kConnectTimeoutMs = 3000;
}

LiveTcpReceiver::LiveTcpReceiver(QObject* parent)
    : QObject(parent),
      m_server(nullptr),
      m_socket(nullptr),
      m_frameLength(0),
      m_running(false),
      m_role(Listen)
{
}

LiveTcpReceiver::~LiveTcpReceiver()
{
    stop();
}

bool LiveTcpReceiver::start(Role role, const QString& host, quint16 port, int frameLength, QString& errorOut)
{
    errorOut.clear();
    if (m_running || m_server || m_socket)
        stop();

    m_role = role;
    m_frameLength = frameLength;
    m_buffer.clear();

    if (port == 0)
    {
        errorOut = "The TCP port is 0. Solution: pick a port (1-65535).";
        return false;
    }

    if (role == Listen)
    {
        m_server = new QTcpServer(this);
        if (!m_server->listen(QHostAddress::AnyIPv4, port))
        {
            errorOut = QString("Could not listen on TCP port %1: %2. "
                               "Solution: pick a free port (ports below 1024 may need admin rights).")
                           .arg(port).arg(m_server->errorString());
            delete m_server;
            m_server = nullptr;
            return false;
        }
        connect(m_server, &QTcpServer::newConnection, this, &LiveTcpReceiver::onNewConnection);
        m_running = true;
        emit peerChanged(QString("TCP listening on port %1").arg(port));
        return true;
    }

    // Role::Connect — dial an external TCP server.
    if (host.trimmed().isEmpty())
    {
        errorOut = "No TCP host/IP entered. Solution: type the TCP server's IP address or hostname.";
        return false;
    }
    QTcpSocket* sock = new QTcpSocket(this);
    sock->connectToHost(host.trimmed(), port);
    if (!sock->waitForConnected(kConnectTimeoutMs))
    {
        errorOut = QString("Could not connect to %1:%2 — %3. "
                           "Solution: confirm a TCP server is listening there and the IP/port/firewall are correct.")
                       .arg(host.trimmed()).arg(port).arg(sock->errorString());
        sock->abort();
        sock->deleteLater();
        return false;
    }
    attachClient(sock);
    m_running = true;
    emit peerChanged(QString("TCP connected to %1:%2").arg(host.trimmed()).arg(port));
    return true;
}

void LiveTcpReceiver::onNewConnection()
{
    if (!m_server)
        return;
    QTcpSocket* sock = m_server->nextPendingConnection();
    if (!sock)
        return;
    if (m_socket)
    {
        // One client at a time in v1; politely refuse extras.
        sock->disconnectFromHost();
        sock->deleteLater();
        return;
    }
    attachClient(sock);
    emit peerChanged(QString("TCP client connected: %1:%2")
                         .arg(sock->peerAddress().toString()).arg(sock->peerPort()));
}

void LiveTcpReceiver::attachClient(QTcpSocket* sock)
{
    m_socket = sock;
    m_buffer.clear();
    connect(m_socket, &QTcpSocket::readyRead, this, &LiveTcpReceiver::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &LiveTcpReceiver::onDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { if (m_socket) emit socketError(m_socket->errorString()); });
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this,
            [this](QAbstractSocket::SocketError) { if (m_socket) emit socketError(m_socket->errorString()); });
#endif
}

void LiveTcpReceiver::onReadyRead()
{
    if (!m_socket)
        return;
    const QByteArray chunk = m_socket->readAll();
    if (chunk.isEmpty())
        return;

    const QHostAddress peer = m_socket->peerAddress();
    const quint16 peerPort = m_socket->peerPort();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (m_frameLength <= 0)
    {
        // Per-chunk fallback: treat each delivery as one message.
        emit datagramReceived(chunk, peer, peerPort, now);
        return;
    }

    m_buffer.append(chunk);
    while (m_buffer.size() >= m_frameLength)
    {
        const QByteArray frame = m_buffer.left(m_frameLength);
        m_buffer.remove(0, m_frameLength);
        emit datagramReceived(frame, peer, peerPort, now);
    }
}

void LiveTcpReceiver::onDisconnected()
{
    if (m_socket)
    {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();

    if (m_role == Connect)
    {
        // Dialled-out link is gone — fatal for this session.
        m_running = false;
        emit socketError("The TCP server closed the connection. "
                         "Solution: restart the server and Start Live again.");
    }
    else
    {
        // Listen mode: stay up and accept the next client (non-fatal).
        emit peerChanged("TCP client disconnected — waiting for a new connection…");
    }
}

void LiveTcpReceiver::stop()
{
    if (m_socket)
    {
        m_socket->disconnect(this);
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_server)
    {
        m_server->disconnect(this);
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_buffer.clear();
    m_running = false;
}

bool LiveTcpReceiver::isRunning() const
{
    return m_running;
}
