#include "AsterixDecoder.h"

#include "AsterixUapRegistry.h"

#include <QString>
#include <QStringList>

#include <cstring>

namespace
{
// ---------------------------------------------------------------------------
// Low-level byte readers
// ---------------------------------------------------------------------------

quint64 readUintBE(const QByteArray& bytes)
{
    quint64 v = 0;
    for (int i = 0; i < bytes.size(); ++i)
    {
        v <<= 8;
        v |= static_cast<quint8>(bytes.at(i));
    }
    return v;
}

qint64 readIntBE(const QByteArray& bytes)
{
    if (bytes.isEmpty()) return 0;
    const int bits = bytes.size() * 8;
    quint64 raw = readUintBE(bytes);
    if (bits >= 64)
    {
        qint64 s = 0;
        std::memcpy(&s, &raw, sizeof(s));
        return s;
    }
    const quint64 mask   = (1ULL << bits) - 1ULL;
    raw &= mask;
    const quint64 signBit = 1ULL << (bits - 1);
    if ((raw & signBit) == 0) return static_cast<qint64>(raw);
    const quint64 mag = ((~raw) & mask) + 1ULL;
    return -static_cast<qint64>(mag);
}

QString hexUpper(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toHex()).toUpper();
}

QString formatFloat(double v)
{
    return QString::number(v, 'f', 6);
}

// ---------------------------------------------------------------------------
// Per-kind value formatters
// ---------------------------------------------------------------------------

QString formatMode3A(const QByteArray& bytes)
{
    if (bytes.size() < 2) return "N/A";
    const quint16 v = static_cast<quint16>(readUintBE(bytes));
    // Lower 12 bits = 4 octal digits A B C D
    const int a = (v >> 9) & 0x7;
    const int b = (v >> 6) & 0x7;
    const int c = (v >> 3) & 0x7;
    const int d =  v       & 0x7;
    // Top bits 13-16: V, G, L, spare. Include validity hint if set.
    const bool valid = ((v >> 15) & 1) == 0;     // V=0 = valid
    const bool garbled = ((v >> 14) & 1) != 0;   // G=1 = garbled
    QString s = QString("%1%2%3%4").arg(a).arg(b).arg(c).arg(d);
    if (!valid) s += "(invalid)";
    if (garbled) s += "(garbled)";
    return s;
}

QString formatModeCFL(const QByteArray& bytes)
{
    if (bytes.size() < 2) return "N/A";
    const quint16 raw = static_cast<quint16>(readUintBE(bytes));
    const bool valid = ((raw >> 15) & 1) == 0;
    const bool garbled = ((raw >> 14) & 1) != 0;
    // Lower 14 bits, signed two's complement, LSB = 1/4 FL.
    qint16 s = static_cast<qint16>((raw & 0x3FFF));
    if (s & 0x2000) s |= 0xC000; // sign-extend 14-bit
    const double fl = s * 0.25;
    QString out = QString("FL%1").arg(fl, 0, 'f', 2);
    if (!valid) out += "(invalid)";
    if (garbled) out += "(garbled)";
    return out;
}

