#include "DataSender.h"

DataSender::DataSender(QObject* parent)
    : QObject(parent)
{
}

DataSender::~DataSender()
{
}

QByteArray DataSender::healthMessage()
{
    return QByteArray("UDS-HEALTH-CHECK\r\n");
}
