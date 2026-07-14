// Universal Data Suite — headless functional test harness.
//
// Exercises the REAL data-path code of both apps (no mocks): the shared ICD
// importer + codecs + validators, the simulator's PayloadBuilder (encode), the
// parser's ExtractionEngine (decode), and the PcapWriter -> PcapFileReader ->
// UdpPacketParser capture round-trip. Each check appends a pass/fail record;
// the records are written to test_files/output/test_results.json for the Excel
// report generator (test_files/make_test_report.py).

#include "AppTypes.h"
#include "ConnectionJsonCodec.h"
#include "ConnectionTypes.h"
#include "ExcelFieldCodec.h"
#include "ExtractionEngine.h"
#include "IcdDocxImporter.h"
#include "IcdImportTypes.h"
#include "InputValidator.h"
#include "MessageDefinition.h"
#include "MessageJsonCodec.h"
#include "NmeaDecoder.h"
#include "PayloadBuilder.h"
#include "PcapFileReader.h"
#include "PcapWriter.h"
#include "UdpPacketParser.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cstdio>

// ---------------------------------------------------------------- result sink
struct Rec
{
    QString id;
    QString area;     // feature group (drives grouping in the report)
    QString icd;      // ICD / dataset the check ran against ("-" for none)
    QString problem;  // the problem/feature this case verifies
    QString status;   // "PASS" / "FAIL"
    QString detail;   // short evidence string
};

static QList<Rec> g_recs;
static int g_counter = 0;

static void add(const QString& area, const QString& icd, const QString& problem,
                bool pass, const QString& detail)
{
    Rec r;
    r.id = QString("TC-%1").arg(++g_counter, 2, 10, QChar('0'));
    r.area = area;
    r.icd = icd.isEmpty() ? QString("-") : icd;
    r.problem = problem;
    r.status = pass ? "PASS" : "FAIL";
    r.detail = detail;
    g_recs.append(r);
    std::fprintf(stdout, "[%s] %-22s | %-22s | %s\n",
                 qPrintable(r.status), qPrintable(r.area), qPrintable(r.icd), qPrintable(r.problem));
    if (!pass)
        std::fprintf(stdout, "        -> %s\n", qPrintable(detail));
}

// ---------------------------------------------------------------- ICD helpers
// Drive the REAL import pipeline: extract -> continuation grouping ->
// per-parent suggestMapping -> buildGroupedDrafts. Mirrors what each app's
// IcdImportDialog does, minus the editable review UI.
static bool importIcd(const QString& path, int& tableCount,
                      QList<IcdMessageDraft>& drafts, QString& err)
{
    tableCount = 0;
    drafts.clear();
    IcdDocument doc;
    if (!IcdDocxImporter::extract(path, doc, err))
        return false;
    tableCount = doc.tables.size();

    QList<int> sel;
    for (int i = 0; i < doc.tables.size(); ++i)
        sel << i;
    QHash<int, int> parentOf;
    IcdDocxImporter::suggestContinuationGroups(doc, sel, parentOf);

    QList<IcdTableGroup> groups;
    for (int t = 0; t < doc.tables.size(); ++t)
    {
        if (parentOf.value(t, t) != t)
            continue; // a child; folded into its parent below
        IcdTableGroup grp;
        IcdMappingProfile prof;
        IcdDocxImporter::suggestMapping(doc.tables.at(t), prof);
        grp.mapping = prof;
        grp.tableIndices << t;
        for (int c = 0; c < doc.tables.size(); ++c)
            if (c != t && parentOf.value(c, c) == t)
                grp.tableIndices << c;
        groups << grp;
    }

    QStringList warns;
    IcdDocxImporter::buildGroupedDrafts(doc, groups, drafts, warns);
    return true;
}

static int totalFields(const QList<IcdMessageDraft>& drafts)
{
    int n = 0;
    for (int i = 0; i < drafts.size(); ++i)
        n += drafts.at(i).message.fields.size();
    return n;
}

static QList<FieldDefinition> allFields(const QList<IcdMessageDraft>& drafts)
{
    QList<FieldDefinition> out;
    for (int i = 0; i < drafts.size(); ++i)
        out += drafts.at(i).message.fields;
    return out;
}

