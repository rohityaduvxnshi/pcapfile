#ifndef COMPAREOPTIONSENGINE_H
#define COMPAREOPTIONSENGINE_H

#include "MessageDefinition.h"

#include <QByteArray>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QtGlobal>

// v13: rolling 1-second window packet counter — emits observed Hz per packet.
class RefreshRateTracker
{
public:
    RefreshRateTracker();
    int observe(qint64 timestampMs);
    void reset();

private:
    QQueue<qint64> m_timestampsMs;
};

namespace CompareOptionsEngine
{
    QStringList compareColumnNames(const MessageDefinition& msg);
    QStringList compareRow(const QByteArray& payload,
                           const MessageDefinition& msg,
                           RefreshRateTracker& tracker,
                           qint64 timestampMs);
}

#endif // COMPAREOPTIONSENGINE_H
