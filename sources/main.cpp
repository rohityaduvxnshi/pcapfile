#include <QApplication>
#include <QCoreApplication>
#include "MainWindow.h"

int main(int argc, char** argv)
{
    // Internal IDs deliberately stay "PcapUdpExtractor" so existing QSettings
    // (ui/theme) and AppData (project sidecars, ICD mapping profiles) keep
    // working after the rename. The display name lives in forms/MainWindow.ui.
    QCoreApplication::setOrganizationName("PcapUdpExtractor");
    QCoreApplication::setApplicationName("PcapUdpExtractor");
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