// --------------------------------------------------- encode/decode contract
static QString sampleValue(FieldDataType t)
{
    switch (t)
    {
    case FieldDataType::Uint8:   return "200";
    case FieldDataType::Int8:    return "-50";
    case FieldDataType::Uint16:  return "12345";
    case FieldDataType::Int16:   return "-1234";
    case FieldDataType::Uint32:  return "100000";
    case FieldDataType::Int32:   return "-100000";
    case FieldDataType::Uint64:  return "5000000000";
    case FieldDataType::Int64:   return "-5000000000";
    case FieldDataType::Float32: return "3.5";
    case FieldDataType::Float64: return "2.5";
    case FieldDataType::Bool:    return "1";
    case FieldDataType::String:  return "Hi";
    case FieldDataType::RawUnsignedBE: return "1234";
    default: return "1";
    }
}

// Encode a value with the simulator, decode it back with the parser, and check
// the result matches — proving the encode/decode contract for this field. Wide
// (>8 byte) integer fields are decode-only (the simulator caps encode at 8B).
static bool contractField(FieldDefinition f, QString& detail)
{
    f.byteOffset = 1;
    f.byteOffsetcorrect = 0;
    f.resolution = 1.0;
    f.resolutionExpression = "1";
    f.endianness = FieldEndianness::Big;
    if (f.length <= 0)
        f.length = qMax(1, fieldDataTypeNaturalLength(f.dataType));

    // Wide integer: simulator cannot encode >8 bytes, so test the reader's
    // exact base-256 -> base-10 wide decode against a known byte pattern.
    if (f.length > 8 && f.dataType != FieldDataType::String)
    {
        QByteArray bytes(f.length, char(0));
        const quint32 v = 12345678u;
        bytes[f.length - 1] = char(v & 0xFF);
        bytes[f.length - 2] = char((v >> 8) & 0xFF);
        bytes[f.length - 3] = char((v >> 16) & 0xFF);
        bytes[f.length - 4] = char((v >> 24) & 0xFF);
        const QString dec = ExtractionEngine::valueFromPayload(bytes, f).trimmed();
        const bool ok = (dec == QString::number(v));
        detail = QString("wide len=%1 decoded '%2' (expected %3)").arg(f.length).arg(dec).arg(v);
        return ok;
    }

    const QString v = sampleValue(f.dataType);
    QByteArray bytes;
    QString reason, solution;
    if (!PayloadBuilder::encodeFieldValue(f, v, bytes, reason, solution))
    {
        detail = "encode failed: " + reason;
        return false;
    }
    if (bytes.size() != f.length)
    {
        detail = QString("encoded %1 bytes, expected %2").arg(bytes.size()).arg(f.length);
        return false;
    }
    const QString dec = ExtractionEngine::valueFromPayload(bytes, f).trimmed();

    if (f.dataType == FieldDataType::String)
    {
        const bool ok = (dec == v);
        detail = QString("string '%1' -> '%2'").arg(v).arg(dec);
        return ok;
    }
    if (f.dataType == FieldDataType::Bool)
    {
        const bool ok = dec.contains("1") || dec.toLower().contains("true");
        detail = QString("bool '%1' -> '%2'").arg(v).arg(dec);
        return ok;
    }
    bool okd = false;
    const double dv = dec.toDouble(&okd);
    const double ev = v.toDouble();
    const double tol = (f.dataType == FieldDataType::Float32) ? 1e-3
                     : (f.dataType == FieldDataType::Float64) ? 1e-9 : 0.5;
    const bool ok = okd && qAbs(dv - ev) <= tol;
    detail = QString("%1 -> '%2'").arg(v).arg(dec);
    return ok;
}

static bool contractAllFields(const QList<FieldDefinition>& fields, QString& detail)
{
    int passed = 0;
    QStringList fails;
    for (int i = 0; i < fields.size(); ++i)
    {
        QString d;
        if (contractField(fields.at(i), d))
            ++passed;
        else
            fails << QString("%1: %2").arg(fields.at(i).name).arg(d);
    }
    detail = QString("%1/%2 fields round-tripped").arg(passed).arg(fields.size());
    if (!fails.isEmpty())
        detail += "; failed: " + fails.join(" | ");
    return fails.isEmpty() && !fields.isEmpty();
}

