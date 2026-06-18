#ifndef SIMSETUPFILE_H
#define SIMSETUPFILE_H

// JSON persistence for the Universal Data Simulator.
//
// 1. Whole-setup save/load (File > Open/Save Setup + the silent auto-save on
//    close / auto-restore on launch): destination + every message with its
//    fields, values, ticks and send rates.
// 2. Per-message field-list import/export for the field dialog's JSON menu.
//    Import also accepts the parser app's "PcapUdpExtractorFieldList" files,
//    so field tables exported from Universal Wireshark Log Reader load
//    directly (their decoder configs are ignored; sendValue starts empty).
//
// Writes are atomic: <file>.tmp then rename, keeping a <file>.bak of the
// previous content (same pattern the parser's ProjectFile used).

#include "MessageDefinition.h"

#include <QList>
#include <QString>

struct SimSetup
{
    int version;
    QString destinationType; // "UDP" or "SERIAL"

    QString udpIp;
    int udpPort;

    QString serialPortName;
    int serialBaud;
    int serialDataBits;
    QString serialParity;   // "None"/"Even"/"Odd"/"Mark"/"Space"
    QString serialStopBits; // "1"/"1.5"/"2"

    QList<MessageDefinition> messages;

    SimSetup()
        : version(1),
          destinationType("UDP"),
          udpPort(5000),
          serialBaud(115200),
          serialDataBits(8),
          serialParity("None"),
          serialStopBits("1")
    {
    }
};

class SimSetupFile
{
public:
    static bool save(const SimSetup& setup, const QString& path, QString& errorMessage);
    static bool load(const QString& path, SimSetup& setup, QString& errorMessage);

    // AppDataLocation/last_setup.json (directory created on demand).
    static QString autoSavePath();

    static QString fieldListToJson(const QList<FieldDefinition>& fields);
    static bool fieldListFromJson(const QString& jsonText,
                                  QList<FieldDefinition>& fields,
                                  QString& errorMessage);
};

#endif // SIMSETUPFILE_H
