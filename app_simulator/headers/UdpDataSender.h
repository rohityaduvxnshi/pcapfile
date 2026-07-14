#ifndef UDPDATASENDER_H
#define UDPDATASENDER_H

#include "DataSender.h"

#include <QHostAddress>

class QUdpSocket;

class UdpDataSender : public DataSender
{
    Q_OBJECT

public:
    explicit UdpDataSender(QObject* parent = 0);
    ~UdpDataSender();

    // Set before open(). ipText is validated in open().
    void setDestination(const QString& ipText, int port);

    // Local interface to send from (IPv4 string). Empty = OS default route.
    // Set before open(): the socket binds to this address so datagrams egress the
    // chosen NIC, and (for multicast destinations) it becomes the multicast
    // outgoing interface — fixing "only ever sends to loopback" on multi-homed PCs.
    void setBindAddress(const QString& adapterAddress);

    bool open(QString& errorMessage) override;
    bool send(const QByteArray& payload, QString& errorMessage) override;
    void close() override;
    bool isOpen() const override;
    QString description() const override;

    // Largest payload a single UDP datagram can carry (IPv4).
    static int maxDatagramBytes();

private:
    QUdpSocket* m_socket;
    QString m_ipText;
    QString m_bindAddress;
    QHostAddress m_address;
    quint16 m_port;
    bool m_open;
};

#endif // UDPDATASENDER_H
