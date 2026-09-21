/**
 * @file RythmoWidget.h
 * @brief Rendering and editing widget for one track of the Rythmo band.
 *
 * Draws a scrolling text band synchronized with the video position, and owns
 * the editing of its text: typing, Backspace, Delete and the cursor-index
 * computation are done in keyPressEvent. Each edit emits textChanged(); the
 * owner (MainWindow) stores it in RythmoManager, which is a plain text/style
 * store. Seeks and play requests are emitted as signals.
 *
 * @note Part of the GUI layer.
 */

#ifndef RYTHMOWIDGET_H
#define RYTHMOWIDGET_H

#include "../core/RythmoManager.h"
#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QTimer>
#include <QWidget>

/**
 * @class RythmoWidget
 * @brief Displays a single Rythmo track with scrolling text.
 *
 * Position and speed are pushed in (sync / setSpeed). Character i sits at
 * i * charMs() on the timeline: that grid is set from the font and the speed
 * while the band is empty, then kept and saved with the project, so a font,
 * size or machine change rescales the band on screen without moving the sync.
 * Text edits are reported through textChanged(), seeks through seekRequested().
 */
class RythmoWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged)

public:
  explicit RythmoWidget(QWidget *parent = nullptr);
  ~RythmoWidget() override = default;

  // =========================================================================
  // Display Configuration
  // =========================================================================

  void setTrackStyle(const RythmoTrackStyle &style);
  RythmoTrackStyle trackStyle() const;

  /** @brief Nominal speed in pixels/second; a change rescales the time grid. */
  void setSpeed(int speed);
  int speed() const;

  /** @brief Duration of one character in ms (the time grid of this band). */
  double charMs() const;
  /** @brief Restores a saved time grid; <= 0 derives it from font and speed. */
  void setCharMs(double ms);

  /** @brief Enable/disable text editing on this band. */
  void setEditable(bool editable);
  bool isEditable() const;

signals:
  void textChanged(const QString &text);

public slots:
  // =========================================================================
  // Data Input
  // =========================================================================

  /**
   * @brief Updates the display with new track data.
   * @param positionMs Current time position in milliseconds.
   * @param text Text content to display.
   * @param speed Scrolling speed in pixels/second.
   */
  void updateDisplay(qint64 positionMs, const QString &text, int speed);

  /**
   * @brief Sets the playing state for visual feedback.
   * @param playing True if video is playing.
   */
  void setPlaying(bool playing);

  /**
   * @brief Legacy sync method for compatibility.
   */
  void sync(qint64 positionMs);

  /**
   * @brief Sets the text directly (for backward compatibility).
   */
  void setText(const QString &text);
  QString text() const;

signals:
  // =========================================================================
  // User Interaction Signals
  // =========================================================================

  /**
   * @brief Emitted when user requests a direct position.
   * @param positionMs Target position in milliseconds.
   */
  void seekRequested(qint64 positionMs);

  /**
   * @brief Emitted when user presses Escape (insert space + play).
   */
  void playRequested();

  /**
   * @brief Emitted when speed changes.
   */
  void speedChanged(int speed);

protected:
  void paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  // Helpers
  bool isDarkTheme();
  double charWidth() const;
  double nominalCharMs() const;
  double pixelsPerMs() const;
  int cursorIndex() const;
  qint64 timeAtIndex(int index) const;
  void requestDebouncedSeek(qint64 positionMs);
  void triggerSeek();

  // =========================================================================
  // Display State (set externally)
  // =========================================================================

  QString m_text;
  qint64 m_currentPosition;
  int m_speed;
  double m_charMs;
  bool m_isPlaying;
  bool m_editable;

  // Visual configuration
  RythmoTrackStyle m_style;
  QColor m_barColor;
  QColor m_playingBarColor;
  int m_isDark = -1; // Cached theme lookup; -1 = recompute (theme/palette change)

  // Interaction state
  int m_lastMouseX;

  // Font cache
  mutable double m_cachedCharWidth;

  // Seek debouncing
  QTimer *m_seekTimer;
  qint64 m_pendingSeekPosition;

  // Animation & Smoothness
  QTimer *m_animationTimer;
  qint64 m_lastSyncPosition = 0;
  qint64 m_lastSyncTime = 0;  // Monotonic time (ms) at last sync
  QElapsedTimer m_syncClock; // Monotonic clock, immune to system time changes

private slots:
  void animate();
};

#endif // RYTHMOWIDGET_H
