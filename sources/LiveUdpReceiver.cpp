#include "LiveUdpReceiver.h"

#include <QVariant>
#include <limits>

LiveUdpReceiver::LiveUdpReceiver(QObject *parent)
    : QObject(parent)
{
}

LiveUdpReceiver::~LiveUdpReceiver()
{
    stop();
}

bool LiveUdpReceiver::start(quint16 port, QString &errorOut)
{
    errorOut.clear();

    // If a previous session is still open, close it before re-binding.
    if (m_running || m_socket)
        stop();

    m_socket = new QUdpSocket(this);

    // Ask the OS for a 1 MB receive buffer to reduce datagram loss
    // during traffic bursts. This is only a hint; the OS may give less.
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(1 << 20));

    if (!m_socket->bind(QHostAddress::AnyIPv4, port)) {
        errorOut = m_socket->errorString();   // e.g. "The bound address is already in use"
        delete m_socket;
        m_socket = nullptr;
        m_running = false;
        return false;
    }

    connect(m_socket, &QUdpSocket::readyRead,
            this, &LiveUdpReceiver::onReadyRead);

    // The socket error signal name differs across Qt versions.
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) {
                if (m_socket)
                    emit socketError(m_socket->errorString());
            });
#else
    connect(m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, [this](QAbstractSocket::SocketError) {
                if (m_socket)
                    emit socketError(m_socket->errorString());
            });
#endif

    m_running = true;
    return true;
}

void LiveUdpReceiver::stop()
{
    if (m_socket) {
        // Detach all signals from this object so no callbacks fire after stop.
        m_socket->disconnect(this);
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->close();
        // deleteLater() is safe even if stop() is reached from inside a slot.
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_running = false;
}

bool LiveUdpReceiver::isRunning() const
{
    return m_running;
}

void LiveUdpReceiver::onReadyRead()
{
    if (!m_socket)
        return;

    // Drain ALL pending datagrams. Reading only one per signal would
    // cause the receiver to fall behind under load.
    while (m_socket->hasPendingDatagrams()) {

        const qint64 pending = m_socket->pendingDatagramSize();

        // Guard against an invalid or impossible size report. Normal UDP
        // payloads are small, but QByteArray::resize() takes int.
        if (pending < 0 || pending > std::numeric_limits<int>::max()) {
            emit socketError(QStringLiteral("Invalid pending datagram size."));
            return;
        }

        QByteArray payload;
        payload.resize(static_cast<int>(pending));   // UDP payload <= 65507 bytes

        QHostAddress sender;
        quint16 senderPort = 0;

        const qint64 read = m_socket->readDatagram(
            payload.data(), payload.size(), &sender, &senderPort);

        if (read < 0) {
            emit socketError(m_socket->errorString());
            return;
        }

        // readDatagram may legitimately return fewer bytes than requested.
        if (read != payload.size())
            payload.resize(static_cast<int>(read));

        emit datagramReceived(payload, sender, senderPort,
                              QDateTime::currentDateTimeUtc());
    }
}
