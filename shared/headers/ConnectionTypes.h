#ifndef CONNECTIONTYPES_H
#define CONNECTIONTYPES_H

// Shared connection model for the Universal Data Suite. A ConnectionDefinition is
// a named transport endpoint that both apps can own a list of:
//   Parser    - a live-capture source. The app binds a receiver to the adapter +
//               port; every datagram is tagged with the connection's id so each
//               message only ever decodes traffic from its bound connection.
//   Simulator - a send destination. Each enabled message is transmitted on the
//               connection its connectionId names.
// The struct is a SUPERSET of both apps' needs (the established pattern in this
// repo): the parser ignores the serial/host destination fields, the simulator
// ignores the adapter bind address. A MessageDefinition references one of these
// by its `id` via MessageDefinition::connectionId (empty = default connection).

#include <QString>
#include <QUuid>
#include <QtGlobal>

struct ConnectionDefinition
{
    QString id;        // stable unique id (see makeConnectionId)
    QString name;      // user-facing label, e.g. "Radar UDP 5000"
    QString transport; // "UDP", "TCP", or "SERIAL" (SERIAL is simulator-only)

    // Network adapter. For the parser this is the local interface a receiver binds
    // to; adapterAddress is the IPv4 string to bind ("" = any interface,
    // "127.0.0.1" = loopback). adapterName is the human label shown in the UI.
    QString adapterName;
    QString adapterAddress;

    // Port the connection uses (UDP/TCP). 1..65535.
    quint16 port;

    // TCP role: "Listen" (act as server / parser receive) or "Connect" (dial a
    // remote). host is the remote endpoint for "Connect" (and the simulator's UDP
    // destination IP). Unused for plain UDP receive.
    QString tcpRole;
    QString host;

    // Serial (simulator only). Ignored by the parser.
    QString serialPortName;
    int     serialBaud;
    int     serialDataBits;
    QString serialParity;
    QString serialStopBits;

    ConnectionDefinition()
        : transport("UDP"),
          adapterAddress(),
          port(5000),
          tcpRole("Listen"),
          host("127.0.0.1"),
          serialBaud(9600),
          serialDataBits(8),
          serialParity("None"),
          serialStopBits("1")
    {
    }
};

// Generate a stable, collision-free id for a new connection.
inline QString makeConnectionId()
{
    return QString("conn-") + QUuid::createUuid().toString()
                                   .remove('{').remove('}');
}

#endif // CONNECTIONTYPES_H
