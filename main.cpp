/**
 * @file main.cpp
 * @brief Application entry point for DubInstante.
 * 
 * DubInstante is a professional dubbing studio application for
 * recording voice-over synchronized with video playback.
 */

#include "MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor("#f3f4f6"));
    lightPalette.setColor(QPalette::WindowText, QColor("#1f2937"));
    lightPalette.setColor(QPalette::Base, QColor("#ffffff"));
    lightPalette.setColor(QPalette::AlternateBase, QColor("#f8fafc"));
    lightPalette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
    lightPalette.setColor(QPalette::ToolTipText, QColor("#111827"));
    lightPalette.setColor(QPalette::Text, QColor("#111827"));
    lightPalette.setColor(QPalette::Button, QColor("#f8fafc"));
    lightPalette.setColor(QPalette::ButtonText, QColor("#334155"));
    lightPalette.setColor(QPalette::BrightText, QColor("#ffffff"));
    lightPalette.setColor(QPalette::Link, QColor("#3b82f6"));
    lightPalette.setColor(QPalette::Highlight, QColor("#3b82f6"));
    lightPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    app.setPalette(lightPalette);
    
    // Application metadata
    app.setApplicationName("DubInstante");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("DubInstante");
    
    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
