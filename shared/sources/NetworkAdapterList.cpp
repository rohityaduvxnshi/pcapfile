#include "NetworkAdapterList.h"

#include <QHostAddress>
#include <QNetworkInterface>

QList<NetworkAdapter> listNetworkAdapters()
{
    QList<NetworkAdapter> adapters;

    // Entry 0: bind to every interface (QHostAddress::AnyIPv4). Empty address.
    NetworkAdapter any;
    any.name = "Any adapter (all interfaces)";
    any.address = QString();
    any.isAny = true;
    adapters.append(any);

    // Entry 1: loopback — for parser<->simulator on the same machine.
    NetworkAdapter loop;
    loop.name = "Loopback — 127.0.0.1";
    loop.address = "127.0.0.1";
    loop.isLoopback = true;
    adapters.append(loop);

    // Each active IPv4 interface, one entry per IPv4 address. Skip the loopback
    // (already added above) and any interface that is down or has no IPv4.
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaces.size(); ++i)
    {
        const QNetworkInterface& iface = interfaces.at(i);
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp))
            continue;
        if (flags & QNetworkInterface::IsLoopBack)
            continue;

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (int j = 0; j < entries.size(); ++j)
        {
            const QHostAddress addr = entries.at(j).ip();
            if (addr.protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            NetworkAdapter a;
            const QString human = iface.humanReadableName();
            a.name = QString("%1 — %2")
                         .arg(human.isEmpty() ? iface.name() : human)
                         .arg(addr.toString());
            a.address = addr.toString();
            adapters.append(a);
        }
    }

    return adapters;
}

int adapterIndexForAddress(const QList<NetworkAdapter>& adapters, const QString& address)
{
    for (int i = 0; i < adapters.size(); ++i)
    {
        if (adapters.at(i).address == address)
            return i;
    }
    return 0;   // fall back to "Any adapter"
}
