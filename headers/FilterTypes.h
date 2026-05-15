#ifndef FILTERTYPES_H
#define FILTERTYPES_H

#include <QByteArray>
#include <QList>
#include <QString>

struct MessageFilter
{
    QString label;
    int port;
    QByteArray header;

    MessageFilter() : label(), port(-1), header() {}
};

struct FilterConfiguration
{
    int mode;
    int commonPort;
    QList<MessageFilter> filters;

    FilterConfiguration() : mode(0), commonPort(0), filters() {}
};

#endif
