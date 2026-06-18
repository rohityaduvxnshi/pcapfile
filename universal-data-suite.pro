# Universal Data Suite — one project, two distinct executables:
#   app_parser     -> UniversalWiresharkLogReader.exe   (reads/decodes pcap + live UDP)
#   app_simulator  -> UniversalDataSimulator.exe        (transmits user-defined data)
# Both pull a single shared core (shared/shared.pri). Build the whole suite with:
#   qmake ../universal-data-suite.pro && mingw32-make -j4
TEMPLATE = subdirs

SUBDIRS = app_parser app_simulator

app_parser.file    = app_parser/parser.pro
app_simulator.file = app_simulator/simulator.pro
