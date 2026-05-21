#include <QApplication>
#include <QCoreApplication>
#include "MainWindow.h"

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName("PcapUdpExtractor");
    QCoreApplication::setApplicationName("PcapUdpExtractor");
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
