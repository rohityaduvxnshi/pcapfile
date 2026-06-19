# Shared core for the Universal Data Suite — compiled into each app via include().
# One source of truth for the data model, NMEA stack, themes, codecs and the ICD
# .docx extraction layer. $$PWD here is this file's directory (shared/), so the
# paths resolve the same no matter which app .pro includes it.

QT += core gui widgets network serialport gui-private

INCLUDEPATH += $$PWD/headers

SOURCES += $$files($$PWD/sources/*.cpp)
HEADERS += $$files($$PWD/headers/*.h)
FORMS   += $$files($$PWD/forms/*.ui)

# Embedded chevron icons for combo/spin dropdown arrows (used by Themes).
RESOURCES += $$PWD/assets.qrc
