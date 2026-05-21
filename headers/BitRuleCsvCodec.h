#ifndef BITRULECSVCODEC_H
#define BITRULECSVCODEC_H

#include "AppTypes.h"

#include <QList>
#include <QString>
#include <QStringList>

class BitRuleCsvCodec
{
public:
    static bool importFromCsv(const QString& path,
                              int fieldLengthBytes,
                              QList<BitDecodeRule>& out,
                              QStringList& warnings,
                              QString& errorMessage);

    static bool exportToCsv(const QString& path,
                            const QList<BitDecodeRule>& rules,
                            QString& errorMessage);

    static bool writeTemplate(const QString& path, QString& errorMessage);
};

#endif // BITRULECSVCODEC_H
