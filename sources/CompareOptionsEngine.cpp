#include "CompareOptionsEngine.h"

#include <QChar>
#include <QStringList>
#include <QtMath>
#include <cstring>

RefreshRateTracker::RefreshRateTracker()
{
}

int RefreshRateTracker::observe(qint64 timestampMs)
{
    m_timestampsMs.enqueue(timestampMs);
    const qint64 cutoff = timestampMs - 1000;
    while (!m_timestampsMs.isEmpty() && m_timestampsMs.head() < cutoff)
        m_timestampsMs.dequeue();
    return m_timestampsMs.size();
}

void RefreshRateTracker::reset()
{
    m_timestampsMs.clear();
}

namespace
{

bool isMultiByteNumeric(FieldDataType t)
{
    switch (t)
    {
    case FieldDataType::Uint16:
    case FieldDataType::Int16:
    case FieldDataType::Uint32:
    case FieldDataType::Int32:
    case FieldDataType::Uint64:
    case FieldDataType::Int64:
    case FieldDataType::Float32:
    case FieldDataType::Float64:
        return true;
    default:
        return false;
    }
}

bool isSignedIntegerType(FieldDataType t)
{
    return t == FieldDataType::Int16 || t == FieldDataType::Int32 || t == FieldDataType::Int64;
}

QString sliceToHexUpper(const QByteArray& b)
{
    return QString::fromLatin1(b.toHex()).toUpper();
}

QString readIntegerBE(const QByteArray& slice, bool isSigned)
{
    if (slice.isEmpty()) return QString();
    quint64 val = 0;
    for (int i = 0; i < slice.size(); ++i)
        val = (val << 8) | static_cast<quint8>(slice.at(i));
    if (isSigned)
    {
        const int bits = slice.size() * 8;
        if (bits > 0 && bits < 64)
        {
            const quint64 signMask = 1ULL << (bits - 1);
            if (val & signMask)
            {
                const qint64 sub = static_cast<qint64>(1ULL << bits);
                return QString::number(static_cast<qint64>(val) - sub);
            }
        }
        return QString::number(static_cast<qint64>(val));
    }
    return QString::number(val);
}

QString readIntegerLE(const QByteArray& slice, bool isSigned)
{
    if (slice.isEmpty()) return QString();
    quint64 val = 0;
    for (int i = slice.size() - 1; i >= 0; --i)
        val = (val << 8) | static_cast<quint8>(slice.at(i));
    if (isSigned)
    {
        const int bits = slice.size() * 8;
        if (bits > 0 && bits < 64)
        {
            const quint64 signMask = 1ULL << (bits - 1);
            if (val & signMask)
            {
                const qint64 sub = static_cast<qint64>(1ULL << bits);
                return QString::number(static_cast<qint64>(val) - sub);
            }
        }
        return QString::number(static_cast<qint64>(val));
    }
    return QString::number(val);
}

QString readFloat32(const QByteArray& slice, bool bigEndian)
{
    if (slice.size() < 4) return QString();
    quint32 raw = 0;
    if (bigEndian)
    {
        for (int i = 0; i < 4; ++i) raw = (raw << 8) | static_cast<quint8>(slice.at(i));
    }
    else
    {
        for (int i = 3; i >= 0; --i) raw = (raw << 8) | static_cast<quint8>(slice.at(i));
    }
    float f;
    std::memcpy(&f, &raw, sizeof(float));
    return QString::number(static_cast<double>(f), 'g', 7);
}

QString readFloat64(const QByteArray& slice, bool bigEndian)
{
    if (slice.size() < 8) return QString();
    quint64 raw = 0;
    if (bigEndian)
    {
        for (int i = 0; i < 8; ++i) raw = (raw << 8) | static_cast<quint8>(slice.at(i));
    }
    else
    {
        for (int i = 7; i >= 0; --i) raw = (raw << 8) | static_cast<quint8>(slice.at(i));
    }
    double d;
    std::memcpy(&d, &raw, sizeof(double));
    return QString::number(d, 'g', 15);
}

QString readEndianValue(const QByteArray& slice, FieldDataType t, bool bigEndian)
{
    if (slice.isEmpty()) return QString("N/A");
    if (t == FieldDataType::Float32)
        return readFloat32(slice, bigEndian);
    if (t == FieldDataType::Float64)
        return readFloat64(slice, bigEndian);
    return bigEndian
        ? readIntegerBE(slice, isSignedIntegerType(t))
        : readIntegerLE(slice, isSignedIntegerType(t));
}

quint64 computeChecksum(const QByteArray& payload, int start, int end, const QString& algo)
{
    if (start < 0) start = 0;
    if (end > payload.size()) end = payload.size();
    if (start >= end) return 0;
    if (algo == "XOR")
    {
        quint64 acc = 0;
        for (int i = start; i < end; ++i)
            acc ^= static_cast<quint8>(payload.at(i));
        return acc;
    }
    quint64 acc = 0;
    for (int i = start; i < end; ++i)
        acc += static_cast<quint8>(payload.at(i));
    return acc;
}

quint64 readStoredChecksum(const QByteArray& payload, int offset, int length)
{
    if (offset < 0 || length <= 0) return 0;
    if (offset + length > payload.size()) return 0;
    quint64 val = 0;
    for (int i = 0; i < length; ++i)
        val = (val << 8) | static_cast<quint8>(payload.at(offset + i));
    return val;
}

bool hasHeaderComparison(const CompareOptionsConfig& c)
{
    return c.checkHeader && !c.expectedHeader.isEmpty();
}

bool hasTerminatorComparison(const CompareOptionsConfig& c)
{
    return c.checkTerminator && !c.expectedTerminator.isEmpty();
}

bool hasRefreshRateComparison(const CompareOptionsConfig& c)
{
    return c.checkRefreshRate && c.expectedRefreshRateHz > 0.0;
}

bool hasEndianComparison(const CompareOptionsConfig& c)
{
    if (!c.checkEndianness) return false;
    const QString& e = c.expectedEndianness;
    return !e.isEmpty() && e != "(don't compare)";
}

bool hasAnyComparison(const CompareOptionsConfig& c)
{
    return hasHeaderComparison(c)
        || hasTerminatorComparison(c)
        || c.checkChecksum
        || hasRefreshRateComparison(c)
        || hasEndianComparison(c);
}

QString hexFixedWidth(quint64 value, int byteLen)
{
    const int width = byteLen > 0 ? byteLen * 2 : 2;
    return QString("0x%1")
        .arg(value, width, 16, QChar('0'))
        .toUpper()
        .replace("0X", "0x");
}

} // namespace

