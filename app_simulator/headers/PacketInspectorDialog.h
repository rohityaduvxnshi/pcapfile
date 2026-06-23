#ifndef PACKETINSPECTORDIALOG_H
#define PACKETINSPECTORDIALOG_H

// A read-only, Wireshark-style view of a single transmitted packet, opened by
// double-clicking a row in the Outgoing Data History. It shows:
//   1. a protocol tree  (Frame / Ethernet II / IPv4 / UDP|TCP / Data) built from
//      the same synthesized headers the pcapng export writes,
//   2. a field breakdown of the payload against the message definition
//      (name / offset / length / type / hex slice / sent value), and
//   3. a hex + ASCII dump of the payload.
//
// The dialog is built in code (no .ui) and styled via Themes::apply.

#include "MessageDefinition.h"

#include <QByteArray>
#include <QDialog>
#include <QString>
#include <QtGlobal>

class PacketInspectorDialog : public QDialog
{
    Q_OBJECT

public:
    PacketInspectorDialog(const QString& transport,
                          const QString& srcIp, quint16 srcPort,
                          const QString& dstIp, quint16 dstPort,
                          const QByteArray& payload,
                          const QString& messageName,
                          const MessageDefinition& message,
                          QWidget* parent = 0);
};

#endif // PACKETINSPECTORDIALOG_H
