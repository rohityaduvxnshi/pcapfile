#ifndef MESSAGEDEFINITION_H
#define MESSAGEDEFINITION_H

#include "AppTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct MessageDefinition
{
    QString messageName;
    quint16 port;
    int payloadLengthBytes;
    QList<FieldDefinition> fields;

    // v12: optional disambiguator. When non-empty, packetMatchesMessage requires the
    // first N bytes of the UDP payload to match. Allows two messages of the same length
    // on the same port to be told apart by a header signature (e.g. a leading opcode byte).
    // Empty = no header check (pre-v12 behaviour).
    QByteArray optionalHeader;

    // v13: per-message compare options (header / terminator / checksum / refresh rate /
    // endianness). When hasCompareOptions is false, no extra CSV columns are emitted.
    bool hasCompareOptions;
    CompareOptionsConfig compareOptions;

    // NMEA: per-message data format selector. dataFormat == "HEX" (default)
    // preserves all pre-existing byte-offset behaviour. dataFormat == "NMEA"
    // routes extraction through NmeaDecoder and uses nmeaSentenceType (the
    // 3-char formatter, e.g. "GGA") to pick the sentence definition. Empty
    // nmeaSentenceType = unset.
    QString dataFormat;
    QString nmeaSentenceType;

    // Simulator: how often this message is re-sent while streaming (one rate
    // per message; every field travels together in one payload).
    double sendFrequencyHz;

    // Simulator: the "Send?" tick — only ticked messages are streamed.
    bool sendEnabled;

    // Simulator, NMEA only: 2-char talker id prepended before the formatter
    // when building the sentence ($GPGGA -> talker "GP").
    QString nmeaTalker;

    MessageDefinition()
        : port(0),
          payloadLengthBytes(0),
          fields(),
          optionalHeader(),
          hasCompareOptions(false),
          compareOptions(),
          dataFormat("HEX"),
          nmeaSentenceType(),
          sendFrequencyHz(1.0),
          sendEnabled(true),
          nmeaTalker("GP")
    {
    }
};

#endif // MESSAGEDEFINITION_H
