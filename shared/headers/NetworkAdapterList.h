#ifndef NETWORKADAPTERLIST_H
#define NETWORKADAPTERLIST_H

// Enumerates the local network adapters both apps offer for a connection. The
// user picks an adapter by NUMBER (its index in this list) plus a port — no IP
// address is typed for UDP. Entry 0 is always "Any adapter" (bind to every
// interface), followed by the loopback and each up IPv4 interface.

#include <QList>
#include <QString>

struct NetworkAdapter
{
    QString name;       // display label, e.g. "Ethernet — 192.168.1.10"
    QString address;    // IPv4 to bind: "" = any, "127.0.0.1" = loopback
    bool    isLoopback;
    bool    isAny;

    NetworkAdapter() : isLoopback(false), isAny(false) {}
};

// Snapshot of the machine's adapters, numbered for selection. Always begins with
// the synthetic "Any adapter" entry, then loopback, then each active IPv4
// interface. Re-query (e.g. on a Refresh button) to pick up cable/VPN changes.
QList<NetworkAdapter> listNetworkAdapters();

// Find the list index whose bind address matches `address` (used to re-select a
// saved connection's adapter). Returns 0 ("Any") when no match is found.
int adapterIndexForAddress(const QList<NetworkAdapter>& adapters, const QString& address);

#endif // NETWORKADAPTERLIST_H
