#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Enable high DPI scaling for modern displays
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    QApplication app(argc, argv);
    
    // Set application metadata
    app.setApplicationName("Bluetooth Telemetry Server");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("Telemetry Systems");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
