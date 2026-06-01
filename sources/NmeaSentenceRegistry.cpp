#include "NmeaSentenceRegistry.h"

#include <QHash>

// NOTE: This catalogue is generated from the NMEA 0183 v3.01 approved
// parametric sentence templates (section 6.3). Common GNSS / navigation
// sentences carry curated field names; the remaining sentences carry
// type-correct names derived from the template field symbols. Field kinds
// drive CSV formatting (lat/lon/time/date). Extend by appending a sentence.

namespace
{

typedef NmeaValueKind VK;

void add(NmeaSentenceDef& d, const QString& name, int index, VK kind)
{
    d.fields.append(NmeaFieldDef(name, index, kind));
}

NmeaSentenceDef makeAAM()
{
    NmeaSentenceDef d;
    d.formatter = "AAM";
    d.displayName = "AAM - Waypoint Arrival Alarm";
    add(d, "ArrivalCircleEntered", 1, VK::Status);
    add(d, "PerpendicularPassed", 2, VK::Status);
    add(d, "ArrivalCircleRadius", 3, VK::Numeric);
    add(d, "RadiusUnit", 4, VK::Char);
    add(d, "WaypointId", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeABK()
{
    NmeaSentenceDef d;
    d.formatter = "ABK";
    d.displayName = "ABK - UAIS Addressed and binary broadcast acknowledgement";
    add(d, "Field1", 1, VK::Text);
    add(d, "Indicator", 2, VK::Char);
    add(d, "Value", 3, VK::Numeric);
    add(d, "Field2", 4, VK::Numeric);
    add(d, "Field3", 5, VK::Numeric);
    return d;
}

NmeaSentenceDef makeACA()
{
    NmeaSentenceDef d;
    d.formatter = "ACA";
    d.displayName = "ACA - UAIS Regional Channel Assignment Message";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Latitude1", 2, VK::Latitude);
    add(d, "NS1", 3, VK::Char);
    add(d, "Longitude1", 4, VK::Longitude);
    add(d, "EW1", 5, VK::Char);
    add(d, "Latitude2", 6, VK::Latitude);
    add(d, "NS2", 7, VK::Char);
    add(d, "Longitude2", 8, VK::Longitude);
    add(d, "EW2", 9, VK::Char);
    add(d, "Field2", 10, VK::Numeric);
    add(d, "Field3", 11, VK::Numeric);
    add(d, "Field4", 12, VK::Numeric);
    add(d, "Field5", 13, VK::Numeric);
    add(d, "Field6", 14, VK::Numeric);
    add(d, "Field7", 15, VK::Numeric);
    add(d, "Field8", 16, VK::Numeric);
    add(d, "Indicator", 17, VK::Char);
    add(d, "Field9", 18, VK::Numeric);
    add(d, "UTC", 19, VK::Time);
    return d;
}

NmeaSentenceDef makeACK()
{
    NmeaSentenceDef d;
    d.formatter = "ACK";
    d.displayName = "ACK - Acknowledge Alarm";
    add(d, "Field", 1, VK::Numeric);
    return d;
}

NmeaSentenceDef makeACS()
{
    NmeaSentenceDef d;
    d.formatter = "ACS";
    d.displayName = "ACS - UAIS Channel management information Source";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Text);
    add(d, "UTC", 3, VK::Time);
    add(d, "Field3", 4, VK::Numeric);
    add(d, "Field4", 5, VK::Numeric);
    add(d, "Field5", 6, VK::Numeric);
    return d;
}

NmeaSentenceDef makeAIR()
{
    NmeaSentenceDef d;
    d.formatter = "AIR";
    d.displayName = "AIR - UAIS Interrogation Request";
    add(d, "Field1", 1, VK::Text);
    add(d, "Value1", 2, VK::Numeric);
    add(d, "Field2", 3, VK::Numeric);
    add(d, "Value2", 4, VK::Numeric);
    add(d, "Field3", 5, VK::Numeric);
    add(d, "Field4", 6, VK::Text);
    add(d, "Value3", 7, VK::Numeric);
    add(d, "Field5", 8, VK::Numeric);
    return d;
}

NmeaSentenceDef makeALM()
{
    NmeaSentenceDef d;
    d.formatter = "ALM";
    d.displayName = "ALM - GPS Almanac Data";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Field", 3, VK::Numeric);
    add(d, "Value3", 4, VK::Numeric);
    add(d, "Hex1", 5, VK::Text);
    add(d, "Hex2", 6, VK::Text);
    add(d, "Hex3", 7, VK::Text);
    add(d, "Hex4", 8, VK::Text);
    add(d, "Hex5", 9, VK::Text);
    add(d, "Hex6", 10, VK::Text);
    add(d, "Hex7", 11, VK::Text);
    add(d, "Hex8", 12, VK::Text);
    add(d, "Hex9", 13, VK::Text);
    add(d, "Hex10", 14, VK::Text);
    add(d, "Hex11", 15, VK::Text);
    return d;
}

NmeaSentenceDef makeALR()
{
    NmeaSentenceDef d;
    d.formatter = "ALR";
    d.displayName = "ALR - Set Alarm State";
    add(d, "UTC", 1, VK::Time);
    add(d, "Field", 2, VK::Numeric);
    add(d, "Status1", 3, VK::Status);
    add(d, "Status2", 4, VK::Status);
    add(d, "Text", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeAPB()
{
    NmeaSentenceDef d;
    d.formatter = "APB";
    d.displayName = "APB - Heading/Track Controller (Autopilot) Sentence \"B\"";
    add(d, "Status1", 1, VK::Status);
    add(d, "Status2", 2, VK::Status);
    add(d, "CrossTrackError", 3, VK::Numeric);
    add(d, "DirectionToSteer", 4, VK::Char);
    add(d, "XteUnit", 5, VK::Char);
    add(d, "ArrivalCircleEntered", 6, VK::Status);
    add(d, "PerpendicularPassed", 7, VK::Status);
    add(d, "BearingOriginToDest", 8, VK::Numeric);
    add(d, "BearingOriginToDestRef", 9, VK::Char);
    add(d, "DestinationWaypointId", 10, VK::Text);
    add(d, "BearingPresentToDest", 11, VK::Numeric);
    add(d, "BearingPresentToDestRef", 12, VK::Char);
    add(d, "HeadingToSteer", 13, VK::Numeric);
    add(d, "HeadingToSteerRef", 14, VK::Char);
    add(d, "ModeIndicator", 15, VK::Char);
    return d;
}

NmeaSentenceDef makeBEC()
{
    NmeaSentenceDef d;
    d.formatter = "BEC";
    d.displayName = "BEC - Bearing & Distance to Waypoint - Dead Reckoning";
    add(d, "UTC", 1, VK::Time);
    add(d, "Latitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "Longitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "Value1", 6, VK::Numeric);
    add(d, "True", 7, VK::Char);
    add(d, "Value2", 8, VK::Numeric);
    add(d, "Magnetic", 9, VK::Char);
    add(d, "Value3", 10, VK::Numeric);
    add(d, "Knots", 11, VK::Char);
    add(d, "Text", 12, VK::Text);
    return d;
}

NmeaSentenceDef makeBOD()
{
    NmeaSentenceDef d;
    d.formatter = "BOD";
    d.displayName = "BOD - Bearing - Origin to Destination";
    add(d, "BearingTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    add(d, "BearingMagnetic", 3, VK::Numeric);
    add(d, "MagneticRef", 4, VK::Char);
    add(d, "DestinationWaypointId", 5, VK::Text);
    add(d, "OriginWaypointId", 6, VK::Text);
    return d;
}

NmeaSentenceDef makeBWC()
{
    NmeaSentenceDef d;
    d.formatter = "BWC";
    d.displayName = "BWC - Bearing & Distance to Waypoint";
    add(d, "UTC", 1, VK::Time);
    add(d, "WaypointLatitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "WaypointLongitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "BearingTrue", 6, VK::Numeric);
    add(d, "TrueRef", 7, VK::Char);
    add(d, "BearingMagnetic", 8, VK::Numeric);
    add(d, "MagneticRef", 9, VK::Char);
    add(d, "Distance", 10, VK::Numeric);
    add(d, "DistanceUnit", 11, VK::Char);
    add(d, "WaypointId", 12, VK::Text);
    add(d, "ModeIndicator", 13, VK::Char);
    return d;
}

NmeaSentenceDef makeBWR()
{
    NmeaSentenceDef d;
    d.formatter = "BWR";
    d.displayName = "BWR - Bearing & Distance to Waypoint - Rhumb Line";
    add(d, "UTC", 1, VK::Time);
    add(d, "WaypointLatitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "WaypointLongitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "BearingTrue", 6, VK::Numeric);
    add(d, "TrueRef", 7, VK::Char);
    add(d, "BearingMagnetic", 8, VK::Numeric);
    add(d, "MagneticRef", 9, VK::Char);
    add(d, "Distance", 10, VK::Numeric);
    add(d, "DistanceUnit", 11, VK::Char);
    add(d, "WaypointId", 12, VK::Text);
    add(d, "ModeIndicator", 13, VK::Char);
    return d;
}

NmeaSentenceDef makeBWW()
{
    NmeaSentenceDef d;
    d.formatter = "BWW";
    d.displayName = "BWW - Bearing - Waypoint to Waypoint";
    add(d, "BearingTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    add(d, "BearingMagnetic", 3, VK::Numeric);
    add(d, "MagneticRef", 4, VK::Char);
    add(d, "ToWaypointId", 5, VK::Text);
    add(d, "FromWaypointId", 6, VK::Text);
    return d;
}

NmeaSentenceDef makeCUR()
{
    NmeaSentenceDef d;
    d.formatter = "CUR";
    d.displayName = "CUR - Water Current Layer";
    add(d, "Status", 1, VK::Status);
    add(d, "Field", 2, VK::Numeric);
    add(d, "Value1", 3, VK::Numeric);
    add(d, "Value2", 4, VK::Numeric);
    add(d, "Value3", 5, VK::Numeric);
    add(d, "Indicator1", 6, VK::Char);
    add(d, "Value4", 7, VK::Numeric);
    add(d, "Value5", 8, VK::Numeric);
    add(d, "Value6", 9, VK::Numeric);
    add(d, "Indicator2", 10, VK::Char);
    add(d, "Indicator3", 11, VK::Char);
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

NmeaSentenceDef makeDCN()
{
    NmeaSentenceDef d;
    d.formatter = "DCN";
    d.displayName = "DCN - Decca Position";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Text1", 2, VK::Text);
    add(d, "Value1", 3, VK::Numeric);
    add(d, "Status1", 4, VK::Status);
    add(d, "Text2", 5, VK::Text);
    add(d, "Value2", 6, VK::Numeric);
    add(d, "Status2", 7, VK::Status);
    add(d, "Text3", 8, VK::Text);
    add(d, "Value3", 9, VK::Numeric);
    add(d, "Status3", 10, VK::Status);
    add(d, "Status4", 11, VK::Status);
    add(d, "Status5", 12, VK::Status);
    add(d, "Status6", 13, VK::Status);
    add(d, "Value4", 14, VK::Numeric);
    add(d, "Knots", 15, VK::Char);
    add(d, "Field2", 16, VK::Numeric);
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

NmeaSentenceDef makeDSC()
{
    NmeaSentenceDef d;
    d.formatter = "DSC";
    d.displayName = "DSC - Digital Selective Calling Information";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Text);
    add(d, "Field3", 3, VK::Numeric);
    add(d, "Field4", 4, VK::Numeric);
    add(d, "Field5", 5, VK::Numeric);
    add(d, "Value1", 6, VK::Numeric);
    add(d, "Value2", 7, VK::Numeric);
    add(d, "Field6", 8, VK::Text);
    add(d, "Field7", 9, VK::Numeric);
    add(d, "Indicator1", 10, VK::Char);
    add(d, "Indicator2", 11, VK::Char);
    return d;
}

NmeaSentenceDef makeDSE()
{
    NmeaSentenceDef d;
    d.formatter = "DSE";
    d.displayName = "DSE - Expanded Digital Selective Calling";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Numeric);
    add(d, "Indicator", 3, VK::Char);
    add(d, "Field3", 4, VK::Text);
    add(d, "Field4", 5, VK::Numeric);
    add(d, "Text1", 6, VK::Text);
    add(d, "Field5", 7, VK::Numeric);
    add(d, "Text2", 8, VK::Text);
    return d;
}

NmeaSentenceDef makeDSI()
{
    NmeaSentenceDef d;
    d.formatter = "DSI";
    d.displayName = "DSI - DSC Transponder Initialize";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Numeric);
    add(d, "Field3", 3, VK::Text);
    add(d, "Field4", 4, VK::Numeric);
    add(d, "Field5", 5, VK::Numeric);
    add(d, "Field6", 6, VK::Text);
    add(d, "Field7", 7, VK::Numeric);
    add(d, "Text1", 8, VK::Text);
    add(d, "Field8", 9, VK::Numeric);
    add(d, "Text2", 10, VK::Text);
    add(d, "Indicator", 11, VK::Char);
    return d;
}

NmeaSentenceDef makeDSR()
{
    NmeaSentenceDef d;
    d.formatter = "DSR";
    d.displayName = "DSR - DSC Transponder Response";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Numeric);
    add(d, "Field3", 3, VK::Text);
    add(d, "Field4", 4, VK::Numeric);
    add(d, "Text1", 5, VK::Text);
    add(d, "Field5", 6, VK::Numeric);
    add(d, "Text2", 7, VK::Text);
    add(d, "Indicator", 8, VK::Char);
    return d;
}

NmeaSentenceDef makeDTM()
{
    NmeaSentenceDef d;
    d.formatter = "DTM";
    d.displayName = "DTM - Datum Reference";
    add(d, "LocalDatum", 1, VK::Text);
    add(d, "LocalDatumSubcode", 2, VK::Char);
    add(d, "LatitudeOffset", 3, VK::Numeric);
    add(d, "LatitudeOffsetNS", 4, VK::Char);
    add(d, "LongitudeOffset", 5, VK::Numeric);
    add(d, "LongitudeOffsetEW", 6, VK::Char);
    add(d, "AltitudeOffset", 7, VK::Numeric);
    add(d, "ReferenceDatum", 8, VK::Text);
    return d;
}

NmeaSentenceDef makeFSI()
{
    NmeaSentenceDef d;
    d.formatter = "FSI";
    d.displayName = "FSI - Frequency Set Information";
    add(d, "Field1", 1, VK::Text);
    add(d, "Field2", 2, VK::Text);
    add(d, "Text", 3, VK::Text);
    add(d, "Field3", 4, VK::Numeric);
    return d;
}

NmeaSentenceDef makeGBS()
{
    NmeaSentenceDef d;
    d.formatter = "GBS";
    d.displayName = "GBS - GNSS Satellite Fault Detection";
    add(d, "UTC", 1, VK::Time);
    add(d, "LatitudeError", 2, VK::Numeric);
    add(d, "LongitudeError", 3, VK::Numeric);
    add(d, "AltitudeError", 4, VK::Numeric);
    add(d, "FailedSatelliteId", 5, VK::Text);
    add(d, "MissedDetectionProbability", 6, VK::Numeric);
    add(d, "BiasEstimate", 7, VK::Numeric);
    add(d, "BiasStdDev", 8, VK::Numeric);
    return d;
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

NmeaSentenceDef makeGLC()
{
    NmeaSentenceDef d;
    d.formatter = "GLC";
    d.displayName = "GLC - Geographic Position - Loran-C";
    add(d, "Field", 1, VK::Numeric);
    add(d, "Value1", 2, VK::Numeric);
    add(d, "Indicator1", 3, VK::Char);
    add(d, "Value2", 4, VK::Numeric);
    add(d, "Indicator2", 5, VK::Char);
    add(d, "Value3", 6, VK::Numeric);
    add(d, "Indicator3", 7, VK::Char);
    add(d, "Value4", 8, VK::Numeric);
    add(d, "Indicator4", 9, VK::Char);
    add(d, "Value5", 10, VK::Numeric);
    add(d, "Indicator5", 11, VK::Char);
    add(d, "Value6", 12, VK::Numeric);
    add(d, "Indicator6", 13, VK::Char);
    return d;
}

NmeaSentenceDef makeGLL()
{
    NmeaSentenceDef d;
    d.formatter = "GLL";
    d.displayName = "GLL - Geographic Position - Latitude/Longitude";
    add(d, "Latitude", 1, VK::Latitude);
    add(d, "NS", 2, VK::Char);
    add(d, "Longitude", 3, VK::Longitude);
    add(d, "EW", 4, VK::Char);
    add(d, "UTC", 5, VK::Time);
    add(d, "Status", 6, VK::Status);
    add(d, "ModeIndicator", 7, VK::Char);
    return d;
}

NmeaSentenceDef makeGMP()
{
    NmeaSentenceDef d;
    d.formatter = "GMP";
    d.displayName = "GMP - GNSS Map Projection Fix Data";
    add(d, "UTC", 1, VK::Time);
    add(d, "Text1", 2, VK::Text);
    add(d, "Text2", 3, VK::Text);
    add(d, "Value1", 4, VK::Numeric);
    add(d, "Value2", 5, VK::Numeric);
    add(d, "Text3", 6, VK::Text);
    add(d, "Field", 7, VK::Numeric);
    add(d, "Value3", 8, VK::Numeric);
    add(d, "Value4", 9, VK::Numeric);
    add(d, "Value5", 10, VK::Numeric);
    add(d, "Value6", 11, VK::Numeric);
    add(d, "Value7", 12, VK::Numeric);
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
    add(d, "DiffStationId", 12, VK::Numeric);
    return d;
}

NmeaSentenceDef makeGRS()
{
    NmeaSentenceDef d;
    d.formatter = "GRS";
    d.displayName = "GRS - GNSS Range Residuals";
    add(d, "UTC", 1, VK::Time);
    add(d, "Mode", 2, VK::Numeric);
    add(d, "Residual1", 3, VK::Numeric);
    add(d, "Residual2", 4, VK::Numeric);
    add(d, "Residual3", 5, VK::Numeric);
    add(d, "Residual4", 6, VK::Numeric);
    add(d, "Residual5", 7, VK::Numeric);
    add(d, "Residual6", 8, VK::Numeric);
    add(d, "Residual7", 9, VK::Numeric);
    add(d, "Residual8", 10, VK::Numeric);
    add(d, "Residual9", 11, VK::Numeric);
    add(d, "Residual10", 12, VK::Numeric);
    add(d, "Residual11", 13, VK::Numeric);
    add(d, "Residual12", 14, VK::Numeric);
    return d;
}

NmeaSentenceDef makeGSA()
{
    NmeaSentenceDef d;
    d.formatter = "GSA";
    d.displayName = "GSA - GNSS DOP and Active Satellites";
    add(d, "Mode", 1, VK::Char);
    add(d, "FixType", 2, VK::Numeric);
    add(d, "SatelliteId1", 3, VK::Numeric);
    add(d, "SatelliteId2", 4, VK::Numeric);
    add(d, "SatelliteId3", 5, VK::Numeric);
    add(d, "SatelliteId4", 6, VK::Numeric);
    add(d, "SatelliteId5", 7, VK::Numeric);
    add(d, "SatelliteId6", 8, VK::Numeric);
    add(d, "SatelliteId7", 9, VK::Numeric);
    add(d, "SatelliteId8", 10, VK::Numeric);
    add(d, "SatelliteId9", 11, VK::Numeric);
    add(d, "SatelliteId10", 12, VK::Numeric);
    add(d, "SatelliteId11", 13, VK::Numeric);
    add(d, "SatelliteId12", 14, VK::Numeric);
    add(d, "PDOP", 15, VK::Numeric);
    add(d, "HDOP", 16, VK::Numeric);
    add(d, "VDOP", 17, VK::Numeric);
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

NmeaSentenceDef makeGSV()
{
    NmeaSentenceDef d;
    d.formatter = "GSV";
    d.displayName = "GSV - GNSS Satellites in View";
    add(d, "NumMessages", 1, VK::Numeric);
    add(d, "MessageNumber", 2, VK::Numeric);
    add(d, "SatellitesInView", 3, VK::Numeric);
    add(d, "Sat1_Id", 4, VK::Numeric);
    add(d, "Sat1_Elevation", 5, VK::Numeric);
    add(d, "Sat1_Azimuth", 6, VK::Numeric);
    add(d, "Sat1_SNR", 7, VK::Numeric);
    add(d, "Sat2_Id", 8, VK::Numeric);
    add(d, "Sat2_Elevation", 9, VK::Numeric);
    add(d, "Sat2_Azimuth", 10, VK::Numeric);
    add(d, "Sat2_SNR", 11, VK::Numeric);
    add(d, "Sat3_Id", 12, VK::Numeric);
    add(d, "Sat3_Elevation", 13, VK::Numeric);
    add(d, "Sat3_Azimuth", 14, VK::Numeric);
    add(d, "Sat3_SNR", 15, VK::Numeric);
    add(d, "Sat4_Id", 16, VK::Numeric);
    add(d, "Sat4_Elevation", 17, VK::Numeric);
    add(d, "Sat4_Azimuth", 18, VK::Numeric);
    add(d, "Sat4_SNR", 19, VK::Numeric);
    return d;
}

NmeaSentenceDef makeHDG()
{
    NmeaSentenceDef d;
    d.formatter = "HDG";
    d.displayName = "HDG - Heading, Deviation & Variation";
    add(d, "MagneticHeading", 1, VK::Numeric);
    add(d, "Deviation", 2, VK::Numeric);
    add(d, "DeviationEW", 3, VK::Char);
    add(d, "Variation", 4, VK::Numeric);
    add(d, "VariationEW", 5, VK::Char);
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

NmeaSentenceDef makeHMR()
{
    NmeaSentenceDef d;
    d.formatter = "HMR";
    d.displayName = "HMR - Heading Monitor Receive";
    add(d, "Text1", 1, VK::Text);
    add(d, "Text2", 2, VK::Text);
    add(d, "Value1", 3, VK::Numeric);
    add(d, "Value2", 4, VK::Numeric);
    add(d, "Status1", 5, VK::Status);
    add(d, "Value3", 6, VK::Numeric);
    add(d, "Status2", 7, VK::Status);
    add(d, "Indicator1", 8, VK::Char);
    add(d, "Value4", 9, VK::Numeric);
    add(d, "Indicator2", 10, VK::Char);
    add(d, "Value5", 11, VK::Numeric);
    add(d, "Status3", 12, VK::Status);
    add(d, "Indicator3", 13, VK::Char);
    add(d, "Value6", 14, VK::Numeric);
    add(d, "Indicator4", 15, VK::Char);
    add(d, "Value7", 16, VK::Numeric);
    add(d, "Indicator5", 17, VK::Char);
    return d;
}

NmeaSentenceDef makeHMS()
{
    NmeaSentenceDef d;
    d.formatter = "HMS";
    d.displayName = "HMS - Heading Monitor Set";
    add(d, "Text1", 1, VK::Text);
    add(d, "Text2", 2, VK::Text);
    add(d, "Value", 3, VK::Numeric);
    return d;
}

NmeaSentenceDef makeHSC()
{
    NmeaSentenceDef d;
    d.formatter = "HSC";
    d.displayName = "HSC - Heading Steering Command";
    add(d, "HeadingTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    add(d, "HeadingMagnetic", 3, VK::Numeric);
    add(d, "MagneticRef", 4, VK::Char);
    return d;
}

NmeaSentenceDef makeHTC()
{
    NmeaSentenceDef d;
    d.formatter = "HTC";
    d.displayName = "HTC - Heading/Track Control Command";
    add(d, "Status", 1, VK::Status);
    add(d, "Value1", 2, VK::Numeric);
    add(d, "Indicator1", 3, VK::Char);
    add(d, "Indicator2", 4, VK::Char);
    add(d, "Indicator3", 5, VK::Char);
    add(d, "Value2", 6, VK::Numeric);
    add(d, "Value3", 7, VK::Numeric);
    add(d, "Value4", 8, VK::Numeric);
    add(d, "Value5", 9, VK::Numeric);
    add(d, "Value6", 10, VK::Numeric);
    add(d, "Value7", 11, VK::Numeric);
    add(d, "Value8", 12, VK::Numeric);
    add(d, "Indicator4", 13, VK::Char);
    return d;
}

NmeaSentenceDef makeHTD()
{
    NmeaSentenceDef d;
    d.formatter = "HTD";
    d.displayName = "HTD - Heading/Track Control Data";
    add(d, "Status1", 1, VK::Status);
    add(d, "Value1", 2, VK::Numeric);
    add(d, "Indicator1", 3, VK::Char);
    add(d, "Indicator2", 4, VK::Char);
    add(d, "Indicator3", 5, VK::Char);
    add(d, "Value2", 6, VK::Numeric);
    add(d, "Value3", 7, VK::Numeric);
    add(d, "Value4", 8, VK::Numeric);
    add(d, "Value5", 9, VK::Numeric);
    add(d, "Value6", 10, VK::Numeric);
    add(d, "Value7", 11, VK::Numeric);
    add(d, "Value8", 12, VK::Numeric);
    add(d, "Indicator4", 13, VK::Char);
    add(d, "Status2", 14, VK::Status);
    add(d, "Status3", 15, VK::Status);
    add(d, "Status4", 16, VK::Status);
    add(d, "Value9", 17, VK::Numeric);
    return d;
}

NmeaSentenceDef makeLCD()
{
    NmeaSentenceDef d;
    d.formatter = "LCD";
    d.displayName = "LCD - Loran-C Signal Data";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Numeric);
    add(d, "Field3", 3, VK::Numeric);
    add(d, "Field4", 4, VK::Numeric);
    add(d, "Field5", 5, VK::Numeric);
    add(d, "Field6", 6, VK::Numeric);
    add(d, "Field7", 7, VK::Numeric);
    add(d, "Field8", 8, VK::Numeric);
    add(d, "Field9", 9, VK::Numeric);
    add(d, "Field10", 10, VK::Numeric);
    add(d, "Field11", 11, VK::Numeric);
    add(d, "Field12", 12, VK::Numeric);
    add(d, "Field13", 13, VK::Numeric);
    return d;
}

NmeaSentenceDef makeLR1()
{
    NmeaSentenceDef d;
    d.formatter = "LR1";
    d.displayName = "LR1 - UAIS Long-range Reply Sentence 1";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Text);
    add(d, "Field3", 3, VK::Text);
    add(d, "Text1", 4, VK::Text);
    add(d, "Text2", 5, VK::Text);
    add(d, "Field4", 6, VK::Text);
    return d;
}

NmeaSentenceDef makeLR2()
{
    NmeaSentenceDef d;
    d.formatter = "LR2";
    d.displayName = "LR2 - UAIS Long-range Reply Sentence 2";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Text);
    add(d, "Field3", 3, VK::Text);
    add(d, "UTC", 4, VK::Time);
    add(d, "Latitude", 5, VK::Latitude);
    add(d, "NS", 6, VK::Char);
    add(d, "Longitude", 7, VK::Longitude);
    add(d, "EW", 8, VK::Char);
    add(d, "Value1", 9, VK::Numeric);
    add(d, "True", 10, VK::Char);
    add(d, "Value2", 11, VK::Numeric);
    add(d, "Knots", 12, VK::Char);
    return d;
}

NmeaSentenceDef makeLR3()
{
    NmeaSentenceDef d;
    d.formatter = "LR3";
    d.displayName = "LR3 - UAIS Long-range Reply Sentence 3";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Text);
    add(d, "Text", 3, VK::Text);
    add(d, "Field3", 4, VK::Text);
    add(d, "UTC", 5, VK::Time);
    add(d, "Value1", 6, VK::Numeric);
    add(d, "Value2", 7, VK::Numeric);
    add(d, "Value3", 8, VK::Numeric);
    add(d, "Value4", 9, VK::Numeric);
    add(d, "Value5", 10, VK::Numeric);
    add(d, "Value6", 11, VK::Numeric);
    return d;
}

NmeaSentenceDef makeLRF()
{
    NmeaSentenceDef d;
    d.formatter = "LRF";
    d.displayName = "LRF - UAIS Long-Range Function";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Text);
    add(d, "Text1", 3, VK::Text);
    add(d, "Text2", 4, VK::Text);
    add(d, "Text3", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeLRI()
{
    NmeaSentenceDef d;
    d.formatter = "LRI";
    d.displayName = "LRI - UAIS Long-Range Interrogation";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Indicator", 2, VK::Char);
    add(d, "Field2", 3, VK::Text);
    add(d, "Field3", 4, VK::Text);
    add(d, "Latitude1", 5, VK::Latitude);
    add(d, "NS1", 6, VK::Char);
    add(d, "Longitude1", 7, VK::Longitude);
    add(d, "EW1", 8, VK::Char);
    add(d, "Latitude2", 9, VK::Latitude);
    add(d, "NS2", 10, VK::Char);
    add(d, "Longitude2", 11, VK::Longitude);
    add(d, "EW2", 12, VK::Char);
    return d;
}

NmeaSentenceDef makeMLA()
{
    NmeaSentenceDef d;
    d.formatter = "MLA";
    d.displayName = "MLA - GLONASS Almanac Data";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Field", 3, VK::Numeric);
    add(d, "Value3", 4, VK::Numeric);
    add(d, "Hex1", 5, VK::Text);
    add(d, "Hex2", 6, VK::Text);
    add(d, "Hex3", 7, VK::Text);
    add(d, "Hex4", 8, VK::Text);
    add(d, "Hex5", 9, VK::Text);
    add(d, "Hex6", 10, VK::Text);
    add(d, "Hex7", 11, VK::Text);
    add(d, "Hex8", 12, VK::Text);
    add(d, "Hex9", 13, VK::Text);
    add(d, "Hex10", 14, VK::Text);
    add(d, "Hex11", 15, VK::Text);
    return d;
}

NmeaSentenceDef makeMSK()
{
    NmeaSentenceDef d;
    d.formatter = "MSK";
    d.displayName = "MSK - MSK Receiver Interface";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Indicator1", 2, VK::Char);
    add(d, "Value2", 3, VK::Numeric);
    add(d, "Indicator2", 4, VK::Char);
    add(d, "Value3", 5, VK::Numeric);
    add(d, "Field", 6, VK::Numeric);
    return d;
}

NmeaSentenceDef makeMSS()
{
    NmeaSentenceDef d;
    d.formatter = "MSS";
    d.displayName = "MSS - MSK Receiver Signal";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Value3", 3, VK::Numeric);
    add(d, "Value4", 4, VK::Numeric);
    add(d, "Field", 5, VK::Numeric);
    return d;
}

NmeaSentenceDef makeMTW()
{
    NmeaSentenceDef d;
    d.formatter = "MTW";
    d.displayName = "MTW - Water Temperature";
    add(d, "WaterTemperature", 1, VK::Numeric);
    add(d, "Unit", 2, VK::Char);
    return d;
}

NmeaSentenceDef makeMWD()
{
    NmeaSentenceDef d;
    d.formatter = "MWD";
    d.displayName = "MWD - Wind Direction & Speed";
    add(d, "WindDirectionTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    add(d, "WindDirectionMagnetic", 3, VK::Numeric);
    add(d, "MagneticRef", 4, VK::Char);
    add(d, "WindSpeedKnots", 5, VK::Numeric);
    add(d, "KnotsUnit", 6, VK::Char);
    add(d, "WindSpeedMps", 7, VK::Numeric);
    add(d, "MpsUnit", 8, VK::Char);
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

NmeaSentenceDef makeOSD()
{
    NmeaSentenceDef d;
    d.formatter = "OSD";
    d.displayName = "OSD - Own Ship Data";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Status", 2, VK::Status);
    add(d, "Value2", 3, VK::Numeric);
    add(d, "Indicator1", 4, VK::Char);
    add(d, "Value3", 5, VK::Numeric);
    add(d, "Indicator2", 6, VK::Char);
    add(d, "Value4", 7, VK::Numeric);
    add(d, "Value5", 8, VK::Numeric);
    add(d, "Indicator3", 9, VK::Char);
    return d;
}

NmeaSentenceDef makeRMA()
{
    NmeaSentenceDef d;
    d.formatter = "RMA";
    d.displayName = "RMA - Recommended Minimum Specific Loran-C Data";
    add(d, "Status", 1, VK::Status);
    add(d, "Latitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "Longitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "TimeDifferenceA", 6, VK::Numeric);
    add(d, "TimeDifferenceB", 7, VK::Numeric);
    add(d, "SpeedOverGround", 8, VK::Numeric);
    add(d, "CourseOverGround", 9, VK::Numeric);
    add(d, "MagneticVariation", 10, VK::Numeric);
    add(d, "VariationEW", 11, VK::Char);
    add(d, "ModeIndicator", 12, VK::Char);
    return d;
}

NmeaSentenceDef makeRMB()
{
    NmeaSentenceDef d;
    d.formatter = "RMB";
    d.displayName = "RMB - Recommended Minimum Navigation Information";
    add(d, "Status", 1, VK::Status);
    add(d, "CrossTrackError", 2, VK::Numeric);
    add(d, "DirectionToSteer", 3, VK::Char);
    add(d, "OriginWaypointId", 4, VK::Text);
    add(d, "DestinationWaypointId", 5, VK::Text);
    add(d, "DestinationLatitude", 6, VK::Latitude);
    add(d, "NS", 7, VK::Char);
    add(d, "DestinationLongitude", 8, VK::Longitude);
    add(d, "EW", 9, VK::Char);
    add(d, "RangeToDestination", 10, VK::Numeric);
    add(d, "BearingToDestination", 11, VK::Numeric);
    add(d, "DestinationClosingVelocity", 12, VK::Numeric);
    add(d, "ArrivalStatus", 13, VK::Status);
    add(d, "ModeIndicator", 14, VK::Char);
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

NmeaSentenceDef makeROT()
{
    NmeaSentenceDef d;
    d.formatter = "ROT";
    d.displayName = "ROT - Rate Of Turn";
    add(d, "RateOfTurn", 1, VK::Numeric);
    add(d, "Status", 2, VK::Status);
    return d;
}

NmeaSentenceDef makeRPM()
{
    NmeaSentenceDef d;
    d.formatter = "RPM";
    d.displayName = "RPM - Revolutions";
    add(d, "Source", 1, VK::Char);
    add(d, "EngineShaftNumber", 2, VK::Numeric);
    add(d, "Speed", 3, VK::Numeric);
    add(d, "PropellerPitch", 4, VK::Numeric);
    add(d, "Status", 5, VK::Status);
    return d;
}

NmeaSentenceDef makeRSA()
{
    NmeaSentenceDef d;
    d.formatter = "RSA";
    d.displayName = "RSA - Rudder Sensor Angle";
    add(d, "StarboardRudderAngle", 1, VK::Numeric);
    add(d, "StarboardStatus", 2, VK::Status);
    add(d, "PortRudderAngle", 3, VK::Numeric);
    add(d, "PortStatus", 4, VK::Status);
    return d;
}

NmeaSentenceDef makeRSD()
{
    NmeaSentenceDef d;
    d.formatter = "RSD";
    d.displayName = "RSD - Radar System Data";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Value3", 3, VK::Numeric);
    add(d, "Value4", 4, VK::Numeric);
    add(d, "Value5", 5, VK::Numeric);
    add(d, "Value6", 6, VK::Numeric);
    add(d, "Value7", 7, VK::Numeric);
    add(d, "Value8", 8, VK::Numeric);
    add(d, "Value9", 9, VK::Numeric);
    add(d, "Value10", 10, VK::Numeric);
    add(d, "Value11", 11, VK::Numeric);
    add(d, "Indicator1", 12, VK::Char);
    add(d, "Indicator2", 13, VK::Char);
    return d;
}

NmeaSentenceDef makeRTE()
{
    NmeaSentenceDef d;
    d.formatter = "RTE";
    d.displayName = "RTE - Routes RTE - Routes";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Indicator", 3, VK::Char);
    add(d, "Text1", 4, VK::Text);
    add(d, "Text2", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeSFI()
{
    NmeaSentenceDef d;
    d.formatter = "SFI";
    d.displayName = "SFI - Scanning Frequency Information";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Field", 3, VK::Text);
    add(d, "Text1", 4, VK::Text);
    add(d, "Text2", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeSSD()
{
    NmeaSentenceDef d;
    d.formatter = "SSD";
    d.displayName = "SSD - UAIS Ship Static Data";
    add(d, "Text1", 1, VK::Text);
    add(d, "Text2", 2, VK::Text);
    add(d, "Field1", 3, VK::Numeric);
    add(d, "Field2", 4, VK::Numeric);
    add(d, "Field3", 5, VK::Numeric);
    add(d, "Field4", 6, VK::Numeric);
    add(d, "Text3", 7, VK::Text);
    add(d, "Field5", 8, VK::Text);
    return d;
}

NmeaSentenceDef makeSTN()
{
    NmeaSentenceDef d;
    d.formatter = "STN";
    d.displayName = "STN - Multiple Data ID";
    add(d, "Field", 1, VK::Numeric);
    return d;
}

NmeaSentenceDef makeTLB()
{
    NmeaSentenceDef d;
    d.formatter = "TLB";
    d.displayName = "TLB - Target Label";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Text1", 2, VK::Text);
    add(d, "Value2", 3, VK::Numeric);
    add(d, "Text2", 4, VK::Text);
    add(d, "Text3", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeTLL()
{
    NmeaSentenceDef d;
    d.formatter = "TLL";
    d.displayName = "TLL - Target Latitude and Longitude";
    add(d, "Field", 1, VK::Numeric);
    add(d, "Latitude", 2, VK::Latitude);
    add(d, "NS", 3, VK::Char);
    add(d, "Longitude", 4, VK::Longitude);
    add(d, "EW", 5, VK::Char);
    add(d, "Text", 6, VK::Text);
    add(d, "UTC", 7, VK::Time);
    add(d, "Indicator1", 8, VK::Char);
    add(d, "Indicator2", 9, VK::Char);
    return d;
}

NmeaSentenceDef makeTTM()
{
    NmeaSentenceDef d;
    d.formatter = "TTM";
    d.displayName = "TTM - Tracked Target Message";
    add(d, "Field", 1, VK::Numeric);
    add(d, "Value1", 2, VK::Numeric);
    add(d, "Value2", 3, VK::Numeric);
    add(d, "Indicator1", 4, VK::Char);
    add(d, "Value3", 5, VK::Numeric);
    add(d, "Value4", 6, VK::Numeric);
    add(d, "Indicator2", 7, VK::Char);
    add(d, "Value5", 8, VK::Numeric);
    add(d, "Value6", 9, VK::Numeric);
    add(d, "Indicator3", 10, VK::Char);
    add(d, "Text", 11, VK::Text);
    add(d, "Indicator4", 12, VK::Char);
    add(d, "Indicator5", 13, VK::Char);
    add(d, "UTC", 14, VK::Time);
    add(d, "Indicator6", 15, VK::Char);
    return d;
}

NmeaSentenceDef makeTUT()
{
    NmeaSentenceDef d;
    d.formatter = "TUT";
    d.displayName = "TUT - Transmission of Multi-language Text";
    add(d, "Field1", 1, VK::Text);
    add(d, "Hex1", 2, VK::Text);
    add(d, "Hex2", 3, VK::Text);
    add(d, "Field2", 4, VK::Numeric);
    add(d, "Text", 5, VK::Text);
    add(d, "Hex3", 6, VK::Text);
    return d;
}

NmeaSentenceDef makeTXT()
{
    NmeaSentenceDef d;
    d.formatter = "TXT";
    d.displayName = "TXT - Text Transmission";
    add(d, "Field1", 1, VK::Numeric);
    add(d, "Field2", 2, VK::Numeric);
    add(d, "Field3", 3, VK::Numeric);
    add(d, "Text", 4, VK::Text);
    return d;
}

NmeaSentenceDef makeVBW()
{
    NmeaSentenceDef d;
    d.formatter = "VBW";
    d.displayName = "VBW - Dual Ground/Water Speed";
    add(d, "LongitudinalWaterSpeed", 1, VK::Numeric);
    add(d, "TransverseWaterSpeed", 2, VK::Numeric);
    add(d, "WaterSpeedStatus", 3, VK::Status);
    add(d, "LongitudinalGroundSpeed", 4, VK::Numeric);
    add(d, "TransverseGroundSpeed", 5, VK::Numeric);
    add(d, "GroundSpeedStatus", 6, VK::Status);
    add(d, "SternTransverseWaterSpeed", 7, VK::Numeric);
    add(d, "SternWaterStatus", 8, VK::Status);
    add(d, "SternTransverseGroundSpeed", 9, VK::Numeric);
    add(d, "SternGroundStatus", 10, VK::Status);
    return d;
}

NmeaSentenceDef makeVDR()
{
    NmeaSentenceDef d;
    d.formatter = "VDR";
    d.displayName = "VDR - Set and Drift";
    add(d, "DirectionTrue", 1, VK::Numeric);
    add(d, "TrueRef", 2, VK::Char);
    add(d, "DirectionMagnetic", 3, VK::Numeric);
    add(d, "MagneticRef", 4, VK::Char);
    add(d, "CurrentSpeed", 5, VK::Numeric);
    add(d, "SpeedUnit", 6, VK::Char);
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
    add(d, "KnotsUnit", 6, VK::Char);
    add(d, "SpeedKmh", 7, VK::Numeric);
    add(d, "KmhUnit", 8, VK::Char);
    return d;
}

NmeaSentenceDef makeVLW()
{
    NmeaSentenceDef d;
    d.formatter = "VLW";
    d.displayName = "VLW - Dual Ground/Water Distance";
    add(d, "TotalWaterDistance", 1, VK::Numeric);
    add(d, "TotalWaterUnit", 2, VK::Char);
    add(d, "TripWaterDistance", 3, VK::Numeric);
    add(d, "TripWaterUnit", 4, VK::Char);
    add(d, "TotalGroundDistance", 5, VK::Numeric);
    add(d, "TotalGroundUnit", 6, VK::Char);
    add(d, "TripGroundDistance", 7, VK::Numeric);
    add(d, "TripGroundUnit", 8, VK::Char);
    return d;
}

NmeaSentenceDef makeVPW()
{
    NmeaSentenceDef d;
    d.formatter = "VPW";
    d.displayName = "VPW - Speed - Measured Parallel to Wind";
    add(d, "SpeedParallelKnots", 1, VK::Numeric);
    add(d, "KnotsUnit", 2, VK::Char);
    add(d, "SpeedParallelMps", 3, VK::Numeric);
    add(d, "MpsUnit", 4, VK::Char);
    return d;
}

NmeaSentenceDef makeVSD()
{
    NmeaSentenceDef d;
    d.formatter = "VSD";
    d.displayName = "VSD - UAIS Voyage Static Data";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Value2", 2, VK::Numeric);
    add(d, "Value3", 3, VK::Numeric);
    add(d, "Text", 4, VK::Text);
    add(d, "UTC", 5, VK::Time);
    add(d, "Field1", 6, VK::Numeric);
    add(d, "Field2", 7, VK::Numeric);
    add(d, "Value4", 8, VK::Numeric);
    add(d, "Value5", 9, VK::Numeric);
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

NmeaSentenceDef makeWCV()
{
    NmeaSentenceDef d;
    d.formatter = "WCV";
    d.displayName = "WCV - Waypoint Closure Velocity";
    add(d, "Value", 1, VK::Numeric);
    add(d, "Knots", 2, VK::Char);
    add(d, "Text", 3, VK::Text);
    add(d, "Indicator", 4, VK::Char);
    return d;
}

NmeaSentenceDef makeWNC()
{
    NmeaSentenceDef d;
    d.formatter = "WNC";
    d.displayName = "WNC - Distance - Waypoint to Waypoint";
    add(d, "Value1", 1, VK::Numeric);
    add(d, "Knots", 2, VK::Char);
    add(d, "Value2", 3, VK::Numeric);
    add(d, "KmH", 4, VK::Char);
    add(d, "Text1", 5, VK::Text);
    add(d, "Text2", 6, VK::Text);
    return d;
}

NmeaSentenceDef makeWPL()
{
    NmeaSentenceDef d;
    d.formatter = "WPL";
    d.displayName = "WPL - Waypoint Location";
    add(d, "Latitude", 1, VK::Latitude);
    add(d, "NS", 2, VK::Char);
    add(d, "Longitude", 3, VK::Longitude);
    add(d, "EW", 4, VK::Char);
    add(d, "WaypointId", 5, VK::Text);
    return d;
}

NmeaSentenceDef makeXDR()
{
    NmeaSentenceDef d;
    d.formatter = "XDR";
    d.displayName = "XDR - Transducer Measurements";
    add(d, "Indicator1", 1, VK::Char);
    add(d, "Value1", 2, VK::Numeric);
    add(d, "Indicator2", 3, VK::Char);
    add(d, "Text1", 4, VK::Text);
    add(d, "Value2", 5, VK::Numeric);
    add(d, "Indicator3", 6, VK::Char);
    add(d, "Text2", 7, VK::Text);
    return d;
}

NmeaSentenceDef makeXTE()
{
    NmeaSentenceDef d;
    d.formatter = "XTE";
    d.displayName = "XTE - Cross-Track Error, Measured";
    add(d, "Status1", 1, VK::Status);
    add(d, "Status2", 2, VK::Status);
    add(d, "CrossTrackError", 3, VK::Numeric);
    add(d, "DirectionToSteer", 4, VK::Char);
    add(d, "XteUnit", 5, VK::Char);
    add(d, "ModeIndicator", 6, VK::Char);
    return d;
}

NmeaSentenceDef makeXTR()
{
    NmeaSentenceDef d;
    d.formatter = "XTR";
    d.displayName = "XTR - Cross-Track Error - Dead Reckoning";
    add(d, "Value", 1, VK::Numeric);
    add(d, "Indicator", 2, VK::Char);
    add(d, "Knots", 3, VK::Char);
    return d;
}

NmeaSentenceDef makeZDA()
{
    NmeaSentenceDef d;
    d.formatter = "ZDA";
    d.displayName = "ZDA - Time & Date";
    add(d, "UTC", 1, VK::Time);
    add(d, "Day", 2, VK::Numeric);
    add(d, "Month", 3, VK::Numeric);
    add(d, "Year", 4, VK::Numeric);
    add(d, "LocalZoneHours", 5, VK::Numeric);
    add(d, "LocalZoneMinutes", 6, VK::Numeric);
    return d;
}

NmeaSentenceDef makeZDL()
{
    NmeaSentenceDef d;
    d.formatter = "ZDL";
    d.displayName = "ZDL - Time and Distance to Variable Point";
    add(d, "UTC", 1, VK::Time);
    add(d, "Value", 2, VK::Numeric);
    add(d, "Indicator", 3, VK::Char);
    return d;
}

NmeaSentenceDef makeZFO()
{
    NmeaSentenceDef d;
    d.formatter = "ZFO";
    d.displayName = "ZFO - UTC & Time from Origin Waypoint";
    add(d, "UTC1", 1, VK::Time);
    add(d, "UTC2", 2, VK::Time);
    add(d, "Text", 3, VK::Text);
    return d;
}

NmeaSentenceDef makeZTG()
{
    NmeaSentenceDef d;
    d.formatter = "ZTG";
    d.displayName = "ZTG - UTC & Time to Destination Waypoint";
    add(d, "UTC1", 1, VK::Time);
    add(d, "UTC2", 2, VK::Time);
    add(d, "Text", 3, VK::Text);
    return d;
}

struct Catalogue
{
    QList<NmeaSentenceDef> defs;
    QHash<QString, int>    indexByFormatter;

    Catalogue()
    {
        defs << makeAAM();
        defs << makeABK();
        defs << makeACA();
        defs << makeACK();
        defs << makeACS();
        defs << makeAIR();
        defs << makeALM();
        defs << makeALR();
        defs << makeAPB();
        defs << makeBEC();
        defs << makeBOD();
        defs << makeBWC();
        defs << makeBWR();
        defs << makeBWW();
        defs << makeCUR();
        defs << makeDBT();
        defs << makeDCN();
        defs << makeDPT();
        defs << makeDSC();
        defs << makeDSE();
        defs << makeDSI();
        defs << makeDSR();
        defs << makeDTM();
        defs << makeFSI();
        defs << makeGBS();
        defs << makeGGA();
        defs << makeGLC();
        defs << makeGLL();
        defs << makeGMP();
        defs << makeGNS();
        defs << makeGRS();
        defs << makeGSA();
        defs << makeGST();
        defs << makeGSV();
        defs << makeHDG();
        defs << makeHDT();
        defs << makeHMR();
        defs << makeHMS();
        defs << makeHSC();
        defs << makeHTC();
        defs << makeHTD();
        defs << makeLCD();
        defs << makeLR1();
        defs << makeLR2();
        defs << makeLR3();
        defs << makeLRF();
        defs << makeLRI();
        defs << makeMLA();
        defs << makeMSK();
        defs << makeMSS();
        defs << makeMTW();
        defs << makeMWD();
        defs << makeMWV();
        defs << makeOSD();
        defs << makeRMA();
        defs << makeRMB();
        defs << makeRMC();
        defs << makeROT();
        defs << makeRPM();
        defs << makeRSA();
        defs << makeRSD();
        defs << makeRTE();
        defs << makeSFI();
        defs << makeSSD();
        defs << makeSTN();
        defs << makeTLB();
        defs << makeTLL();
        defs << makeTTM();
        defs << makeTUT();
        defs << makeTXT();
        defs << makeVBW();
        defs << makeVDR();
        defs << makeVHW();
        defs << makeVLW();
        defs << makeVPW();
        defs << makeVSD();
        defs << makeVTG();
        defs << makeWCV();
        defs << makeWNC();
        defs << makeWPL();
        defs << makeXDR();
        defs << makeXTE();
        defs << makeXTR();
        defs << makeZDA();
        defs << makeZDL();
        defs << makeZFO();
        defs << makeZTG();
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