static bool hasField(const QList<FieldDefinition>& fields, const QString& name)
{
    for (int i = 0; i < fields.size(); ++i)
        if (fields.at(i).name == name)
            return true;
    return false;
}

// ----------------------------------------------------------- per-ICD tests
static void testRadarBasic(const QString& dir)
{
    const QString icd = "icd_radar_basic.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract a multi-message ICD .docx", false, err); return; }
    add("ICD Import", icd, "Extract a multi-message ICD into separate messages",
        d.size() == 2 && tc == 2, QString("tables=%1 messages=%2 (expected 2/2)").arg(tc).arg(d.size()));
    add("ICD Import", icd, "Map all field rows of both messages",
        totalFields(d) == 13, QString("total fields=%1 (expected 13)").arg(totalFields(d)));
    QString cd;
    add("Encode/Decode", icd, "Encode (simulator) then decode (reader) every field",
        contractAllFields(allFields(d), cd), cd);
}

static void testAllTypes(const QString& dir)
{
    const QString icd = "icd_all_types.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract an all-types ICD", false, err); return; }
    const QList<FieldDefinition> f = allFields(d);
    add("ICD Import", icd, "Recognise every supported data type label",
        f.size() == 12, QString("fields=%1 (expected 12)").arg(f.size()));
    // verify a representative spread of decoded types
    const FieldDataType expect[12] = {
        FieldDataType::Uint8, FieldDataType::Int8, FieldDataType::Uint16, FieldDataType::Int16,
        FieldDataType::Uint32, FieldDataType::Int32, FieldDataType::Uint64, FieldDataType::Int64,
        FieldDataType::Float32, FieldDataType::Float64, FieldDataType::Bool, FieldDataType::String };
    bool typesOk = (f.size() == 12);
    for (int i = 0; typesOk && i < 12; ++i)
        typesOk = (f.at(i).dataType == expect[i]);
    add("ICD Import", icd, "Map each cell to the correct FieldDataType", typesOk,
        typesOk ? "all 12 types mapped in order" : "type order mismatch");
    QString cd;
    add("Encode/Decode", icd, "Round-trip all 12 types (int/float/bool/string)",
        contractAllFields(f, cd), cd);
}

static void testScaled(const QString& dir)
{
    const QString icd = "icd_scaled.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract a scaled-resolution ICD", false, err); return; }
    QList<FieldDefinition> f = allFields(d);
    add("ICD Import", icd, "Carry per-field resolution from the ICD",
        f.size() == 4, QString("fields=%1 (expected 4)").arg(f.size()));

    // Scaled decode: Pressure is Uint16 @ 0.01. raw 2500 -> 25.00.
    bool scaledOk = false; QString sd = "Pressure field not found";
    for (int i = 0; i < f.size(); ++i)
    {
        if (f.at(i).name == "Pressure")
        {
            FieldDefinition pf = f.at(i);
            pf.byteOffset = 1; pf.byteOffsetcorrect = 0; pf.length = 2;
            QByteArray raw; raw.append(char(0x09)); raw.append(char(0xC4)); // 2500 BE
            const QString val = ExtractionEngine::valueFromPayload(raw, pf).trimmed();
            const double dv = val.toDouble();
            scaledOk = qAbs(dv - 25.0) < 1e-6;
            sd = QString("res=%1 raw=2500 -> '%2' (expected 25.0)").arg(pf.resolution).arg(val);
            break;
        }
    }
    add("Decode", icd, "Apply resolution scaling on decode (value = raw * res)", scaledOk, sd);
}

static void testWide(const QString& dir)
{
    const QString icd = "icd_wide_fields.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract a wide-field ICD", false, err); return; }
    QList<FieldDefinition> f = allFields(d);
    add("ICD Import", icd, "Accept fields wider than 8 bytes",
        f.size() == 4, QString("fields=%1 (expected 4)").arg(f.size()));
    QString cd;
    add("Decode", icd, "Decode >8-byte fields exactly (base-256 to base-10)",
        contractAllFields(f, cd), cd);
}

