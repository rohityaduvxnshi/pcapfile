#include "UdpDataSender.h"

#include <QNetworkInterface>
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

void UdpDataSender::setBindAddress(const QString& adapterAddress)
{
    m_bindAddress = adapterAddress.trimmed();
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

    // Bind to the chosen local interface so datagrams leave that NIC (empty =
    // OS default route). Without this, a multi-homed PC can route everything to
    // loopback. ReuseAddressHint keeps the bind from clashing with a receiver on
    // the same host (e.g. the reader's Live mode on the same port).
    if (!m_bindAddress.isEmpty())
    {
        QHostAddress bindAddr(m_bindAddress);
        if (!bindAddr.isNull() && !m_socket->bind(bindAddr, 0, QUdpSocket::ReuseAddressHint))
        {
            errorMessage = QString("Could not send from adapter %1: %2. "
                                   "Solution: pick a different 'Send via adapter' (or 'Any adapter'), "
                                   "or check the adapter is up.")
                               .arg(m_bindAddress, m_socket->errorString());
            delete m_socket;
            m_socket = 0;
            return false;
        }

        // For multicast destinations, also pin the outgoing multicast interface to
        // the matching adapter and give the datagrams a routable TTL.
        if (m_address.isMulticast())
        {
            const QHostAddress bindAddr2(m_bindAddress);
            const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
            for (int i = 0; i < ifaces.size(); ++i)
            {
                const QList<QNetworkAddressEntry> entries = ifaces.at(i).addressEntries();
                bool match = false;
                for (int e = 0; e < entries.size(); ++e)
                    if (entries.at(e).ip() == bindAddr2) { match = true; break; }
                if (match)
                {
                    m_socket->setMulticastInterface(ifaces.at(i));
                    break;
                }
            }
            m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
        }
    }

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
