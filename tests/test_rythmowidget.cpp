// CHECK-based test for the time grid of RythmoWidget: character i sits at
// i * charMs(), and that grid must survive what used to move the sync
// (font size, speed round-trips, long typing sessions).

#include "RythmoWidget.h"
#include "check.h"

#include <QApplication>
#include <QFontMetrics>
#include <QEventLoop>
#include <QKeyEvent>
#include <QTimer>

namespace {

void press(RythmoWidget &w, int key, const QString &text = QString()) {
  QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, text);
  QApplication::sendEvent(&w, &event);
}

// The seek is debounced by 200 ms: returns the position it finally asks for,
// or -1 if nothing came. (No QSignalSpy: Qt6::Test is not a project dependency.)
qint64 waitForSeek(RythmoWidget &w) {
  qint64 position = -1;
  QEventLoop loop;
  QObject::connect(&w, &RythmoWidget::seekRequested, &loop, [&](qint64 ms) {
    position = ms;
    loop.quit();
  });
  QTimer::singleShot(1000, &loop, &QEventLoop::quit);
  loop.exec();
  return position;
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  RythmoTrackStyle style;
  const int cw16 = QFontMetrics(style.font).horizontalAdvance('A');
  RythmoTrackStyle big = style;
  big.globalSize = 24;
  big.font.setPointSize(24);
  const int cw24 = QFontMetrics(big.font).horizontalAdvance('A');
  CHECK(cw16 > 0 && cw24 > cw16);

  // --- 1. Empty band: the grid follows font and speed (legacy behaviour) ---
  {
    RythmoWidget w;
    CHECK(w.charMs() == cw16 * 1000.0 / 100);
    w.setTrackStyle(big);
    CHECK(w.charMs() == cw24 * 1000.0 / 100);
    w.setSpeed(200);
    CHECK(qFuzzyCompare(w.charMs(), cw24 * 1000.0 / 200));
  }

  // --- 2. Band with text: a size change no longer moves the sync ---
  {
    RythmoWidget w;
    w.setText("Bonjour");
    const double grid = w.charMs();
    w.setTrackStyle(big);
    CHECK(w.charMs() == grid);
    w.setTrackStyle(style);
    CHECK(w.charMs() == grid);

    // Speed rescales the grid and comes back to it
    w.setSpeed(137);
    CHECK(qFuzzyCompare(w.charMs(), grid * 100 / 137));
    w.setSpeed(100);
    CHECK(qFuzzyCompare(w.charMs(), grid));
  }

  // --- 3. A stored grid wins over the local font metrics; <= 0 derives it ---
  {
    RythmoWidget w;
    w.setText("x");
    w.setCharMs(96.5);
    CHECK(w.charMs() == 96.5);
    w.setCharMs(0.0);
    CHECK(w.charMs() == cw16 * 1000.0 / 100);
  }

  // --- 4. Typing stays on the grid: 108.33 ms cells, 300 characters.
  // Adding a truncated 108 ms per key used to end 100 ms (one cell) early.
  {
    RythmoWidget w;
    w.setCharMs(13 * 1000.0 / 120);
    int textChanges = 0;
    QObject::connect(&w, &RythmoWidget::textChanged, &w,
                     [&textChanges]() { ++textChanges; });
    const QString line = QString("abcdefghij").repeated(30);
    for (const QChar c : line)
      press(w, 0, QString(c));
    CHECK(w.text() == line); // no character skipped or swapped
    CHECK(textChanges == line.length());
    CHECK(waitForSeek(w) == qRound64(300 * w.charMs()));

    // Walking back 300 cells lands exactly on 0, then stays there
    for (int i = 0; i < 301; ++i)
      press(w, Qt::Key_Left);
    CHECK(waitForSeek(w) == 0);
  }

  return 0;
}