QStringList CompareOptionsEngine::compareColumnNames(const MessageDefinition& msg)
{
    QStringList cols;
    if (!msg.hasCompareOptions) return cols;
    const CompareOptionsConfig& c = msg.compareOptions;

    if (c.checkHeader)
    {
        cols << "HeaderObserved";
        if (hasHeaderComparison(c))
            cols << "HeaderExpected" << "HeaderOK";
    }
    if (c.checkTerminator)
    {
        cols << "TerminatorObserved";
        if (hasTerminatorComparison(c))
            cols << "TerminatorExpected" << "TerminatorOK";
    }
    if (c.checkChecksum)
    {
        cols << "ChecksumComputed" << "ChecksumStoredInPayload" << "ChecksumOK";
    }
    if (c.checkRefreshRate)
    {
        cols << "RefreshRateObservedHz";
        if (hasRefreshRateComparison(c))
            cols << "RefreshRateExpectedHz" << "RefreshRateOK";
    }
    if (c.checkEndianness)
    {
        const bool compare = hasEndianComparison(c);
        for (int i = 0; i < msg.fields.size(); ++i)
        {
            const FieldDefinition& f = msg.fields.at(i);
            if (!isMultiByteNumeric(f.dataType)) continue;
            cols << (f.name + "_BE") << (f.name + "_LE");
            if (compare)
                cols << (f.name + "_EndianOK");
        }
        if (compare)
            cols << "EndianConfigured";
    }
    if (hasAnyComparison(c))
        cols << "CompareReason";
    return cols;
}

