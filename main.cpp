/**
 * @file main.cpp
 * @brief Application entry point for DubInstante.
 * 
 * DubInstante is a professional dubbing studio application for
 * recording voice-over synchronized with video playback.
 */

#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Application metadata
    app.setApplicationName("DubInstante");
    app.setApplicationVersion("1.4.0");
    app.setOrganizationName("DubInstante");
    
    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
