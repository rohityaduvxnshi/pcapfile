#include "AsterixUapRegistry.h"

// v15: UAP tables for CAT021, CAT034, CAT048, CAT062. Sources:
//   - EUROCONTROL-SPEC-0149 Part 4  (CAT048)  ed. 1.27
//   - EUROCONTROL-SPEC-0149 Part 2b (CAT034)  ed. 1.29
//   - EUROCONTROL-SPEC-0149 Part 9  (CAT062)  ed. 1.20
//   - EUROCONTROL-SPEC-0149 Part 12 (CAT021)  ed. 2.4
//
// The tables intentionally cover the well-documented items in each category.
// Items whose internal structure (esp. large compounds in CAT062) is not fully
// transcribed here are still walked by the decoder using the sub-items listed;
// any FSPEC bit pointing at an item the table does not describe causes the
// decoder to stop walking the current record with a warning (preserving any
// already-decoded items earlier in FRN order).

#include <QMutex>
#include <QMutexLocker>

namespace
{
AsterixSubItem subItem(const QString& id, const QString& name,
                       int byteLength,
                       AsterixValueKind kind = AsterixValueKind::HexBytes)
{
    AsterixSubItem s;
    s.id          = id;
    s.defaultName = name;
    s.byteLength  = byteLength;
    s.valueKind   = kind;
    return s;
}

AsterixItemDef fixedItem(int frn, const QString& id, const QString& name,
                         int length,
                         AsterixValueKind kind = AsterixValueKind::HexBytes)
{
    AsterixItemDef d;
    d.frn         = frn;
    d.id          = id;
    d.defaultName = name;
    d.kind        = AsterixItemKind::Fixed;
    d.fixedLength = length;
    d.valueKind   = kind;
    return d;
}

AsterixItemDef extendedItem(int frn, const QString& id, const QString& name,
                            int extentLength)
{
    AsterixItemDef d;
    d.frn                  = frn;
    d.id                   = id;
    d.defaultName          = name;
    d.kind                 = AsterixItemKind::Extended;
    d.extendedExtentLength = extentLength;
    d.valueKind            = AsterixValueKind::HexBytes;
    return d;
}

AsterixItemDef repetitiveItem(int frn, const QString& id, const QString& name,
                              int elementLength)
{
    AsterixItemDef d;
    d.frn                     = frn;
    d.id                      = id;
    d.defaultName             = name;
    d.kind                    = AsterixItemKind::Repetitive;
    d.repetitiveElementLength = elementLength;
    d.valueKind               = AsterixValueKind::MultiPart;
    return d;
}

AsterixItemDef compoundItem(int frn, const QString& id, const QString& name,
                            const QList<AsterixSubItem>& subItems)
{
    AsterixItemDef d;
    d.frn              = frn;
    d.id               = id;
    d.defaultName      = name;
    d.kind             = AsterixItemKind::Compound;
    d.compoundSubItems = subItems;
    d.valueKind        = AsterixValueKind::MultiPart;
    return d;
}

AsterixItemDef explicitLengthItem(int frn, const QString& id, const QString& name)
{
    AsterixItemDef d;
    d.frn         = frn;
    d.id          = id;
    d.defaultName = name;
    d.kind        = AsterixItemKind::ExplicitLength;
    d.valueKind   = AsterixValueKind::HexBytes;
    return d;
}

AsterixItemDef placeholder(int frn, const QString& id, const QString& name)
{
    AsterixItemDef d;
    d.frn         = frn;
    d.id          = id;
    d.defaultName = name;
    d.kind        = AsterixItemKind::Unknown;
    return d;
}

AsterixCategoryDef buildCat048()
{
    AsterixCategoryDef c;
    c.category = 48;
    c.name     = "CAT048 Monoradar Target Reports";
    c.uap.reserve(28);

    c.uap << fixedItem    ( 1, "I048/010", "Data Source Identifier",                  2);
    c.uap << fixedItem    ( 2, "I048/140", "Time of Day",                             3, AsterixValueKind::TimeOfDay);
    c.uap << extendedItem ( 3, "I048/020", "Target Report Descriptor",                1);
    c.uap << fixedItem    ( 4, "I048/040", "Measured Position (Polar)",               4);
    c.uap << fixedItem    ( 5, "I048/070", "Mode-3/A Code (Octal)",                   2, AsterixValueKind::Mode3A);
    c.uap << fixedItem    ( 6, "I048/090", "Flight Level (binary)",                   2, AsterixValueKind::ModeC_FL);
    c.uap << compoundItem ( 7, "I048/130", "Radar Plot Characteristics",
                            QList<AsterixSubItem>()
                                << subItem("SRL", "SSR Plot Runlength",       1)
                                << subItem("SRR", "SSR Number of Replies",    1)
                                << subItem("SAM", "SSR Amplitude",            1, AsterixValueKind::IntBE)
                                << subItem("PRL", "PSR Plot Runlength",       1)
                                << subItem("PAM", "PSR Amplitude",            1, AsterixValueKind::IntBE)
                                << subItem("RPD", "Range Position Difference",1, AsterixValueKind::IntBE)
                                << subItem("APD", "Azimuth Position Difference",1, AsterixValueKind::IntBE));
    c.uap << fixedItem    ( 8, "I048/220", "Aircraft Address",                        3, AsterixValueKind::Address24bit);
    c.uap << fixedItem    ( 9, "I048/240", "Aircraft Identification",                 6, AsterixValueKind::Callsign6);
    c.uap << repetitiveItem(10, "I048/250", "Mode S MB Data",                         8);
    c.uap << fixedItem    (11, "I048/161", "Track Number",                            2, AsterixValueKind::UintBE);
    c.uap << fixedItem    (12, "I048/042", "Calculated Position (Cartesian)",         4);
    c.uap << fixedItem    (13, "I048/200", "Calculated Track Velocity (Polar)",       4);
    c.uap << extendedItem (14, "I048/170", "Track Status",                            1);
    c.uap << fixedItem    (15, "I048/210", "Track Quality",                           4);
    c.uap << extendedItem (16, "I048/030", "Warning/Error Conditions",                1);
    c.uap << fixedItem    (17, "I048/080", "Mode-3/A Code Confidence Indicator",      2);
    c.uap << fixedItem    (18, "I048/100", "Mode-C Code & Confidence Indicator",      4);
    c.uap << fixedItem    (19, "I048/110", "Height Measured by 3D Radar",             2);
    c.uap << compoundItem (20, "I048/120", "Radial Doppler Speed",
                            QList<AsterixSubItem>()
                                << subItem("CAL", "Calculated Doppler Speed",        2, AsterixValueKind::IntBE)
                                << subItem("RDS", "Raw Doppler Speed",               6));
    c.uap << fixedItem    (21, "I048/230", "Comm/ACAS Capability & Flight Status",    2);
    c.uap << fixedItem    (22, "I048/260", "ACAS Resolution Advisory Report",         7);
    c.uap << fixedItem    (23, "I048/055", "Mode-1 Code (Octal)",                     1);
    c.uap << fixedItem    (24, "I048/050", "Mode-2 Code (Octal)",                     2);
    c.uap << fixedItem    (25, "I048/065", "Mode-1 Code Confidence Indicator",        1);
    c.uap << fixedItem    (26, "I048/060", "Mode-2 Code Confidence Indicator",        2);
    c.uap << explicitLengthItem(27, "I048/SPF", "Special Purpose Field");
    c.uap << explicitLengthItem(28, "I048/RE",  "Reserved Expansion Field");

    return c;
}

AsterixCategoryDef buildCat034()
{
    AsterixCategoryDef c;
    c.category = 34;
    c.name     = "CAT034 Monoradar Service Messages";
    c.uap.reserve(14);

    c.uap << fixedItem    ( 1, "I034/010", "Data Source Identifier",                  2);
    c.uap << fixedItem    ( 2, "I034/000", "Message Type",                            1, AsterixValueKind::UintBE);
    c.uap << fixedItem    ( 3, "I034/030", "Time of Day",                             3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    ( 4, "I034/020", "Sector Number",                           1, AsterixValueKind::UintBE);
    c.uap << fixedItem    ( 5, "I034/041", "Antenna Rotation Speed",                  2);
    c.uap << compoundItem ( 6, "I034/050", "System Configuration and Status",
                            QList<AsterixSubItem>()
                                << subItem("COM", "Common Subfield",                 1)
                                << subItem("RSV", "Reserved",                        1)
                                << subItem("PSR", "PSR Sensor Subfield",             1)
                                << subItem("SSR", "SSR Sensor Subfield",             1)
                                << subItem("MDS", "Mode-S Sensor Subfield",          2));
    c.uap << compoundItem ( 7, "I034/060", "System Processing Mode",
                            QList<AsterixSubItem>()
                                << subItem("COM", "Common Subfield",                 1)
                                << subItem("RSV", "Reserved",                        1)
                                << subItem("PSR", "PSR Processing Subfield",         1)
                                << subItem("SSR", "SSR Processing Subfield",         1)
                                << subItem("MDS", "Mode-S Processing Subfield",      1));
    c.uap << repetitiveItem( 8, "I034/070", "Message Count Values",                   2);
    c.uap << fixedItem    ( 9, "I034/100", "Generic Polar Window",                    8);
    c.uap << fixedItem    (10, "I034/110", "Data Filter",                             1, AsterixValueKind::UintBE);
    c.uap << fixedItem    (11, "I034/120", "3D Position of Data Source",              8);
    c.uap << fixedItem    (12, "I034/090", "Collimation Error",                       2);
    c.uap << explicitLengthItem(13, "I034/RE", "Reserved Expansion Field");
    c.uap << explicitLengthItem(14, "I034/SP", "Special Purpose Field");

    return c;
}

AsterixCategoryDef buildCat021()
{
    AsterixCategoryDef c;
    c.category = 21;
    c.name     = "CAT021 ADS-B Target Reports";
    c.uap.reserve(48);

    c.uap << fixedItem    ( 1, "I021/010", "Data Source Identification",              2);
    c.uap << extendedItem ( 2, "I021/040", "Target Report Descriptor",                1);
    c.uap << fixedItem    ( 3, "I021/161", "Track Number",                            2, AsterixValueKind::UintBE);
    c.uap << fixedItem    ( 4, "I021/015", "Service Identification",                  1, AsterixValueKind::UintBE);
    c.uap << fixedItem    ( 5, "I021/071", "Time of Applicability for Position",      3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    ( 6, "I021/130", "Position in WGS-84 Coordinates",          6);
    c.uap << fixedItem    ( 7, "I021/131", "High-Res Position WGS-84",                8);
    c.uap << fixedItem    ( 8, "I021/072", "Time of Applicability for Velocity",      3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    ( 9, "I021/150", "Air Speed",                               2);
    c.uap << fixedItem    (10, "I021/151", "True Air Speed",                          2);
    c.uap << fixedItem    (11, "I021/080", "Target Address",                          3, AsterixValueKind::Address24bit);
    c.uap << fixedItem    (12, "I021/073", "Time of Message Reception of Position",   3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    (13, "I021/074", "Time of Reception of Position (HP)",      4);
    c.uap << fixedItem    (14, "I021/075", "Time of Message Reception of Velocity",   3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    (15, "I021/076", "Time of Reception of Velocity (HP)",      4);
    c.uap << fixedItem    (16, "I021/140", "Geometric Height",                        2, AsterixValueKind::IntBE);
    c.uap << extendedItem (17, "I021/090", "Quality Indicators",                      1);
    c.uap << fixedItem    (18, "I021/210", "MOPS Version",                            1);
    c.uap << fixedItem    (19, "I021/070", "Mode-3/A Code",                           2, AsterixValueKind::Mode3A);
    c.uap << fixedItem    (20, "I021/230", "Roll Angle",                              2, AsterixValueKind::IntBE);
    c.uap << fixedItem    (21, "I021/145", "Flight Level",                            2, AsterixValueKind::ModeC_FL);
    c.uap << fixedItem    (22, "I021/152", "Magnetic Heading",                        2, AsterixValueKind::UintBE);
    c.uap << fixedItem    (23, "I021/200", "Target Status",                           1);
    c.uap << fixedItem    (24, "I021/155", "Barometric Vertical Rate",                2, AsterixValueKind::IntBE);
    c.uap << fixedItem    (25, "I021/157", "Geometric Vertical Rate",                 2, AsterixValueKind::IntBE);
    c.uap << fixedItem    (26, "I021/160", "Airborne Ground Vector",                  4);
    c.uap << fixedItem    (27, "I021/165", "Track Angle Rate",                        2, AsterixValueKind::IntBE);
    c.uap << fixedItem    (28, "I021/077", "Time of Report Transmission",             3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    (29, "I021/170", "Target Identification",                   6, AsterixValueKind::Callsign6);
    c.uap << fixedItem    (30, "I021/020", "Emitter Category",                        1, AsterixValueKind::UintBE);
    c.uap << compoundItem (31, "I021/220", "Met Information",
                            QList<AsterixSubItem>()
                                << subItem("WS",  "Wind Speed",                      2)
                                << subItem("WD",  "Wind Direction",                  2)
                                << subItem("TMP", "Temperature",                     2, AsterixValueKind::IntBE)
                                << subItem("TRB", "Turbulence",                      1));
    c.uap << fixedItem    (32, "I021/146", "Selected Altitude",                       2);
    c.uap << fixedItem    (33, "I021/148", "Final State Selected Altitude",          2);
    c.uap << compoundItem (34, "I021/110", "Trajectory Intent",
                            QList<AsterixSubItem>()
                                << subItem("TIS", "Trajectory Intent Status",        1)
                                << subItem("TID", "Trajectory Intent Data",          15));
    c.uap << fixedItem    (35, "I021/016", "Service Management",                      1);
    c.uap << fixedItem    (36, "I021/008", "Aircraft Operational Status",             1);
    c.uap << extendedItem (37, "I021/271", "Surface Capabilities and Characteristics",1);
    c.uap << fixedItem    (38, "I021/132", "Message Amplitude",                       1, AsterixValueKind::IntBE);
    c.uap << repetitiveItem(39, "I021/250", "Mode S MB Data",                         8);
    c.uap << fixedItem    (40, "I021/260", "ACAS Resolution Advisory Report",         7);
    c.uap << fixedItem    (41, "I021/400", "Receiver ID",                             1, AsterixValueKind::UintBE);
    c.uap << compoundItem (42, "I021/295", "Data Ages",
                            QList<AsterixSubItem>()
                                << subItem("AOS", "Aircraft Operational Status Age", 1)
                                << subItem("TRD", "Target Report Descriptor Age",    1)
                                << subItem("M3A", "Mode-3/A Age",                    1)
                                << subItem("QI",  "Quality Indicator Age",           1)
                                << subItem("TI",  "Trajectory Intent Age",           1)
                                << subItem("MAM", "Message Amplitude Age",           1)
                                << subItem("GH",  "Geometric Height Age",            1));
    c.uap << placeholder  (43, "I021/spare1", "spare");
    c.uap << placeholder  (44, "I021/spare2", "spare");
    c.uap << placeholder  (45, "I021/spare3", "spare");
    c.uap << placeholder  (46, "I021/spare4", "spare");
    c.uap << explicitLengthItem(47, "I021/RE", "Reserved Expansion Field");
    c.uap << explicitLengthItem(48, "I021/SP", "Special Purpose Field");

    return c;
}

AsterixCategoryDef buildCat062()
{
    AsterixCategoryDef c;
    c.category = 62;
    c.name     = "CAT062 System Track Data";
    c.uap.reserve(35);

    c.uap << fixedItem    ( 1, "I062/010", "Data Source Identifier",                  2);
    c.uap << placeholder  ( 2, "I062/spare1", "spare");
    c.uap << fixedItem    ( 3, "I062/015", "Service Identification",                  1, AsterixValueKind::UintBE);
    c.uap << fixedItem    ( 4, "I062/070", "Time of Track Information",               3, AsterixValueKind::TimeOfDay);
    c.uap << fixedItem    ( 5, "I062/105", "Calculated Track Position (WGS-84)",      8);
    c.uap << fixedItem    ( 6, "I062/100", "Calculated Track Position (Cartesian)",   6);
    c.uap << fixedItem    ( 7, "I062/185", "Calculated Track Velocity (Cartesian)",   4);
    c.uap << fixedItem    ( 8, "I062/210", "Calculated Acceleration (Cartesian)",     2);
    c.uap << fixedItem    ( 9, "I062/060", "Track Mode 3/A Code",                     2, AsterixValueKind::Mode3A);
    c.uap << fixedItem    (10, "I062/245", "Target Identification",                   7);
    c.uap << compoundItem (11, "I062/380", "Aircraft Derived Data",
                            QList<AsterixSubItem>()
                                << subItem("ADR", "Target Address",                  3, AsterixValueKind::Address24bit)
                                << subItem("ID",  "Target Identification",           6, AsterixValueKind::Callsign6)
                                << subItem("MHG", "Magnetic Heading",                2)
                                << subItem("IAS", "Indicated Airspeed",              2)
                                << subItem("TAS", "True Airspeed",                   2)
                                << subItem("SAL", "Selected Altitude",               2)
                                << subItem("FNAL","Final State Selected Altitude",   2));
    c.uap << fixedItem    (12, "I062/040", "Track Number",                            2, AsterixValueKind::UintBE);
    c.uap << extendedItem (13, "I062/080", "Track Status",                            1);
    c.uap << compoundItem (14, "I062/290", "System Track Update Ages",
                            QList<AsterixSubItem>()
                                << subItem("TRK", "Track Age",                       1)
                                << subItem("PSR", "PSR Age",                         1)
                                << subItem("SSR", "SSR Age",                         1)
                                << subItem("MDS", "Mode-S Age",                      1)
                                << subItem("ADS", "ADS-C Age",                       2)
                                << subItem("ES",  "ES Age",                          1)
                                << subItem("VDL", "VDL Age",                         1));
    c.uap << fixedItem    (15, "I062/200", "Mode of Movement",                        1);
    c.uap << compoundItem (16, "I062/295", "Track Data Ages",
                            QList<AsterixSubItem>()
                                << subItem("MFL", "Measured Flight Level Age",       1)
                                << subItem("MD1", "Mode-1 Age",                      1)
                                << subItem("MD2", "Mode-2 Age",                      1)
                                << subItem("MDA", "Mode-3/A Age",                    1)
                                << subItem("MD4", "Mode-4 Age",                      1)
                                << subItem("MD5", "Mode-5 Age",                      1));
    c.uap << fixedItem    (17, "I062/136", "Measured Flight Level",                   2, AsterixValueKind::ModeC_FL);
    c.uap << fixedItem    (18, "I062/130", "Calculated Track Geometric Altitude",     2, AsterixValueKind::IntBE);
    c.uap << fixedItem    (19, "I062/135", "Calculated Track Barometric Altitude",    2, AsterixValueKind::IntBE);
    c.uap << fixedItem    (20, "I062/220", "Calculated Rate of Climb/Descent",        2, AsterixValueKind::IntBE);
    c.uap << compoundItem (21, "I062/390", "Flight Plan Related Data",
                            QList<AsterixSubItem>()
                                << subItem("TAG", "FPPS Identification Tag",         2)
                                << subItem("CSN", "Callsign",                        7)
                                << subItem("IFI", "IFPS_FLIGHT_ID",                  4)
                                << subItem("FCT", "Flight Category",                 1)
                                << subItem("TAC", "Type of Aircraft",                4)
                                << subItem("WTC", "Wake Turbulence Category",        1));
    c.uap << extendedItem (22, "I062/270", "Target Size and Orientation",             1);
    c.uap << fixedItem    (23, "I062/300", "Vehicle Fleet Identification",            1);
    c.uap << compoundItem (24, "I062/110", "Mode 5 Data Reports & Extended Mode 1",
                            QList<AsterixSubItem>()
                                << subItem("SUM", "Mode 5 Summary",                  1)
                                << subItem("PMN", "Mode 5 PIN, NAT, MIS",            4)
                                << subItem("POS", "Mode 5 Position",                 6)
                                << subItem("GA",  "Mode 5 GNSS Altitude",            2)
                                << subItem("EM1", "Extended Mode 1 Code",            2)
                                << subItem("TOS", "Time Offset",                     1)
                                << subItem("XP",  "X Pulse Presence",                1));
    c.uap << fixedItem    (25, "I062/120", "Track Mode 2 Code",                       2);
    c.uap << extendedItem (26, "I062/510", "Composed Track Number",                   3);
    c.uap << compoundItem (27, "I062/500", "Estimated Accuracies",
                            QList<AsterixSubItem>()
                                << subItem("APC", "Estimated Accuracy Pos (Cart)",   2)
                                << subItem("COV", "Covariance",                      2)
                                << subItem("APW", "Estimated Accuracy Pos (WGS84)",  4)
                                << subItem("AGA", "Estimated Accuracy Geo Alt",      1)
                                << subItem("ABA", "Estimated Accuracy Baro Alt",     1)
                                << subItem("ATV", "Estimated Accuracy Track Vel",    2)
                                << subItem("AA",  "Estimated Accuracy Accel",        2));
    c.uap << compoundItem (28, "I062/340", "Measured Information",
                            QList<AsterixSubItem>()
                                << subItem("SID", "Sensor Identification",           2)
                                << subItem("POS", "Measured Position",               4)
                                << subItem("HEI", "Measured 3-D Height",             2)
                                << subItem("MDC", "Measured Mode-C Code",            2)
                                << subItem("MDA", "Measured Mode-3/A Code",          2)
                                << subItem("TYP", "Report Type",                     1));
    c.uap << placeholder  (29, "I062/spare2", "spare");
    c.uap << placeholder  (30, "I062/spare3", "spare");
    c.uap << placeholder  (31, "I062/spare4", "spare");
    c.uap << placeholder  (32, "I062/spare5", "spare");
    c.uap << placeholder  (33, "I062/spare6", "spare");
    c.uap << explicitLengthItem(34, "I062/RE", "Reserved Expansion Field");
    c.uap << explicitLengthItem(35, "I062/SP", "Special Purpose Field");

    return c;
}

// Singleton storage. Built once on first access (guarded by a mutex so static
// init is safe across threads even though typical use is single-threaded).
struct UapStore
{
    AsterixCategoryDef cat021;
    AsterixCategoryDef cat034;
    AsterixCategoryDef cat048;
    AsterixCategoryDef cat062;
    bool initialized;

    UapStore() : initialized(false) {}
};

UapStore& store()
{
    static UapStore s;
    static QMutex m;
    QMutexLocker lock(&m);
    if (!s.initialized)
    {
        s.cat021 = buildCat021();
        s.cat034 = buildCat034();
        s.cat048 = buildCat048();
        s.cat062 = buildCat062();
        s.initialized = true;
    }
    return s;
}
}

const AsterixCategoryDef* AsterixUapRegistry::lookup(int category)
{
    UapStore& s = store();
    switch (category)
    {
    case 21: return &s.cat021;
    case 34: return &s.cat034;
    case 48: return &s.cat048;
    case 62: return &s.cat062;
    }
    return 0;
}

QList<int> AsterixUapRegistry::supportedCategories()
{
    QList<int> out;
    out << 21 << 34 << 48 << 62;
    return out;
}

QString AsterixUapRegistry::categoryDisplayName(int category)
{
    const AsterixCategoryDef* def = lookup(category);
    if (!def) return QString();
    return def->name;
}