static void testLengthMsgId(const QString& dir)
{
    const QString icd = "icd_length_msgid.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract a framed (id+length) ICD", false, err); return; }
    QList<FieldDefinition> f = allFields(d);
    add("ICD Import", icd, "Import a framed message carrying id + data-length fields",
        f.size() == 4 && hasField(f, "MessageID") && hasField(f, "DataLength"),
        QString("fields=%1, MessageID=%2, DataLength=%3").arg(f.size())
            .arg(hasField(f, "MessageID")).arg(hasField(f, "DataLength")));
    QString cd;
    add("Encode/Decode", icd, "Round-trip the framed message fields", contractAllFields(f, cd), cd);
}

static void testLenient(const QString& dir)
{
    const QString icd = "icd_lenient.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract an imperfect ICD", false, err); return; }
    QList<FieldDefinition> f = allFields(d);
    // Headless build keeps the 3 well-formed rows; the duplicate "Alpha" is
    // renamed "Alpha_2"; the name-less and type-less rows are skipped safely.
    add("ICD Import", icd, "Auto-rename a duplicate field name to _2",
        hasField(f, "Alpha") && hasField(f, "Alpha_2"),
        QString("fields: %1").arg([&]{ QStringList n; for (const FieldDefinition& x : f) n << x.name; return n.join(","); }()));
    add("ICD Import", icd, "Skip rows missing a required key without crashing",
        f.size() == 3, QString("kept %1 of 5 rows (expected 3)").arg(f.size()));
}

static void testContinuation(const QString& dir)
{
    const QString icd = "icd_continuation.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract a split (continuation) ICD", false, err); return; }
    add("ICD Import", icd, "Merge a continuation table into one message",
        d.size() == 1 && tc == 2, QString("tables=%1 messages=%2 (expected 2 tables -> 1 message)").arg(tc).arg(d.size()));
    add("ICD Import", icd, "Concatenate fields across the merged tables",
        totalFields(d) == 6, QString("fields=%1 (expected 6)").arg(totalFields(d)));
}

static void testWordOffsets(const QString& dir)
{
    const QString icd = "icd_word_offsets.docx";
    int tc = 0; QList<IcdMessageDraft> d; QString err;
    if (!importIcd(dir + "/" + icd, tc, d, err)) { add("ICD Import", icd, "Extract a word-addressed ICD", false, err); return; }
    add("ICD Import", icd, "Import a word-addressed block (offset unit context)",
        totalFields(d) == 4, QString("fields=%1 (expected 4)").arg(totalFields(d)));

    // Bytes<->Words offset conversion is stable round-trip (regression for the
    // word-offset save bug: byteOffsetToUnit(unitToByteOffset(w)) must equal w).
    bool stable = true; int badW = -1;
    for (int w = 1; w <= 64; ++w)
    {
        const int back = byteOffsetToUnit(unitToByteOffset(w, "WORDS"), "WORDS");
        if (back != w) { stable = false; badW = w; break; }
    }
    add("Offset Units", icd, "Bytes/Words offset conversion is a stable round-trip",
        stable, stable ? "w==byteOffsetToUnit(unitToByteOffset(w)) for 1..64"
                       : QString("failed at word %1").arg(badW));
}

// ------------------------------------------------------------ codec tests
static void testJsonRoundTrip()
{
    MessageDefinition m;
    m.messageName = "RoundTrip";
    m.port = 5000;
    m.payloadLengthBytes = 16;
    m.offsetUnit = "WORDS";
    m.connectionId = "conn-A";
    m.connectionIds = QStringList() << "conn-A" << "conn-B";
    FieldDefinition f;
    f.name = "Val"; f.byteOffset = 1; f.byteOffsetcorrect = 0; f.length = 2;
    f.dataType = FieldDataType::Uint16; f.resolution = 0.01; f.resolutionExpression = "0.01";
    f.sendValueText = "12.34"; f.endianness = FieldEndianness::Little;
    m.fields << f;

    const QString json = MessageJsonCodec::messageToJson(m);
    MessageDefinition out; QString err;
    const bool ok = MessageJsonCodec::messageFromJson(json, out, err);
    const bool match = ok
        && out.messageName == m.messageName
        && out.payloadLengthBytes == m.payloadLengthBytes
        && out.offsetUnit == "WORDS"
        && out.fields.size() == 1
        && out.fields.at(0).dataType == FieldDataType::Uint16
        && qAbs(out.fields.at(0).resolution - 0.01) < 1e-9
        && out.fields.at(0).endianness == FieldEndianness::Little;
    add("JSON Codec", "-", "Lossless whole-message JSON export/import", match,
        ok ? "fields/offsetUnit/resolution/endianness preserved" : ("parse failed: " + err));
}

