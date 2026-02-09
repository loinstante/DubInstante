/**
 * @file RythmoWidget.h
 * @brief Passive rendering widget for the Rythmo band display.
 *
 * This widget displays a scrolling text band synchronized with video position.
 * It receives all data from RythmoManager and performs NO calculations itself.
 *
 * Design Principles:
 * - Passive: All state comes via slots, no internal calculations
 * - Emits signals for user interactions (clicks, drags, key presses)
 * - RythmoManager handles all synchronization logic
 *
 * @note Part of the GUI layer - pure rendering, no business logic.
 */

#ifndef RYTHMOWIDGET_H
#define RYTHMOWIDGET_H

#include <QColor>
#include <QFont>
#include <QTimer>
#include <QWidget>

// Forward declaration to avoid including core header in GUI
struct RythmoTrackData;

/**
 * @class RythmoWidget
 * @brief Displays a single Rythmo track with scrolling text.
 *
 * The widget is completely passive - it renders what it's told.
 * All interaction events are forwarded as signals.
 */
class RythmoWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged)

public:
  /**
   * @enum VisualStyle
   * @brief Defines the visual appearance for unified multi-track display.
   */
  enum VisualStyle {
    Standalone,   ///< Full borders and header
    UnifiedTop,   ///< Top track in unified display
    UnifiedBottom ///< Bottom track in unified display
  };
  Q_ENUM(VisualStyle)

  explicit RythmoWidget(QWidget *parent = nullptr);
  ~RythmoWidget() override = default;

  // =========================================================================
  // Display Configuration
  // =========================================================================

  void setVisualStyle(VisualStyle style);
  VisualStyle visualStyle() const;

  void setTextColor(const QColor &color);
  void setSpeed(int speed);
  int speed() const;

public slots:
  // =========================================================================
  // Data Input (from RythmoManager)
  // =========================================================================

  /**
   * @brief Updates the display with new track data.
   * @param cursorIndex Character index for cursor position.
   * @param positionMs Current time position in milliseconds.
   * @param text Text content to display.
   * @param speed Scrolling speed in pixels/second.
   */
  void updateDisplay(int cursorIndex, qint64 positionMs, const QString &text,
                     int speed);

  /**
   * @brief Updates only the position (for smooth sync).
   * @param cursorIndex Character index for cursor position.
   * @param positionMs Current time position in milliseconds.
   */
  void updatePosition(int cursorIndex, qint64 positionMs);

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
   * @brief Emitted when user clicks/drags to scrub.
   * @param deltaPixels Pixel offset from target line (positive = right).
   */
  void scrubRequested(int deltaPixels);

  /**
   * @brief Emitted when user requests a direct position.
   * @param positionMs Target position in milliseconds.
   */
  void seekRequested(qint64 positionMs);

  /**
   * @brief Emitted when user types a character.
   * @param character The typed character(s).
   */
  void characterTyped(const QString &character);

  /**
   * @brief Emitted when user presses Backspace.
   */
  void backspacePressed();

  /**
   * @brief Emitted when user presses Delete.
   */
  void deletePressed();

  /**
   * @brief Emitted when user presses arrow keys.
   * @param forward True for right arrow, false for left.
   */
  void navigationRequested(bool forward);

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

private:
  // Helpers
  QFont getFont() const;
  int charWidth() const;
  int cursorIndex() const;
  qint64 charDurationMs() const;
  void requestDebouncedSeek(qint64 positionMs);
  void triggerSeek();

  // =========================================================================
  // Display State (set externally)
  // =========================================================================

  QString m_text;
  int m_cursorIndex;
  qint64 m_currentPosition;
  int m_speed;
  bool m_isPlaying;

  // Visual configuration
  VisualStyle m_visualStyle;
  int m_fontSize;
  int m_verticalPadding;
  QColor m_textColor;
  QColor m_barColor;
  QColor m_playingBarColor;

  // Interaction state
  int m_lastMouseX;

  // Font cache
  mutable QFont m_cachedFont;
  mutable int m_cachedCharWidth;

  // Seek debouncing
  QTimer *m_seekTimer;
  qint64 m_pendingSeekPosition;

  // Animation & Smoothness
  QTimer *m_animationTimer;
  qint64 m_lastSyncPosition = 0;
  qint64 m_lastSyncTime = 0; // System time (ms) at last sync

private slots:
  void animate();
};

#endif // RYTHMOWIDGET_H
