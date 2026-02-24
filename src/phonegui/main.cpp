#include <QGuiApplication>
#include <QQuickView>
#include <QUrl>

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);

  QQuickView view;
  view.setSource(QUrl(QStringLiteral("qrc:/qt/qml/DubInstante/Main.qml")));
  view.setResizeMode(QQuickView::SizeRootObjectToView);
  view.show();

  return app.exec();
}