static void testFieldsJsonInterchange()
{
    QList<FieldDefinition> fields;
    for (int i = 0; i < 3; ++i)
    {
        FieldDefinition f;
        f.name = QString("f%1").arg(i); f.byteOffset = i * 2 + 1; f.byteOffsetcorrect = i * 2;
        f.length = 2; f.dataType = FieldDataType::Int16; f.resolution = 1.0;
        fields << f;
    }
    const QString json = MessageJsonCodec::fieldsToJson(fields);
    QList<FieldDefinition> out; QString err;
    const bool ok = MessageJsonCodec::fieldsFromJson(json, out, err);
    add("JSON Codec", "-", "Field-list JSON interchange between both apps",
        ok && out.size() == 3, ok ? QString("%1 fields").arg(out.size()) : err);
}

static void testExcelRoundTrip(const QString& outDir)
{
    QList<FieldDefinition> fields;
    const FieldDataType types[4] = { FieldDataType::Uint16, FieldDataType::Int32,
                                     FieldDataType::Float32, FieldDataType::String };
    int off = 1;
    for (int i = 0; i < 4; ++i)
    {
        FieldDefinition f;
        f.name = QString("col%1").arg(i); f.byteOffset = off; f.byteOffsetcorrect = off - 1;
        f.length = (types[i] == FieldDataType::String) ? 8 : fieldDataTypeNaturalLength(types[i]);
        f.dataType = types[i]; f.resolution = (i == 0) ? 0.5 : 1.0;
        f.resolutionExpression = (i == 0) ? "0.5" : "1";
        f.sendValueText = (types[i] == FieldDataType::String) ? "txt" : "7";
        fields << f; off += f.length;
    }
    const QString path = outDir + "/excel_roundtrip.xlsx";
    QString err;
    if (!ExcelFieldCodec::exportFields(path, fields, err))
    {
        add("Excel Codec", "-", "Export a field list to .xlsx", false, err);
        return;
    }
    QList<FieldDefinition> back; QStringList warns; QString ierr;
    const bool ok = ExcelFieldCodec::importFields(path, 0, back, warns, ierr);
    const bool match = ok && back.size() == 4
        && back.at(0).dataType == FieldDataType::Uint16
        && back.at(3).dataType == FieldDataType::String;
    add("Excel Codec", "-", "Excel (.xlsx) field-list export then import round-trip",
        match, ok ? QString("%1 fields back").arg(back.size()) : ierr);
}

static void testConnections()
{
    QList<ConnectionDefinition> conns;
    ConnectionDefinition a; a.id = "conn-A"; a.name = "Radar UDP"; a.transport = "UDP"; a.port = 5000;
    ConnectionDefinition b; b.id = "conn-B"; b.name = "Log TCP"; b.transport = "TCP"; b.port = 6000; b.host = "127.0.0.1";
    conns << a << b;
    const QJsonArray arr = ConnectionJsonCodec::listToJson(conns);
    const QList<ConnectionDefinition> back = ConnectionJsonCodec::listFromJson(arr);
    const bool ok = back.size() == 2 && back.at(0).id == "conn-A" && back.at(1).transport == "TCP";
    add("Connections", "-", "Serialise the connection list (shared by both apps)", ok,
        QString("%1 connections back").arg(back.size()));

    // Multi-connection binding: a message bound to two connections fans out.
    MessageDefinition m;
    m.connectionIds = QStringList() << "conn-A" << "conn-B";
    const QStringList eff = messageConnectionIds(m);
    add("Connections", "-", "One message -> many connections (multi-destination)",
        eff.size() == 2 && eff.contains("conn-A") && eff.contains("conn-B"),
        QString("effective destinations: %1").arg(eff.join(",")));

    // Legacy single binding still resolves.
    MessageDefinition m2; m2.connectionId = "conn-A";
    const QStringList eff2 = messageConnectionIds(m2);
    add("Connections", "-", "Back-compat: single connectionId still resolves",
        eff2.size() == 1 && eff2.at(0) == "conn-A", QString("resolved: %1").arg(eff2.join(",")));
}

