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
  a.setStyleSheet(
      "QToolTip { color: black; background-color: white; border: 1px solid "
      "#ccc; }"
      "QMainWindow { background-color: white; }"
      "QWidget { font-family: 'Segoe UI', 'Roboto', sans-serif; font-size: "
      "10pt; color: #333; }"

      // GroupBoxes & Panels
      "QGroupBox { border: 1px solid #ddd; margin-top: 1ex; border-radius: "
      "4px; }"
      "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top "
      "center; padding: 0 3px; color: #666; }"

      // Buttons
      "QPushButton { background-color: #f5f5f5; border: 1px solid #ccc; "
      "border-radius: 4px; padding: 2px 8px; color: #333; }"
      "QPushButton:hover { background-color: #e5e5e5; border-color: #bbb; }"
      "QPushButton:pressed { background-color: #0078d7; color: white; "
      "border-color: #0078d7; }"
      "QPushButton:disabled { background-color: #eee; color: #999; "
      "border-color: #ddd; }"

      // LineEdit & SpinBox
      "QLineEdit, QSpinBox { background-color: white; border: 1px solid "
      "#ccc; border-radius: 3px; padding: 3px; color: #333; "
      "selection-background-color: #0078d7; }"
      "QLineEdit:focus, QSpinBox:focus { border: 1px solid #0078d7; }"

      // Sliders (Light Pro Look)
      "QSlider::groove:horizontal { border: 1px solid #ddd; height: 6px; "
      "background: #f0f0f0; margin: 2px 0; border-radius: 3px; }"
      "QSlider::handle:horizontal { background: #fff; border: 1px solid "
      "#bbb; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; "
      "}"
      "QSlider::handle:horizontal:hover { background: #0078d7; border-color: "
      "#0078d7; }"
      "QSlider::sub-page:horizontal { background: #0078d7; border-radius: 3px; "
      "}"

      // ProgressBar
      "QProgressBar { border: 1px solid #ccc; border-radius: 3px; text-align: "
      "center; background: #eee; }"
      "QProgressBar::chunk { background-color: #0078d7; width: 10px; }"

      // Labels
      "QLabel { color: #333; }");

  MainWindow w;
  w.show();

  return a.exec();
}
