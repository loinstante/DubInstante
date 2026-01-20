#ifndef RYTHMOWIDGET_H
#define RYTHMOWIDGET_H

#include <QWidget>

/**
 * @brief RythmoWidget displays a scrolling text band synchronized with video
 * position. It simulates a "Bande Rythmo" used in dubbing.
 *
 * Design: The edit cursor is ALWAYS derived from the current video time.
 * There is no separate "cursor index" state - it's purely a function of
 * (m_currentPosition, m_speed, charWidth).
 */
class RythmoWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged)

public:
  explicit RythmoWidget(QWidget *parent = nullptr);

  void setSpeed(int speed);
  int speed() const;

  void setText(const QString &text);
  QString text() const;

public slots:
  void sync(qint64 positionMs);
  void setPlaying(bool playing);

signals:
  void speedChanged(int speed);
  void scrubRequested(qint64 positionMs);
  void playRequested(); // Emitted when user wants to play (e.g., Escape key)

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  // Helpers
  QFont getFont() const;
  int charWidth() const;
  int cursorIndex() const;
  qint64 charDurationMs() const;

  // State
  int m_speed;              // Pixels per second
  qint64 m_currentPosition; // Current video time in ms
  QString m_text;

  // Interaction State
  int m_lastMouseX;

  // Visual parameters
  int m_fontSize;
  int m_verticalPadding;
  QColor m_textColor;
  QColor m_barColor;
  QColor m_playingBarColor;
  bool m_isPlaying;
};

#endif // RYTHMOWIDGET_H
