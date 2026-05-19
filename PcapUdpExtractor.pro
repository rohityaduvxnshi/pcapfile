QT += core gui widgets network

greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}

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
SOURCES += sources/LiveUdpReceiver.cpp
SOURCES += sources/CsvStreamWriter.cpp
SOURCES += sources/MessageDefinition.cpp
SOURCES += sources/MessageLengthFilterDialog.cpp
SOURCES += sources/MessageDefinitionDialog.cpp
SOURCES += sources/FieldConfigurationDialog.cpp

HEADERS += headers/MainWindow.h
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
HEADERS += headers/LiveUdpReceiver.h
HEADERS += headers/CsvStreamWriter.h
HEADERS += headers/MessageDefinition.h
HEADERS += headers/MessageLengthFilterDialog.h
HEADERS += headers/MessageDefinitionDialog.h
HEADERS += headers/FieldConfigurationDialog.h

FORMS += forms/MainWindow.ui
FORMS += forms/BitfieldDecoderDialog.ui
FORMS += forms/BitfieldRuleDialog.ui
FORMS += forms/MessageLengthFilterDialog.ui
FORMS += forms/MessageDefinitionDialog.ui
FORMS += forms/FieldConfigurationDialog.ui
