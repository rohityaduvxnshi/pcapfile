#include "NmeaSentenceRegistry.h"

#include <QHash>

namespace
{

typedef NmeaValueKind VK;

// Append a field to the sentence being built.
void add(NmeaSentenceDef& def, const QString& name, int index, VK kind)
{
    def.fields.append(NmeaFieldDef(name, index, kind));
}

NmeaSentenceDef makeGGA()
{
    NmeaSentenceDef d;
    d.formatter = "GGA";
    d.displayName = "GGA - Global Positioning System Fix Data";
    add(d, "UTC", 1, VK::Time);
    add(d, "Latitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "Longitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "FixQuality", 6, VK::Numeric);
    add(d, "NumSatellites", 7, VK::Numeric);
    add(d, "HDOP", 8, VK::Numeric);
    add(d, "Altitude", 9, VK::Numeric);
    add(d, "AltitudeUnit", 10, VK::Char);
    add(d, "GeoidSeparation", 11, VK::Numeric);
    add(d, "GeoidSeparationUnit", 12, VK::Char);
    add(d, "DiffAge", 13, VK::Numeric);
    add(d, "DiffStationId", 14, VK::Text);
    return d;
}

NmeaSentenceDef makeGLL()
{
    NmeaSentenceDef d;
    d.formatter = "GLL";
    d.displayName = "GLL - Geographic Position, Latitude/Longitude";
    add(d, "Latitude", 1, VK::Latitude);
    add(d, "NS", 2, VK::Char);
    add(d, "Longitude", 3, VK::Longitude);
    add(d, "EW", 4, VK::Char);
    add(d, "UTC", 5, VK::Time);
    add(d, "Status", 6, VK::Status);
    add(d, "ModeIndicator", 7, VK::Char);
    return d;
}

NmeaSentenceDef makeRMC()
{
    NmeaSentenceDef d;
    d.formatter = "RMC";
    d.displayName = "RMC - Recommended Minimum Specific GNSS Data";
    add(d, "UTC", 1, VK::Time);
    add(d, "Status", 2, VK::Status);
    add(d, "Latitude", 3, VK::Latitude);
    add(d, "NS", 4, VK::Char);
    add(d, "Longitude", 5, VK::Longitude);
    add(d, "EW", 6, VK::Char);
    add(d, "SpeedOverGroundKnots", 7, VK::Numeric);
    add(d, "CourseOverGround", 8, VK::Numeric);
    add(d, "Date", 9, VK::Date);
    add(d, "MagneticVariation", 10, VK::Numeric);
    add(d, "MagneticVariationEW", 11, VK::Char);
    add(d, "ModeIndicator", 12, VK::Char);
    return d;
}

NmeaSentenceDef makeVTG()
{
    NmeaSentenceDef d;
    d.formatter = "VTG";
    d.displayName = "VTG - Course Over Ground and Ground Speed";
    add(d, "CourseTrue", 1, VK::Numeric);
    add(d, "CourseTrueRef", 2, VK::Char);
    add(d, "CourseMagnetic", 3, VK::Numeric);
    add(d, "CourseMagneticRef", 4, VK::Char);
    add(d, "SpeedKnots", 5, VK::Numeric);
    add(d, "SpeedKnotsUnit", 6, VK::Char);
    add(d, "SpeedKmh", 7, VK::Numeric);
    add(d, "SpeedKmhUnit", 8, VK::Char);
    add(d, "ModeIndicator", 9, VK::Char);
    return d;
}

NmeaSentenceDef makeGSA()
{
    NmeaSentenceDef d;
    d.formatter = "GSA";
    d.displayName = "GSA - GNSS DOP and Active Satellites";
    add(d, "Mode", 1, VK::Char);
    add(d, "FixType", 2, VK::Numeric);
    for (int s = 0; s < 12; ++s)
        add(d, QString("SatelliteId%1").arg(s + 1), 3 + s, VK::Numeric);
    add(d, "PDOP", 15, VK::Numeric);
    add(d, "HDOP", 16, VK::Numeric);
    add(d, "VDOP", 17, VK::Numeric);
    return d;
}

NmeaSentenceDef makeGSV()
{
    NmeaSentenceDef d;
    d.formatter = "GSV";
    d.displayName = "GSV - GNSS Satellites in View";
    add(d, "NumMessages", 1, VK::Numeric);
    add(d, "MessageNumber", 2, VK::Numeric);
    add(d, "SatellitesInView", 3, VK::Numeric);
    // Up to 4 satellite blocks of 4 fields each.
    for (int s = 0; s < 4; ++s)
    {
        const int base = 4 + s * 4;
        add(d, QString("Sat%1_Id").arg(s + 1), base + 0, VK::Numeric);
        add(d, QString("Sat%1_Elevation").arg(s + 1), base + 1, VK::Numeric);
        add(d, QString("Sat%1_Azimuth").arg(s + 1), base + 2, VK::Numeric);
        add(d, QString("Sat%1_SNR").arg(s + 1), base + 3, VK::Numeric);
    }
    return d;
}

NmeaSentenceDef makeZDA()
{
    NmeaSentenceDef d;
    d.formatter = "ZDA";
    d.displayName = "ZDA - Time and Date";
    add(d, "UTC", 1, VK::Time);
    add(d, "Day", 2, VK::Numeric);
    add(d, "Month", 3, VK::Numeric);
    add(d, "Year", 4, VK::Numeric);
    add(d, "LocalZoneHours", 5, VK::Numeric);
    add(d, "LocalZoneMinutes", 6, VK::Numeric);
    return d;
}

NmeaSentenceDef makeGST()
{
    NmeaSentenceDef d;
    d.formatter = "GST";
    d.displayName = "GST - GNSS Pseudorange Error Statistics";
    add(d, "UTC", 1, VK::Time);
    add(d, "RmsRange", 2, VK::Numeric);
    add(d, "StdDevMajor", 3, VK::Numeric);
    add(d, "StdDevMinor", 4, VK::Numeric);
    add(d, "Orientation", 5, VK::Numeric);
    add(d, "StdDevLatitude", 6, VK::Numeric);
    add(d, "StdDevLongitude", 7, VK::Numeric);
    add(d, "StdDevAltitude", 8, VK::Numeric);
    return d;
}

NmeaSentenceDef makeGNS()
{
    NmeaSentenceDef d;
    d.formatter = "GNS";
    d.displayName = "GNS - GNSS Fix Data";
    add(d, "UTC", 1, VK::Time);
    add(d, "Latitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "Longitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "ModeIndicator", 6, VK::Text);
    add(d, "NumSatellites", 7, VK::Numeric);
    add(d, "HDOP", 8, VK::Numeric);
    add(d, "Altitude", 9, VK::Numeric);
    add(d, "GeoidSeparation", 10, VK::Numeric);
    add(d, "DiffAge", 11, VK::Numeric);
    add(d, "DiffStationId", 12, VK::Text);
    return d;
}

NmeaSentenceDef makeHDT()
{
    NmeaSentenceDef d;
    d.formatter = "HDT";
    d.displayName = "HDT - Heading, True";
    add(d, "HeadingTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    return d;
}

NmeaSentenceDef makeVHW()
{
    NmeaSentenceDef d;
    d.formatter = "VHW";
    d.displayName = "VHW - Water Speed and Heading";
    add(d, "HeadingTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    add(d, "HeadingMagnetic", 3, VK::Numeric);
    add(d, "MagneticRef", 4, VK::Char);
    add(d, "SpeedKnots", 5, VK::Numeric);
    add(d, "SpeedKnotsUnit", 6, VK::Char);
    add(d, "SpeedKmh", 7, VK::Numeric);
    add(d, "SpeedKmhUnit", 8, VK::Char);
    return d;
}

NmeaSentenceDef makeDBT()
{
    NmeaSentenceDef d;
    d.formatter = "DBT";
    d.displayName = "DBT - Depth Below Transducer";
    add(d, "DepthFeet", 1, VK::Numeric);
    add(d, "FeetUnit", 2, VK::Char);
    add(d, "DepthMetres", 3, VK::Numeric);
    add(d, "MetresUnit", 4, VK::Char);
    add(d, "DepthFathoms", 5, VK::Numeric);
    add(d, "FathomsUnit", 6, VK::Char);
    return d;
}

NmeaSentenceDef makeDPT()
{
    NmeaSentenceDef d;
    d.formatter = "DPT";
    d.displayName = "DPT - Depth";
    add(d, "WaterDepth", 1, VK::Numeric);
    add(d, "TransducerOffset", 2, VK::Numeric);
    add(d, "MaxRangeScale", 3, VK::Numeric);
    return d;
}

NmeaSentenceDef makeMWV()
{
    NmeaSentenceDef d;
    d.formatter = "MWV";
    d.displayName = "MWV - Wind Speed and Angle";
    add(d, "WindAngle", 1, VK::Numeric);
    add(d, "Reference", 2, VK::Char);
    add(d, "WindSpeed", 3, VK::Numeric);
    add(d, "WindSpeedUnit", 4, VK::Char);
    add(d, "Status", 5, VK::Status);
    return d;
}

// Lazily-built catalogue, in display order, plus a formatter->index map.
struct Catalogue
{
    QList<NmeaSentenceDef> defs;
    QHash<QString, int>    indexByFormatter;

    Catalogue()
    {
        defs << makeGGA() << makeGLL() << makeRMC() << makeVTG()
             << makeGSA() << makeGSV() << makeZDA() << makeGST()
             << makeGNS() << makeHDT() << makeVHW() << makeDBT()
             << makeDPT() << makeMWV();
        for (int i = 0; i < defs.size(); ++i)
            indexByFormatter.insert(defs.at(i).formatter.toUpper(), i);
    }
};

const Catalogue& catalogue()
{
    static const Catalogue c;
    return c;
}

} // namespace

const NmeaSentenceDef* NmeaSentenceRegistry::lookup(const QString& formatter)
{
    const Catalogue& c = catalogue();
    const int idx = c.indexByFormatter.value(formatter.trimmed().toUpper(), -1);
    if (idx < 0)
        return 0;
    return &c.defs.at(idx);
}

QList<QString> NmeaSentenceRegistry::supportedFormatters()
{
    const Catalogue& c = catalogue();
    QList<QString> out;
    for (int i = 0; i < c.defs.size(); ++i)
        out << c.defs.at(i).formatter;
    return out;
}

QString NmeaSentenceRegistry::displayName(const QString& formatter)
{
    const NmeaSentenceDef* def = lookup(formatter);
    return def ? def->displayName : QString();
}
