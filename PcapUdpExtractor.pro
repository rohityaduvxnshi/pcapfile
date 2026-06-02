QT += core gui widgets network
# ICD .docx import unzips the package with Qt's private QZipReader (offline,
# no external dependency, no GPL/LGPL beyond Qt itself).
QT += gui-private

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
SOURCES += sources/MessageLengthFilterDialog.cpp
SOURCES += sources/MessageDefinitionDialog.cpp
SOURCES += sources/FieldConfigurationDialog.cpp
SOURCES += sources/ConditionalBitfieldDecoder.cpp
SOURCES += sources/ConditionalBitfieldDecoderDialog.cpp
SOURCES += sources/ConditionalProfileDialog.cpp
SOURCES += sources/FieldCsvCodec.cpp
SOURCES += sources/ProjectFile.cpp
SOURCES += sources/BitRuleCsvCodec.cpp
SOURCES += sources/Themes.cpp
SOURCES += sources/CompareOptionsEngine.cpp
SOURCES += sources/CompareOptionsDialog.cpp
SOURCES += sources/NmeaSentenceRegistry.cpp
SOURCES += sources/NmeaDecoder.cpp
SOURCES += sources/NmeaSentencePickerDialog.cpp
SOURCES += sources/NmeaFieldConfigurationDialog.cpp
SOURCES += sources/IcdDocxImporter.cpp
SOURCES += sources/IcdImportDialog.cpp
SOURCES += sources/IcdTableSettingsDialog.cpp

HEADERS += headers/MainWindow.h
HEADERS += headers/AppTypes.h
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
HEADERS += headers/ConditionalBitfieldDecoder.h
HEADERS += headers/ConditionalBitfieldDecoderDialog.h
HEADERS += headers/ConditionalProfileDialog.h
HEADERS += headers/FieldCsvCodec.h
HEADERS += headers/ProjectFile.h
HEADERS += headers/BitRuleCsvCodec.h
HEADERS += headers/Themes.h
HEADERS += headers/CompareOptionsEngine.h
HEADERS += headers/CompareOptionsDialog.h
HEADERS += headers/NmeaTypes.h
HEADERS += headers/NmeaSentenceRegistry.h
HEADERS += headers/NmeaDecoder.h
HEADERS += headers/NmeaSentencePickerDialog.h
HEADERS += headers/NmeaFieldConfigurationDialog.h
HEADERS += headers/IcdImportTypes.h
HEADERS += headers/IcdDocxImporter.h
HEADERS += headers/IcdImportDialog.h
HEADERS += headers/IcdTableSettingsDialog.h

FORMS += forms/MainWindow.ui
FORMS += forms/BitfieldDecoderDialog.ui
FORMS += forms/BitfieldRuleDialog.ui
FORMS += forms/MessageLengthFilterDialog.ui
FORMS += forms/MessageDefinitionDialog.ui
FORMS += forms/FieldConfigurationDialog.ui
FORMS += forms/ConditionalBitfieldDecoderDialog.ui
FORMS += forms/ConditionalProfileDialog.ui
FORMS += forms/CompareOptionsDialog.ui
FORMS += forms/NmeaSentencePickerDialog.ui
FORMS += forms/NmeaFieldConfigurationDialog.ui
FORMS += forms/IcdImportDialog.ui
FORMS += forms/IcdTableSettingsDialog.ui
FORMS += forms/IcdTablePreviewDialog.ui
FORMS += forms/FilterRowWidget.ui