QStringList CompareOptionsEngine::compareRow(const QByteArray& payload,
                                              const MessageDefinition& msg,
                                              RefreshRateTracker& tracker,
                                              qint64 timestampMs)
{
    QStringList row;
    if (!msg.hasCompareOptions) return row;
    const CompareOptionsConfig& c = msg.compareOptions;
    QStringList reasons;

    if (c.checkHeader)
    {
        const bool inBounds = c.headerByteOffset >= 0
            && c.headerLength > 0
            && c.headerByteOffset + c.headerLength <= payload.size();
        QString observed = inBounds
            ? sliceToHexUpper(payload.mid(c.headerByteOffset, c.headerLength))
            : QString("N/A");
        row << observed;
        if (hasHeaderComparison(c))
        {
            row << sliceToHexUpper(c.expectedHeader);
            QString ok;
            if (!inBounds)
            {
                ok = "False";
                reasons << "Header OutOfBounds";
            }
            else
            {
                const QByteArray got = payload.mid(c.headerByteOffset, c.headerLength);
                if (got == c.expectedHeader)
                {
                    ok = "True";
                }
                else
                {
                    ok = "False";
                    reasons << QString("Header mismatch (got %1, expected %2)")
                                .arg(sliceToHexUpper(got), sliceToHexUpper(c.expectedHeader));
                }
            }
            row << ok;
        }
    }

    if (c.checkTerminator)
    {
        int offset = c.terminatorByteOffset;
        if (offset < 0) offset = payload.size() - c.terminatorLength;
        const bool inBounds = offset >= 0
            && c.terminatorLength > 0
            && offset + c.terminatorLength <= payload.size();
        QString observed = inBounds
            ? sliceToHexUpper(payload.mid(offset, c.terminatorLength))
            : QString("N/A");
        row << observed;
        if (hasTerminatorComparison(c))
        {
            row << sliceToHexUpper(c.expectedTerminator);
            QString ok;
            if (!inBounds)
            {
                ok = "False";
                reasons << "Terminator OutOfBounds";
            }
            else
            {
                const QByteArray got = payload.mid(offset, c.terminatorLength);
                if (got == c.expectedTerminator)
                {
                    ok = "True";
                }
                else
                {
                    ok = "False";
                    reasons << QString("Terminator mismatch (got %1, expected %2)")
                                .arg(sliceToHexUpper(got), sliceToHexUpper(c.expectedTerminator));
                }
            }
            row << ok;
        }
    }

    if (c.checkChecksum)
    {
        const bool computeInBounds = c.checksumRangeStart >= 0
            && c.checksumRangeEnd <= payload.size()
            && c.checksumRangeStart < c.checksumRangeEnd;
        const bool storedInBounds = c.checksumByteOffset >= 0
            && c.checksumLength > 0
            && c.checksumByteOffset + c.checksumLength <= payload.size();
        quint64 computed = 0;
        quint64 stored = 0;
        if (computeInBounds)
        {
            computed = computeChecksum(payload, c.checksumRangeStart, c.checksumRangeEnd, c.checksumAlgorithm);
            if (c.checksumLength > 0 && c.checksumLength < 8)
            {
                const quint64 mask = (1ULL << (c.checksumLength * 8)) - 1;
                computed &= mask;
            }
        }
        if (storedInBounds)
            stored = readStoredChecksum(payload, c.checksumByteOffset, c.checksumLength);
        const QString comp = computeInBounds ? hexFixedWidth(computed, c.checksumLength) : QString("N/A");
        const QString stor = storedInBounds ? hexFixedWidth(stored, c.checksumLength) : QString("N/A");
        row << comp << stor;
        QString ok;
        if (!computeInBounds || !storedInBounds)
        {
            ok = "False";
            reasons << "Checksum OutOfBounds";
        }
        else if (computed == stored)
        {
            ok = "True";
        }
        else
        {
            ok = "False";
            reasons << QString("Checksum mismatch (computed %1, stored %2)").arg(comp, stor);
        }
        row << ok;
    }

    if (c.checkRefreshRate)
    {
        const int observedHz = tracker.observe(timestampMs);
        row << QString::number(observedHz);
        if (hasRefreshRateComparison(c))
        {
            row << QString::number(c.expectedRefreshRateHz, 'g', 6);
            const double diff = qAbs(static_cast<double>(observedHz) - c.expectedRefreshRateHz);
            QString ok;
            if (diff <= c.refreshRateToleranceHz)
            {
                ok = "True";
            }
            else
            {
                ok = "False";
                reasons << QString("RefreshRate %1Hz outside %2+/-%3Hz")
                            .arg(observedHz)
                            .arg(c.expectedRefreshRateHz)
                            .arg(c.refreshRateToleranceHz);
            }
            row << ok;
        }
    }

    if (c.checkEndianness)
    {
        const bool compare = hasEndianComparison(c);
        for (int i = 0; i < msg.fields.size(); ++i)
        {
            const FieldDefinition& f = msg.fields.at(i);
            if (!isMultiByteNumeric(f.dataType)) continue;
            QByteArray slice;
            if (f.byteOffsetcorrect >= 0
                && f.length > 0
                && f.byteOffsetcorrect + f.length <= payload.size())
            {
                slice = payload.mid(f.byteOffsetcorrect, f.length);
            }
            row << readEndianValue(slice, f.dataType, true);
            row << readEndianValue(slice, f.dataType, false);
            if (compare)
            {
                // Project's natural decode is big-endian; OK == True only when
                // user-stated endianness matches that.
                const QString ok = (c.expectedEndianness == "BIG") ? "True" : "False";
                row << ok;
                if (ok == "False")
                    reasons << QString("Endian configured %1 but project decodes BIG (field %2)")
                                .arg(c.expectedEndianness, f.name);
            }
        }
        if (compare)
            row << c.expectedEndianness;
    }

    if (hasAnyComparison(c))
        row << reasons.join(QStringLiteral("; "));

    return row;
}
