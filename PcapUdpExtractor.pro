QT += core gui widgets

CONFIG += c++11

TARGET = PcapUdpExtractor
TEMPLATE = app

INCLUDEPATH += headers

SOURCES += \
    sources/main.cpp \
    sources/MainWindow.cpp \
    sources/FieldDefinition.cpp \
    sources/InputValidator.cpp \
    sources/PcapFileReader.cpp \
    sources/UdpPacketParser.cpp \
    sources/ExtractionEngine.cpp \
    sources/CsvExporter.cpp \
    sources/MathExpressionEvaluator.cpp

HEADERS += \
    headers/MainWindow.h \
    headers/ui_MainWindow.h \
    headers/AppTypes.h \
    headers/FieldDefinition.h \
    headers/InputValidator.h \
    headers/PcapFileReader.h \
    headers/UdpPacketParser.h \
    headers/ExtractionEngine.h \
    headers/CsvExporter.h \
    headers/MathExpressionEvaluator.h
