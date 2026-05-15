#ifndef FILTERTYPES_H
#define FILTERTYPES_H

#include <QByteArray>
#include <QList>
#include <QString>

const int FILTER_MODE_PORT = 0;
const int FILTER_MODE_HEADER = 1;

struct MessageFilter
{
    QString label;
    int port;
    QByteArray header;

    MessageFilter()
        : label(),
          port(-1),
          header()
    {
    }
};

struct FilterConfiguration
{
    int mode;
    int commonPort;
    QList<MessageFilter> filters;

    FilterConfiguration()
        : mode(FILTER_MODE_PORT),
          commonPort(0),
          filters()
    {
    }
};

#endif // FILTERTYPES_H
