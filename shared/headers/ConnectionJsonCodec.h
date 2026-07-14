#ifndef CONNECTIONJSONCODEC_H
#define CONNECTIONJSONCODEC_H

// Serialises a ConnectionDefinition to/from a QJsonObject so the parser's
// ProjectFile and the simulator's SimSetupFile share ONE connection layout
// (a connection saved by one app reads back correctly in the other). Lenient on
// read: missing keys fall back to ConnectionDefinition's defaults.

#include "ConnectionTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>

class ConnectionJsonCodec
{
public:
    static QJsonObject toJson(const ConnectionDefinition& c);
    static ConnectionDefinition fromJson(const QJsonObject& o);

    static QJsonArray listToJson(const QList<ConnectionDefinition>& list);
    static QList<ConnectionDefinition> listFromJson(const QJsonArray& arr);
};

#endif // CONNECTIONJSONCODEC_H
