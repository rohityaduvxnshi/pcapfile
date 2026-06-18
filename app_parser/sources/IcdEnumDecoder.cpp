#include "IcdEnumDecoder.h"

#include <QMap>
#include <QRegularExpression>

QList<BitDecodeRule> IcdEnumDecoder::rulesFromDescription(const QString& description, const QString& label)
{
    QList<BitDecodeRule> out;
    const QString text = description;
    if (text.trimmed().isEmpty())
        return out;

    // A value token ("0xNN" or a small decimal) followed by a separator (-, =, :,
    // ), en/em dash). The meaning runs from here to the next value token (or end).
    QRegularExpression anchorRe(
        "(0x[0-9A-Fa-f]{1,2}|\\b\\d{1,3})\\s*[-=:\\)\\x{2013}\\x{2014}]\\s*",
        QRegularExpression::CaseInsensitiveOption);

    struct Anchor { quint64 value; bool hex; int matchStart; int meaningStart; };
    QList<Anchor> anchors;
    bool anyHex = false;

    QRegularExpressionMatchIterator it = anchorRe.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const QString tok = m.captured(1);
        const bool hex = tok.startsWith("0x", Qt::CaseInsensitive);
        bool ok = false;
        const quint64 v = hex ? tok.mid(2).toULongLong(&ok, 16) : tok.toULongLong(&ok, 10);
        if (!ok || v > 255)
            continue;
        Anchor a;
        a.value = v;
        a.hex = hex;
        a.matchStart = m.capturedStart(0);
        a.meaningStart = m.capturedEnd(0);
        anchors.append(a);
        if (hex)
            anyHex = true;
    }
    if (anchors.size() < 2)
        return out;

    // Build value -> meaning. When any 0xNN token exists, trust only those (decimals
    // in the surrounding prose are usually ranges/units, not enum values).
    QMap<quint64, QString> meanings;
    quint64 maxVal = 0;
    for (int i = 0; i < anchors.size(); ++i)
    {
        if (anyHex && !anchors.at(i).hex)
            continue;
        const int from = anchors.at(i).meaningStart;
        const int to = (i + 1 < anchors.size()) ? anchors.at(i + 1).matchStart : text.size();
        QString meaning = text.mid(from, to - from).simplified();
        // Filter numeric-range noise ("0-359.9 deg"): a real label starts with a letter.
        if (meaning.isEmpty() || !meaning.at(0).isLetter())
            continue;
        if (meaning.size() > 120)
            meaning = meaning.left(117) + "...";
        if (!meanings.contains(anchors.at(i).value))
        {
            meanings.insert(anchors.at(i).value, meaning);
            if (anchors.at(i).value > maxVal)
                maxVal = anchors.at(i).value;
        }
    }
    if (meanings.size() < 2)
        return out;

    // Low bits covering maxVal: 0/1 -> 1 bit (SINGLE_BIT), else as many as needed.
    int bitCount = 1;
    while ((1ULL << bitCount) <= maxVal)
        ++bitCount;
    if (bitCount > 8)
        return out;   // implausible for a status field; bail rather than guess

    BitDecodeRule rule;
    rule.label = label.trimmed().isEmpty() ? QString("ENUM") : label.trimmed();
    for (int b = 0; b < bitCount; ++b)
        rule.bitPositions << b;
    rule.valueMeanings = meanings;
    rule.reserved = false;
    rule.unknownBehavior = "UNKNOWN";
    rule.enabled = true;

    out << rule;
    return out;
}