// ------------------------------------------------------------ pcap round-trip
static void testPcapRoundTrip(const QString& outDir)
{
    const QByteArray payload = QByteArray::fromHex("AA55001002BCDEF0010203040506070809");
    const QString path = outDir + "/roundtrip.pcapng";

    PcapWriter w; QString err;
    if (!w.openPcapng(path, err)) { add("Capture", "-", "Write a pcapng capture file", false, err); return; }
    const QByteArray udp = PcapFrame::buildEthIpUdp("192.168.1.10", 49152, "192.168.1.20", 5000, payload);
    const QByteArray tcp = PcapFrame::buildEthIpTcp("192.168.1.10", 49153, "192.168.1.20", 6000, payload);
    bool wrote = w.writePacket(1000000, udp, err);
    wrote = w.writePacket(2000000, tcp, err) && wrote;
    w.close();
    add("Capture", "-", "Synthesize Eth/IPv4/UDP+TCP frames and write pcapng", wrote, wrote ? "2 frames written" : err);

    // Read it back through the reader's own pcap parser + UDP/IP stripper.
    PcapFileReader r; QString rerr;
    if (!r.open(path, rerr)) { add("Capture", "-", "Reader re-opens the pcapng", false, rerr); return; }
    RawPacket pkt; bool gotUdp = false; QString detail = "no UDP packet parsed";
    while (r.readNextPacket(pkt, rerr))
    {
        const ParsedUdpPacket p = UdpPacketParser::parsePacket(pkt);
        if (p.valid && p.destinationPort == 5000)
        {
            gotUdp = (p.udpPayload == payload && p.sourceIp == "192.168.1.10" && p.destinationIp == "192.168.1.20");
            detail = QString("payload %1B, %2:%3 -> %4:%5")
                         .arg(p.udpPayload.size()).arg(p.sourceIp).arg(p.sourcePort)
                         .arg(p.destinationIp).arg(p.destinationPort);
            break;
        }
    }
    r.close();
    add("Capture", "-", "pcapng round-trips: payload + IPs + ports survive write->read", gotUdp, detail);
}

// ------------------------------------------------------------ NMEA round-trip
static void testNmea()
{
    MessageDefinition m;
    m.messageName = "GGA_Test"; m.dataFormat = "NMEA"; m.nmeaSentenceType = "GGA"; m.nmeaTalker = "GP";
    FieldDefinition t; t.name = "utc"; t.nmeaFieldIndex = 1; t.nmeaValueKind = 0; t.sendValueText = "123519";
    FieldDefinition lat; lat.name = "lat"; lat.nmeaFieldIndex = 2; lat.nmeaValueKind = 0; lat.sendValueText = "4807.038";
    m.fields << t << lat;

    QByteArray sentence; QStringList problems;
    const bool built = PayloadBuilder::buildNmeaSentence(m, sentence, problems);
    add("NMEA", "-", "Build an NMEA-0183 sentence with XOR checksum + CRLF",
        built && sentence.startsWith('$') && sentence.endsWith("\r\n") && sentence.contains('*'),
        built ? QString::fromLatin1(sentence).trimmed() : problems.join("; "));

    const NmeaDecoder::Result res = NmeaDecoder::decodePacket("GGA", sentence);
    const bool haveOne = (res.records.size() == 1);
    const NmeaDecodedRecord rec = haveOne ? res.records.at(0) : NmeaDecodedRecord();
    // Assert on the RAW token (valueAt() returns the registry-formatted value,
    // e.g. UTC "12:35:19"; rawValueAt() returns the token as transmitted).
    const bool decoded = haveOne && rec.checksumOk && rec.talker == "GP"
        && rec.rawValueAt(1) == "123519" && rec.rawValueAt(2) == "4807.038";
    add("NMEA", "-", "Decode the sentence back (checksum verified, fields recovered)",
        decoded, QString("records=%1 checksumOk=%2 talker=%3 f1=%4 f2=%5")
                     .arg(res.records.size()).arg(haveOne && rec.checksumOk)
                     .arg(rec.talker).arg(rec.rawValueAt(1)).arg(rec.rawValueAt(2)));
}

