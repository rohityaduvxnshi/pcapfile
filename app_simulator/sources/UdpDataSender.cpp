#include "UdpDataSender.h"

#include <QUdpSocket>

UdpDataSender::UdpDataSender(QObject* parent)
    : DataSender(parent)
    , m_socket(0)
    , m_port(0)
    , m_open(false)
{
}

UdpDataSender::~UdpDataSender()
{
    close();
}

void UdpDataSender::setDestination(const QString& ipText, int port)
{
    m_ipText = ipText.trimmed();
    m_port = static_cast<quint16>(port);
}

bool UdpDataSender::open(QString& errorMessage)
{
    close();

    if (m_ipText.isEmpty())
    {
        errorMessage = "No destination IP address has been entered. "
                       "Solution: type the receiver's IPv4 address (e.g. 192.168.1.50) "
                       "or a multicast group (e.g. 239.1.5.6).";
        return false;
    }

    QHostAddress address;
    if (!address.setAddress(m_ipText))
    {
        errorMessage = QString("'%1' is not a valid IP address. "
                               "Solution: use dotted IPv4 notation such as 192.168.1.50 "
                               "(hostnames are not resolved).").arg(m_ipText);
        return false;
    }
    m_address = address;

    if (m_port == 0)
    {
        errorMessage = "The destination UDP port is 0. Solution: pick the port the receiver listens on (1-65535).";
        return false;
    }

    m_socket = new QUdpSocket(this);
    m_open = true;
    return true;
}

bool UdpDataSender::send(const QByteArray& payload, QString& errorMessage)
{
    if (!m_open || !m_socket)
    {
        errorMessage = "The UDP link is not connected. Solution: press Connect first.";
        return false;
    }

    if (payload.size() > maxDatagramBytes())
    {
        errorMessage = QString("The payload is %1 bytes but a single UDP datagram can carry at most %2 bytes. "
                               "Solution: reduce the message's Payload Length.")
                           .arg(payload.size()).arg(maxDatagramBytes());
        return false;
    }

    const qint64 written = m_socket->writeDatagram(payload, m_address, m_port);
    if (written != payload.size())
    {
        errorMessage = QString("Sending to %1:%2 failed: %3. "
                               "Solution: check that this PC has a network route to the destination "
                               "(correct subnet / cable / firewall) and try Connect again.")
                           .arg(m_ipText).arg(m_port).arg(m_socket->errorString());
        return false;
    }

    return true;
}

void UdpDataSender::close()
{
    if (m_socket)
    {
        m_socket->deleteLater();
        m_socket = 0;
    }
    m_open = false;
}

bool UdpDataSender::isOpen() const
{
    return m_open;
}

QString UdpDataSender::description() const
{
    return QString("UDP %1:%2").arg(m_ipText).arg(m_port);
}

int UdpDataSender::maxDatagramBytes()
{
    return 65507;
}
