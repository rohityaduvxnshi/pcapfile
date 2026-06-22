#include "ConnectionJsonCodec.h"

QJsonObject ConnectionJsonCodec::toJson(const ConnectionDefinition& c)
{
    QJsonObject o;
    o.insert("id", c.id);
    o.insert("name", c.name);
    o.insert("transport", c.transport);
    o.insert("adapterName", c.adapterName);
    o.insert("adapterAddress", c.adapterAddress);
    o.insert("port", static_cast<int>(c.port));
    o.insert("tcpRole", c.tcpRole);
    o.insert("host", c.host);
    o.insert("serialPortName", c.serialPortName);
    o.insert("serialBaud", c.serialBaud);
    o.insert("serialDataBits", c.serialDataBits);
    o.insert("serialParity", c.serialParity);
    o.insert("serialStopBits", c.serialStopBits);
    return o;
}

ConnectionDefinition ConnectionJsonCodec::fromJson(const QJsonObject& o)
{
    ConnectionDefinition c;
    c.id = o.value("id").toString();
    c.name = o.value("name").toString();
    c.transport = o.value("transport").toString("UDP");
    c.adapterName = o.value("adapterName").toString();
    c.adapterAddress = o.value("adapterAddress").toString();
    const int p = o.value("port").toInt(5000);
    c.port = static_cast<quint16>(p > 0 && p <= 65535 ? p : 5000);
    c.tcpRole = o.value("tcpRole").toString("Listen");
    c.host = o.value("host").toString("127.0.0.1");
    c.serialPortName = o.value("serialPortName").toString();
    c.serialBaud = o.value("serialBaud").toInt(9600);
    c.serialDataBits = o.value("serialDataBits").toInt(8);
    c.serialParity = o.value("serialParity").toString("None");
    c.serialStopBits = o.value("serialStopBits").toString("1");

    // A connection saved before ids existed (or hand-edited) still needs one.
    if (c.id.trimmed().isEmpty())
        c.id = makeConnectionId();
    return c;
}

QJsonArray ConnectionJsonCodec::listToJson(const QList<ConnectionDefinition>& list)
{
    QJsonArray arr;
    for (int i = 0; i < list.size(); ++i)
        arr.append(toJson(list.at(i)));
    return arr;
}

QList<ConnectionDefinition> ConnectionJsonCodec::listFromJson(const QJsonArray& arr)
{
    QList<ConnectionDefinition> list;
    for (int i = 0; i < arr.size(); ++i)
    {
        if (arr.at(i).isObject())
            list.append(fromJson(arr.at(i).toObject()));
    }
    return list;
}
