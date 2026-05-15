#include "InputValidator.h"

#include <QRegExp>
#include <QSet>
#include <QString>

namespace
{
const int FILTER_COUNT_MIN = 1;
const int FILTER_COUNT_MAX = 20;
const int MAX_HEADER_BYTES = 4;

bool headerStartsWith(const QByteArray& fullHeader, const QByteArray& prefix)
{
    if (prefix.size() > fullHeader.size())
    {
        return false;
    }

    return fullHeader.left(prefix.size()) == prefix;
}
}

int InputValidator::minMessageFilterCount()
{
    return FILTER_COUNT_MIN;
}

int InputValidator::maxMessageFilterCount()
{
    return FILTER_COUNT_MAX;
}

bool InputValidator::validateMessageFilterCount(int count, QString& errorMessage)
{
    if (count < FILTER_COUNT_MIN || count > FILTER_COUNT_MAX)
    {
        errorMessage = QString("Number of message filters must be between %1 and %2.")
                           .arg(FILTER_COUNT_MIN)
                           .arg(FILTER_COUNT_MAX);
        return false;
    }

    return true;
}

bool InputValidator::hexStringToBytes(const QString& normalizedHex,
                                      QByteArray& bytesOut,
                                      QString& errorMessage)
{
    bytesOut.clear();

    if ((normalizedHex.length() % 2) != 0)
    {
        errorMessage = "Header hex length must be even.";
        return false;
    }

    for (int i = 0; i < normalizedHex.length(); i += 2)
    {
        const QString byteText = normalizedHex.mid(i, 2);
        bool ok = false;
        const int value = byteText.toInt(&ok, 16);

        if (!ok || value < 0 || value > 0xFF)
        {
            errorMessage = QString("Invalid hex byte '%1' in header.").arg(byteText);
            return false;
        }

        bytesOut.append(static_cast<char>(value));
    }

    return true;
}

bool InputValidator::validateHeaderHexText(const QString& text,
                                           QByteArray& bytesOut,
                                           QString& labelOut,
                                           QString& errorMessage)
{
    bytesOut.clear();
    labelOut.clear();

    QString cleaned = text;
    cleaned.remove(QRegExp("\\s+"));

    if (cleaned.isEmpty())
    {
        labelOut = "EMPTY";
        return true;
    }

    QRegExp hexOnly("^[0-9A-Fa-f]+$");
    if (!hexOnly.exactMatch(cleaned))
    {
        errorMessage = "Header may only contain hex characters 0-9 and A-F.";
        return false;
    }

    if ((cleaned.length() % 2) != 0)
    {
        errorMessage = "Header must have an even number of hex characters: 0, 2, 4, 6, or 8.";
        return false;
    }

    if (cleaned.length() > (MAX_HEADER_BYTES * 2))
    {
        errorMessage = QString("Header is too long. Maximum length is %1 hex characters (%2 bytes).")
                           .arg(MAX_HEADER_BYTES * 2)
                           .arg(MAX_HEADER_BYTES);
        return false;
    }

    const QString upper = cleaned.toUpper();
    QString byteError;
    if (!hexStringToBytes(upper, bytesOut, byteError))
    {
        errorMessage = byteError;
        return false;
    }

    labelOut = upper;
    return true;
}

bool InputValidator::validatePortFilters(const QList<int>& ports,
                                         QList<MessageFilter>& outFilters,
                                         QString& errorMessage)
{
    outFilters.clear();

    if (!validateMessageFilterCount(ports.size(), errorMessage))
    {
        return false;
    }

    QSet<int> seenPorts;
    for (int i = 0; i < ports.size(); ++i)
    {
        const int port = ports.at(i);
        QString portError;

        if (!validatePortValue(port, portError))
        {
            errorMessage = QString("Port filter %1: %2").arg(i + 1).arg(portError);
            return false;
        }

        if (seenPorts.contains(port))
        {
            errorMessage = QString("Duplicate port filter found: %1").arg(port);
            return false;
        }

        seenPorts.insert(port);

        MessageFilter filter;
        filter.label = QString::number(port);
        filter.port = port;
        outFilters.append(filter);
    }

    return true;
}

bool InputValidator::validateHeaderFilters(int commonPort,
                                           const QStringList& hexTexts,
                                           QList<MessageFilter>& outFilters,
                                           QString& errorMessage)
{
    outFilters.clear();

    QString portError;
    if (!validatePortValue(commonPort, portError))
    {
        errorMessage = QString("Common UDP port: %1").arg(portError);
        return false;
    }

    if (!validateMessageFilterCount(hexTexts.size(), errorMessage))
    {
        return false;
    }

    QSet<QString> seenLabels;
    for (int i = 0; i < hexTexts.size(); ++i)
    {
        QByteArray bytes;
        QString label;
        QString headerError;

        if (!validateHeaderHexText(hexTexts.at(i), bytes, label, headerError))
        {
            errorMessage = QString("Header filter %1: %2").arg(i + 1).arg(headerError);
            return false;
        }

        if (seenLabels.contains(label))
        {
            errorMessage = QString("Duplicate header filter found: %1").arg(label);
            return false;
        }

        seenLabels.insert(label);

        MessageFilter filter;
        filter.label = label;
        filter.port = -1;
        filter.header = bytes;
        outFilters.append(filter);
    }

    if (outFilters.size() > 1)
    {
        for (int i = 0; i < outFilters.size(); ++i)
        {
            if (outFilters.at(i).header.isEmpty())
            {
                errorMessage = "EMPTY header matches every packet on the common port, so it can only be used when there is exactly one header filter.";
                return false;
            }
        }
    }

    for (int i = 0; i < outFilters.size(); ++i)
    {
        for (int j = i + 1; j < outFilters.size(); ++j)
        {
            const QByteArray first = outFilters.at(i).header;
            const QByteArray second = outFilters.at(j).header;

            if (headerStartsWith(first, second) || headerStartsWith(second, first))
            {
                errorMessage = QString("Overlapping header filters are not allowed: %1 and %2. Use non-overlapping message headers to keep CSV partitions clean.")
                                   .arg(outFilters.at(i).label)
                                   .arg(outFilters.at(j).label);
                return false;
            }
        }
    }

    return true;
}

bool InputValidator::validateFilterConfiguration(const FilterConfiguration& config,
                                                 QString& errorMessage)
{
    if (!validateMessageFilterCount(config.filters.size(), errorMessage))
    {
        return false;
    }

    if (config.mode == FILTER_MODE_HEADER)
    {
        QString portError;
        if (!validatePortValue(config.commonPort, portError))
        {
            errorMessage = QString("Common UDP port: %1").arg(portError);
            return false;
        }
    }

    QSet<QString> seenLabels;
    for (int i = 0; i < config.filters.size(); ++i)
    {
        const QString label = config.filters.at(i).label;
        if (label.trimmed().isEmpty())
        {
            errorMessage = QString("Filter %1 has an empty label.").arg(i + 1);
            return false;
        }

        if (seenLabels.contains(label))
        {
            errorMessage = QString("Duplicate filter label found: %1").arg(label);
            return false;
        }

        seenLabels.insert(label);
    }

    return true;
}
