#include "MainWindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // 1. Set Style to Fusion
  a.setStyle(QStyleFactory::create("Fusion"));

  // 2. Define Light Palette
  QPalette lightPalette;
  lightPalette.setColor(QPalette::Window, Qt::white);
  lightPalette.setColor(QPalette::WindowText, Qt::black);
  lightPalette.setColor(QPalette::Base, Qt::white);
  lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
  lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
  lightPalette.setColor(QPalette::ToolTipText, Qt::black);
  lightPalette.setColor(QPalette::Text, Qt::black);
  lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
  lightPalette.setColor(QPalette::ButtonText, Qt::black);
  lightPalette.setColor(QPalette::BrightText, Qt::red);
  lightPalette.setColor(QPalette::Link, QColor(0, 120, 215));
  lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
  lightPalette.setColor(QPalette::HighlightedText, Qt::white);

  a.setPalette(lightPalette);

  // 3. Apply Stylesheet for specific controls

  MainWindow w;
  w.show();

  return a.exec();
}
