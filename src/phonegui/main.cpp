#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QUrl>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  // Force software rendering to avoid MIUI/Android hwui OpenGL conflicts
  qputenv("QT_QUICK_BACKEND", "software");

  QGuiApplication app(argc, argv);

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("DubInstante", "Main");

  return app.exec();
}
