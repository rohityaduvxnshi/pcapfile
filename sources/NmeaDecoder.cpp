#include "NmeaDecoder.h"
#include "NmeaSentenceRegistry.h"

QString NmeaDecodedRecord::valueAt(int oneBasedIndex) const
{
    for (int i = 0; i < fields.size(); ++i)
    {
        if (fields.at(i).index == oneBasedIndex)
            return fields.at(i).formattedValue;
    }
    return QString();
}

QString NmeaDecodedRecord::rawValueAt(int oneBasedIndex) const
{
    for (int i = 0; i < fields.size(); ++i)
    {
        if (fields.at(i).index == oneBasedIndex)
            return fields.at(i).rawValue;
    }
    return QString();
}

namespace
{

// Split a raw payload into candidate sentence strings. A sentence begins at a
// '$' (parametric) delimiter and runs until the next delimiter, a CR/LF, or
// end of input. '!' encapsulation sentences are out of scope and skipped.
QStringList splitSentences(const QString& text)
{
    QStringList out;
    int i = 0;
    const int n = text.size();
    while (i < n)
    {
        if (text.at(i) != QChar('$'))
        {
            ++i;
            continue;
        }
        int j = i + 1;
        while (j < n)
        {
            const QChar c = text.at(j);
            if (c == QChar('\r') || c == QChar('\n')
                || c == QChar('$') || c == QChar('!'))
                break;
            ++j;
        }
        out << text.mid(i, j - i);   // includes leading '$'
        i = j;
    }
    return out;
}

// Compute the NMEA checksum (8-bit XOR) over the characters of `body`, which
// must be the text between '$' and '*' (exclusive of both).
quint8 xorChecksum(const QString& body)
{
    quint8 cs = 0;
    for (int i = 0; i < body.size(); ++i)
        cs ^= static_cast<quint8>(body.at(i).toLatin1());
    return cs;
}

} // namespace

QString NmeaDecoder::formatValue(NmeaValueKind kind, const QString& raw)
{
    const QString r = raw;
    if (r.isEmpty())
        return QString();

    switch (kind)
    {
    case NmeaValueKind::Latitude:
    {
        // llll.ll : 2 digits degrees, rest minutes
        const int dot = r.indexOf(QChar('.'));
        const int intLen = (dot >= 0) ? dot : r.size();
        if (intLen < 3)
            return r;   // malformed, pass through
        const QString deg = r.left(intLen - 2);
        const QString min = r.mid(intLen - 2);
        return QString("%1 %2").arg(deg, min);
    }
    case NmeaValueKind::Longitude:
    {
        // yyyyy.yy : 3 digits degrees, rest minutes
        const int dot = r.indexOf(QChar('.'));
        const int intLen = (dot >= 0) ? dot : r.size();
        if (intLen < 3)
            return r;
        const QString deg = r.left(intLen - 2);
        const QString min = r.mid(intLen - 2);
        return QString("%1 %2").arg(deg, min);
    }
    case NmeaValueKind::Time:
    {
        // hhmmss.ss
        if (r.size() < 6)
            return r;
        const QString hh = r.mid(0, 2);
        const QString mm = r.mid(2, 2);
        const QString ss = r.mid(4);   // includes optional decimal fraction
        return QString("%1:%2:%3").arg(hh, mm, ss);
    }
    case NmeaValueKind::Date:
    {
        // ddmmyy
        if (r.size() < 6)
            return r;
        return QString("%1/%2/%3").arg(r.mid(0, 2), r.mid(2, 2), r.mid(4, 2));
    }
    case NmeaValueKind::Text:
    case NmeaValueKind::Numeric:
    case NmeaValueKind::Status:
    case NmeaValueKind::Char:
    default:
        return r;
    }
}

NmeaDecoder::Result NmeaDecoder::decodePacket(const QString& expectedFormatter,
                                              const QByteArray& payload)
{
    Result result;

    const QString wanted = expectedFormatter.trimmed().toUpper();
    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(wanted);

    // Latin-1 keeps every byte 1:1 so checksum maths stays exact.
    const QString text = QString::fromLatin1(payload.constData(), payload.size());
    const QStringList sentences = splitSentences(text);

    for (int s = 0; s < sentences.size(); ++s)
    {
        const QString sentence = sentences.at(s);
        if (sentence.size() < 2 || sentence.at(0) != QChar('$'))
            continue;

        // Locate the checksum delimiter.
        QString body;          // between '$' and '*'
        bool checksumOk = false;
        const int star = sentence.indexOf(QChar('*'));
        if (star >= 0)
        {
            body = sentence.mid(1, star - 1);
            const QString csText = sentence.mid(star + 1, 2).trimmed();
            bool parsedHex = false;
            const uint given = csText.toUInt(&parsedHex, 16);
            checksumOk = parsedHex && (static_cast<quint8>(given) == xorChecksum(body));
            if (!checksumOk)
                result.warnings << QString("Checksum mismatch in sentence: %1").arg(sentence);
        }
        else
        {
            body = sentence.mid(1);
            result.warnings << QString("Missing checksum in sentence: %1").arg(sentence);
        }

        // Split into address + data fields.
        const QStringList parts = body.split(QChar(','));
        if (parts.isEmpty())
            continue;

        const QString address = parts.at(0);
        if (address.size() < 5)
        {
            result.warnings << QString("Unrecognized address field: %1").arg(address);
            continue;
        }
        const QString talker = address.left(2);
        const QString formatter = address.mid(2, 3).toUpper();

        if (formatter != wanted)
        {
            result.warnings << QString("Skipped sentence with formatter %1 (expected %2).")
                                   .arg(formatter, wanted);
            continue;
        }

        NmeaDecodedRecord record;
        record.talker = talker;
        record.formatter = formatter;
        record.checksumOk = checksumOk;

        // parts.at(0) is the address; data fields are 1..n.
        for (int p = 1; p < parts.size(); ++p)
        {
            NmeaDecodedField field;
            field.index = p;
            field.rawValue = parts.at(p);

            NmeaValueKind kind = NmeaValueKind::Text;
            if (def)
            {
                for (int k = 0; k < def->fields.size(); ++k)
                {
                    if (def->fields.at(k).index == p)
                    {
                        kind = def->fields.at(k).kind;
                        break;
                    }
                }
            }
            field.formattedValue = formatValue(kind, field.rawValue);
            record.fields.append(field);
        }

        result.records.append(record);
    }

    return result;
}