QString formatTimeOfDay(const QByteArray& bytes)
{
    if (bytes.size() < 3) return "N/A";
    const quint32 raw = static_cast<quint32>(readUintBE(bytes));
    // LSB = 1/128 second
    const double seconds = static_cast<double>(raw) / 128.0;
    const int totalMs = static_cast<int>(seconds * 1000.0 + 0.5);
    const int h = (totalMs / 3600000) % 24;
    const int m = (totalMs / 60000) % 60;
    const int s = (totalMs / 1000) % 60;
    const int ms = totalMs % 1000;
    return QString("%1:%2:%3.%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

QString formatLat3(const QByteArray& bytes)
{
    if (bytes.size() < 3) return "N/A";
    const qint64 v = readIntBE(bytes);
    const double deg = static_cast<double>(v) * (180.0 / static_cast<double>(1ULL << 23));
    return formatFloat(deg);
}

QString formatLat4(const QByteArray& bytes)
{
    if (bytes.size() < 4) return "N/A";
    const qint64 v = readIntBE(bytes);
    const double deg = static_cast<double>(v) * (180.0 / static_cast<double>(1ULL << 25));
    return formatFloat(deg);
}

QString formatCallsign6(const QByteArray& bytes)
{
    if (bytes.size() < 6) return "N/A";
    // 8 chars, 6 bits each, packed into 48 bits.
    const quint64 raw = readUintBE(bytes);
    static const char kAlphabet[64] = {
        ' ','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
        'P','Q','R','S','T','U','V','W','X','Y','Z',' ',' ',' ',' ',' ',
        ' ','0','1','2','3','4','5','6','7','8','9',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
    };
    QString s;
    s.reserve(8);
    for (int i = 7; i >= 0; --i)
    {
        const int code = static_cast<int>((raw >> (i * 6)) & 0x3F);
        s.append(QChar(kAlphabet[code]));
    }
    return s.trimmed();
}

QString formatAddress24(const QByteArray& bytes)
{
    if (bytes.size() < 3) return "N/A";
    return hexUpper(bytes);  // ICAO 24-bit hex
}

// Position WGS-84: 6 bytes = 3-byte signed lat + 3-byte signed lon.
// Position WGS-84 HP: 8 bytes = 4-byte signed lat + 4-byte signed lon.
QString formatPositionLatLonPair(const QByteArray& bytes)
{
    if (bytes.size() == 6)
    {
        return formatLat3(bytes.mid(0, 3)) + "," + formatLat3(bytes.mid(3, 3));
    }
    if (bytes.size() == 8)
    {
        return formatLat4(bytes.mid(0, 4)) + "," + formatLat4(bytes.mid(4, 4));
    }
    return hexUpper(bytes);
}
}

QString AsterixDecoder::formatValue(AsterixValueKind kind, const QByteArray& bytes)
{
    if (bytes.isEmpty()) return QString();

    switch (kind)
    {
    case AsterixValueKind::HexBytes:
        return hexUpper(bytes);
    case AsterixValueKind::UintBE:
        return QString::number(static_cast<qulonglong>(readUintBE(bytes)));
    case AsterixValueKind::IntBE:
        return QString::number(static_cast<qlonglong>(readIntBE(bytes)));
    case AsterixValueKind::Float32BE:
    {
        if (bytes.size() != 4) return "N/A";
        const quint32 raw = static_cast<quint32>(readUintBE(bytes));
        float v = 0.0f;
        std::memcpy(&v, &raw, sizeof(v));
        return formatFloat(static_cast<double>(v));
    }
    case AsterixValueKind::TimeOfDay:
        return formatTimeOfDay(bytes);
    case AsterixValueKind::Lat3byte:
        return formatLat3(bytes);
    case AsterixValueKind::Lon3byte:
        return formatLat3(bytes);
    case AsterixValueKind::Lat4byte:
        return formatLat4(bytes);
    case AsterixValueKind::Lon4byte:
        return formatLat4(bytes);
    case AsterixValueKind::Mode3A:
        return formatMode3A(bytes);
    case AsterixValueKind::ModeC_FL:
        return formatModeCFL(bytes);
    case AsterixValueKind::Callsign6:
        return formatCallsign6(bytes);
    case AsterixValueKind::Address24bit:
        return formatAddress24(bytes);
    case AsterixValueKind::MultiPart:
        // Position pair: 6 or 8 bytes — handled via the dedicated helper. For
        // anything else (repetitive blocks, compound aggregates) emit hex.
        if (bytes.size() == 6 || bytes.size() == 8)
            return formatPositionLatLonPair(bytes);
        return hexUpper(bytes);
    }
    return hexUpper(bytes);
}

namespace
{
// ---------------------------------------------------------------------------
// FSPEC walk: read bytes until a byte's LSB (FX) is 0. The 7 high bits of
// each byte correspond, in order, to FRNs frnBase..frnBase+6.
// Returns a list of set FRNs (1-based), in ascending order. Sets bytesConsumed
// to the FSPEC byte count. Returns empty list on truncation; setOk to false.
// ---------------------------------------------------------------------------
QList<int> parseFspec(const QByteArray& payload, int offset, int& bytesConsumed, bool& ok)
{
    QList<int> setFrns;
    bytesConsumed = 0;
    ok = true;
    int frnBase = 1;
    while (true)
    {
        if (offset + bytesConsumed >= payload.size())
        {
            ok = false;
            return setFrns;
        }
        const quint8 b = static_cast<quint8>(payload.at(offset + bytesConsumed));
        ++bytesConsumed;
        // Bits 8..2 → FRNs frnBase..frnBase+6 (bit 8 = frnBase, bit 2 = frnBase+6).
        for (int bit = 0; bit < 7; ++bit)
        {
            const int mask = 1 << (7 - bit);  // bit 8 then 7 then ... then 2
            if (b & mask) setFrns.append(frnBase + bit);
        }
        const bool fx = (b & 0x01) != 0;
        if (!fx) break;
        frnBase += 7;
    }
    return setFrns;
}

// Read an Extended-length field: 1+ extents of `extentLen` bytes. Each extent's
// LSB is the FX continuation bit.
QByteArray readExtended(const QByteArray& payload, int offset, int extentLen,
                        int& bytesConsumed, bool& ok)
{
    bytesConsumed = 0;
    ok = true;
    QByteArray out;
    if (extentLen <= 0)
    {
        ok = false;
        return out;
    }
    while (true)
    {
        if (offset + bytesConsumed + extentLen > payload.size())
        {
            ok = false;
            out.clear();
            return out;
        }
        const QByteArray extent = payload.mid(offset + bytesConsumed, extentLen);
        out += extent;
        bytesConsumed += extentLen;
        const quint8 last = static_cast<quint8>(extent.at(extentLen - 1));
        if ((last & 0x01) == 0) break;
    }
    return out;
}

// Read a Repetitive field: 1-byte REP, then REP * elementLen bytes.
QByteArray readRepetitive(const QByteArray& payload, int offset, int elementLen,
                          int& bytesConsumed, bool& ok)
{
    bytesConsumed = 0;
    ok = true;
    if (offset >= payload.size() || elementLen <= 0)
    {
        ok = false;
        return QByteArray();
    }
    const int rep = static_cast<quint8>(payload.at(offset));
    const int totalLen = 1 + rep * elementLen;
    if (offset + totalLen > payload.size())
    {
        ok = false;
        return QByteArray();
    }
    bytesConsumed = totalLen;
    return payload.mid(offset, totalLen);
}

// Read a Compound field: walk the sub-FSPEC the same way as the main FSPEC,
// then for each set sub-bit (in order) consume the matching sub-item's byteLength.
// Returns the entire compound slice (sub-FSPEC bytes + all sub-item bytes).
QByteArray readCompound(const QByteArray& payload, int offset,
                        const QList<AsterixSubItem>& subItems,
                        int& bytesConsumed, bool& ok, QString& warning)
{
    bytesConsumed = 0;
    ok = true;
    warning.clear();

    // Sub-FSPEC has the same byte layout as the primary FSPEC.
    int fspecLen = 0;
    bool fspecOk = true;
    const QList<int> setBits = parseFspec(payload, offset, fspecLen, fspecOk);
    if (!fspecOk)
    {
        ok = false;
        return QByteArray();
    }
    int cursor = offset + fspecLen;
    for (int i = 0; i < setBits.size(); ++i)
    {
        const int bit = setBits.at(i);  // 1-based
        const int idx = bit - 1;
        if (idx < 0 || idx >= subItems.size())
        {
            warning = QString("compound sub-bit %1 has no sub-item definition").arg(bit);
            ok = false;
            return QByteArray();
        }
        const AsterixSubItem& s = subItems.at(idx);
        if (s.byteLength <= 0)
        {
            warning = QString("compound sub-item %1 has zero byteLength").arg(s.id);
            ok = false;
            return QByteArray();
        }
        if (cursor + s.byteLength > payload.size())
        {
            warning = QString("compound sub-item %1 exceeds payload").arg(s.id);
            ok = false;
            return QByteArray();
        }
        cursor += s.byteLength;
    }
    bytesConsumed = cursor - offset;
    return payload.mid(offset, bytesConsumed);
}

QByteArray readExplicitLength(const QByteArray& payload, int offset,
                              int& bytesConsumed, bool& ok)
{
    bytesConsumed = 0;
    ok = true;
    if (offset >= payload.size())
    {
        ok = false;
        return QByteArray();
    }
    const int len = static_cast<quint8>(payload.at(offset));
    if (len < 1 || offset + len > payload.size())
    {
        ok = false;
        return QByteArray();
    }
    bytesConsumed = len;
    return payload.mid(offset, len);
}
}

AsterixDecoder::Result AsterixDecoder::decodePacket(int expectedCategory,
                                                    const QByteArray& payload)
{
    Result result;

    const AsterixCategoryDef* uap = AsterixUapRegistry::lookup(expectedCategory);
    if (!uap)
    {
        result.fatalError = true;
        result.warnings << QString("Unsupported ASTERIX category %1").arg(expectedCategory);
        return result;
    }

    int datagramCursor = 0;
    while (datagramCursor + 3 <= payload.size())
    {
        const int blockStart = datagramCursor;
        const int cat = static_cast<quint8>(payload.at(blockStart));
        const int len = (static_cast<quint8>(payload.at(blockStart + 1)) << 8)
                      |  static_cast<quint8>(payload.at(blockStart + 2));

        if (len < 3 || blockStart + len > payload.size())
        {
            result.fatalError = true;
            result.warnings << QString("CAT block at offset %1 has invalid length %2 (payload %3)")
                                  .arg(blockStart).arg(len).arg(payload.size());
            return result;
        }

        if (cat != expectedCategory)
        {
            result.warnings << QString("CAT block at offset %1 has category %2 (expected %3) — skipped")
                                  .arg(blockStart).arg(cat).arg(expectedCategory);
            datagramCursor = blockStart + len;
            continue;
        }

        // Walk records inside this block. Each record begins with FSPEC.
        int recordCursor = blockStart + 3;
        const int blockEnd = blockStart + len;
        int recordsInBlock = 0;

        while (recordCursor < blockEnd)
        {
            int fspecLen = 0;
            bool fspecOk = true;
            const QList<int> setFrns = parseFspec(payload, recordCursor, fspecLen, fspecOk);
            if (!fspecOk)
            {
                result.warnings << QString("Truncated FSPEC at offset %1").arg(recordCursor);
                result.fatalError = true;
                return result;
            }

            AsterixDecodedRecord record;
            record.category = expectedCategory;

            int itemCursor = recordCursor + fspecLen;
            bool recordOk = true;

            for (int i = 0; i < setFrns.size(); ++i)
            {
                const int frn = setFrns.at(i);
                if (frn < 1 || frn > uap->uap.size())
                {
                    result.warnings << QString("FRN %1 outside UAP for CAT%2 — stopping record at offset %3")
                                          .arg(frn).arg(expectedCategory).arg(recordCursor);
                    recordOk = false;
                    break;
                }
                const AsterixItemDef& def = uap->uap.at(frn - 1);

                int consumed = 0;
                bool ok = true;
                QByteArray itemBytes;

                switch (def.kind)
                {
                case AsterixItemKind::Fixed:
                {
                    if (def.fixedLength <= 0
                        || itemCursor + def.fixedLength > blockEnd
                        || itemCursor + def.fixedLength > payload.size())
                    {
                        ok = false;
                        break;
                    }
                    itemBytes = payload.mid(itemCursor, def.fixedLength);
                    consumed = def.fixedLength;
                    break;
                }
                case AsterixItemKind::Extended:
                    itemBytes = readExtended(payload, itemCursor, def.extendedExtentLength,
                                              consumed, ok);
                    break;
                case AsterixItemKind::Repetitive:
                    itemBytes = readRepetitive(payload, itemCursor, def.repetitiveElementLength,
                                                consumed, ok);
                    break;
                case AsterixItemKind::Compound:
                {
                    QString warn;
                    itemBytes = readCompound(payload, itemCursor, def.compoundSubItems,
                                              consumed, ok, warn);
                    if (!warn.isEmpty())
                        result.warnings << QString("Item %1: %2").arg(def.id).arg(warn);
                    break;
                }
                case AsterixItemKind::ExplicitLength:
                    itemBytes = readExplicitLength(payload, itemCursor, consumed, ok);
                    break;
                case AsterixItemKind::Unknown:
                default:
                    result.warnings << QString("Item %1 (FRN %2) has no decoder definition — stopping record")
                                          .arg(def.id).arg(frn);
                    ok = false;
                    break;
                }

                if (!ok || itemCursor + consumed > blockEnd)
                {
                    result.warnings << QString("Item %1 (FRN %2) read failed at offset %3")
                                          .arg(def.id).arg(frn).arg(itemCursor);
                    recordOk = false;
                    break;
                }

                AsterixDecodedItem decoded;
                decoded.frn            = frn;
                decoded.id             = def.id;
                decoded.defaultName    = def.defaultName;
                decoded.rawBytes       = itemBytes;
                decoded.formattedValue = formatValue(def.valueKind, itemBytes);
                record.items.append(decoded);

                itemCursor += consumed;
            }

            record.recordLengthBytes = itemCursor - recordCursor;
            result.records.append(record);
            ++recordsInBlock;

            if (!recordOk)
            {
                // Stop walking further records in this block — record stream is
                // out of sync. Keep already-decoded records and continue with
                // any following CAT-blocks in the datagram (if any).
                break;
            }
            recordCursor = itemCursor;
        }

        if (recordsInBlock == 0)
        {
            result.warnings << QString("CAT block at offset %1 produced no records").arg(blockStart);
        }
        datagramCursor = blockEnd;
    }

    return result;
}