// ------------------------------------------------------------ validators
static void testValidators()
{
    double v = 0; QString err;
    const bool r1 = InputValidator::solveResolutionExpression("0.01", v, err) && qAbs(v - 0.01) < 1e-9;
    add("Validation", "-", "Resolution expression: decimal (0.01)", r1, QString("v=%1").arg(v));

    double v2 = 0; QString e2;
    const bool r2 = InputValidator::solveResolutionExpression("1/3", v2, e2) && qAbs(v2 - (1.0 / 3.0)) < 1e-6;
    add("Validation", "-", "Resolution expression: fraction (1/3)", r2, QString("v=%1").arg(v2, 0, 'g', 8));

    double v3 = 0; QString e3;
    const bool r3 = !InputValidator::solveResolutionExpression("abc/", v3, e3);
    add("Validation", "-", "Reject an invalid resolution expression with a reason", r3,
        r3 ? ("rejected: " + e3) : "wrongly accepted");

    QString fe;
    const bool good = InputValidator::validateField("Speed", "5", "2", "0.01", fe, 8);
    add("Validation", "-", "Accept a well-formed field definition", good, good ? "ok" : fe);

    QString fe2;
    const bool bad = !InputValidator::validateField("Bad", "0", "0", "1", fe2, 8);
    add("Validation", "-", "Reject an out-of-range field (offset/length 0)", bad,
        bad ? ("rejected: " + fe2) : "wrongly accepted");
}

// ------------------------------------------------------------ main
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QString root = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                              : QString("C:/GitHub/pcapfile");
    const QString icdDir = root + "/test_files";
    const QString outDir = root + "/test_files/output";
    QDir().mkpath(outDir);

    std::fprintf(stdout, "=== Universal Data Suite — functional test harness ===\n");
    std::fprintf(stdout, "root: %s\n\n", qPrintable(root));

    // ICD-driven
    testRadarBasic(icdDir);
    testAllTypes(icdDir);
    testScaled(icdDir);
    testWide(icdDir);
    testLengthMsgId(icdDir);
    testLenient(icdDir);
    testContinuation(icdDir);
    testWordOffsets(icdDir);

    // Codec / contract / capture / protocol / validation
    testJsonRoundTrip();
    testFieldsJsonInterchange();
    testExcelRoundTrip(outDir);
    testConnections();
    testPcapRoundTrip(outDir);
    testNmea();
    testValidators();

    // Summary + JSON out
    int passed = 0;
    for (int i = 0; i < g_recs.size(); ++i)
        if (g_recs.at(i).status == "PASS") ++passed;

    QJsonArray arr;
    for (int i = 0; i < g_recs.size(); ++i)
    {
        const Rec& r = g_recs.at(i);
        QJsonObject o;
        o["id"] = r.id; o["area"] = r.area; o["icd"] = r.icd;
        o["problem"] = r.problem; o["status"] = r.status; o["detail"] = r.detail;
        arr.append(o);
    }
    QJsonObject rootObj;
    rootObj["total"] = g_recs.size();
    rootObj["passed"] = passed;
    rootObj["failed"] = g_recs.size() - passed;
    rootObj["results"] = arr;

    const QString jsonPath = outDir + "/test_results.json";
    QFile jf(jsonPath);
    if (jf.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        jf.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        jf.close();
    }

    std::fprintf(stdout, "\n=== %d/%d checks passed, %d failed ===\n",
                 passed, g_recs.size(), g_recs.size() - passed);
    std::fprintf(stdout, "results: %s\n", qPrintable(jsonPath));
    return (passed == g_recs.size()) ? 0 : 1;
}
