QT += core gui widgets

CONFIG += c++11

TARGET = PcapUdpExtractor
TEMPLATE = app

INCLUDEPATH += headers

SOURCES += sources/main.cpp
SOURCES += sources/MainWindow.cpp
SOURCES += sources/FieldDefinition.cpp
SOURCES += sources/InputValidator.cpp
SOURCES += sources/InputValidator_filters.cpp
SOURCES += sources/PcapFileReader.cpp
SOURCES += sources/UdpPacketParser.cpp
SOURCES += sources/ExtractionEngine.cpp
SOURCES += sources/CsvExporter.cpp
SOURCES += sources/MathExpressionEvaluator.cpp
SOURCES += sources/BitfieldDecoder.cpp
SOURCES += sources/BitfieldRuleDialog.cpp
SOURCES += sources/BitfieldDecoderDialog.cpp

HEADERS += headers/MainWindow.h
HEADERS += headers/ui_MainWindow.h
HEADERS += headers/AppTypes.h
HEADERS += headers/FieldDefinition.h
HEADERS += headers/FilterTypes.h
HEADERS += headers/InputValidator.h
HEADERS += headers/PcapFileReader.h
HEADERS += headers/UdpPacketParser.h
HEADERS += headers/ExtractionEngine.h
HEADERS += headers/CsvExporter.h
HEADERS += headers/MathExpressionEvaluator.h
HEADERS += headers/BitfieldDecoder.h
HEADERS += headers/BitfieldRuleDialog.h
HEADERS += headers/BitfieldDecoderDialog.h
