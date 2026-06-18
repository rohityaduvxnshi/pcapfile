# Universal Wireshark Log Reader (parser) — one of the two apps in the suite.
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}

TARGET = UniversalWiresharkLogReader
TEMPLATE = app

# Internal IDs stay "PcapUdpExtractor" (see sources/main.cpp) so existing
# QSettings / AppData survive; the display name lives in forms/MainWindow.ui.

include(../shared/shared.pri)

INCLUDEPATH += $$PWD/headers

# Excel (.xlsx) export — vendored QXlsx (MIT). The .pri self-locates via its own
# $$PWD, so explicit paths just point at the shared third_party copy at repo root.
QXLSX_PARENTPATH = $$PWD/../third_party/QXlsx/
QXLSX_HEADERPATH = $$PWD/../third_party/QXlsx/header/
QXLSX_SOURCEPATH = $$PWD/../third_party/QXlsx/source/
include(../third_party/QXlsx/QXlsx.pri)

SOURCES += $$files($$PWD/sources/*.cpp)
HEADERS += $$files($$PWD/headers/*.h)
FORMS   += $$files($$PWD/forms/*.ui)
