#include <QApplication>
#include <QCoreApplication>
#include "SimulatorWindow.h"

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName("UniversalDataSimulator");
    QCoreApplication::setApplicationName("UniversalDataSimulator");
    QApplication app(argc, argv);
    SimulatorWindow window;
    window.show();
    return app.exec();
}
