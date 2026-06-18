#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include "MainWindow.h"
#include "Themes.h"

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

    // Re-apply the theme once the window is on screen. Applying a complex
    // stylesheet before the first show leaves some nested group boxes with a
    // stale style (the long-standing startup repaint glitch); a second pass
    // after show forces a full re-polish so the app opens looking correct.
    QTimer::singleShot(0, &window, []() { Themes::applyToAllTopLevels(); });

    return app.exec();
}
