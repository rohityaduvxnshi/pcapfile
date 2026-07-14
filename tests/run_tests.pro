# Universal Data Suite — headless functional test harness.
#
# Links the SHARED core (via shared.pri) plus the specific, non-colliding
# data-path .cpp files from each app (PayloadBuilder = simulator encode,
# ExtractionEngine = parser decode, PcapFileReader/UdpPacketParser = parser
# capture read-back). None of these pull in the diverged same-named dialogs, so
# both apps' files coexist in one binary. Runs real assertions and writes
# test_files/output/test_results.json for the Excel report generator.
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}

TARGET = run_tests
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle

include(../shared/shared.pri)

# Excel codec needs the vendored QXlsx (same wiring as the apps).
QXLSX_PARENTPATH = $$PWD/../third_party/QXlsx/
QXLSX_HEADERPATH = $$PWD/../third_party/QXlsx/header/
QXLSX_SOURCEPATH = $$PWD/../third_party/QXlsx/source/
include(../third_party/QXlsx/QXlsx.pri)

INCLUDEPATH += $$PWD/../app_simulator/headers
INCLUDEPATH += $$PWD/../app_parser/headers

# Specific app data-path sources (no dialogs, no main.cpp, no name clashes).
SOURCES += $$PWD/../app_simulator/sources/PayloadBuilder.cpp
SOURCES += $$PWD/../app_parser/sources/ExtractionEngine.cpp
SOURCES += $$PWD/../app_parser/sources/BitfieldDecoder.cpp
SOURCES += $$PWD/../app_parser/sources/ConditionalBitfieldDecoder.cpp
SOURCES += $$PWD/../app_parser/sources/PcapFileReader.cpp
SOURCES += $$PWD/../app_parser/sources/UdpPacketParser.cpp

SOURCES += $$PWD/main.cpp
