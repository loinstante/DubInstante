/**
 * @file RythmoManager.h
 * @brief Core manager for Rythmo band synchronization and text management.
 *
 * This class handles the business logic for the "Bande Rythmo" - the scrolling
 * text band used in dubbing. It manages:
 * - Multiple tracks (dynamically scalable)
 * - Time-to-position synchronization calculations
 * - Text content for each track
 *
 * The UI (RythmoWidget) receives pre-calculated values via signals, keeping
 * all computation in this Core layer.
 *
 * @note Part of the Core layer - no UI dependencies allowed.
 */

#ifndef RYTHMOMANAGER_H
#define RYTHMOMANAGER_H

#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @struct RythmoTrackData
 * @brief Data structure emitted to UI for rendering a track.
 */
struct RythmoTrackData {
  int trackIndex;
  QString text;
  int cursorIndex;
  qint64 positionMs;
  int speed;
};

/**
 * @class RythmoManager
 * @brief Manages synchronization logic and text for multiple Rythmo tracks.
 *
 * Design Principles:
 * - Scalable: Supports any number of tracks via dynamic QVector
 * - Pure Logic: No rendering, only calculations
 * - Observable: All state changes emit signals for UI binding
 *
 * @example
 * @code
 * auto manager = new RythmoManager(this);
 * manager->setSpeed(100);  // 100 pixels/second
 * manager->setText(0, "First track text");
 * manager->setText(1, "Second track text");
 *
 * connect(playbackEngine, &PlaybackEngine::positionChanged,
 *         manager, &RythmoManager::sync);
 * connect(manager, &RythmoManager::trackDataChanged,
 *         rythmoWidget, &RythmoWidget::updateTrackDisplay);
 * @endcode
 */
class RythmoManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged)

public:
  explicit RythmoManager(QObject *parent = nullptr);
  ~RythmoManager() override = default;

  // =========================================================================
  // Track Management
  // =========================================================================

  /**
   * @brief Sets the text content for a specific track.
   * @param trackIndex Index of the track (0-based). Vector auto-expands if
   * needed.
   * @param text The dubbing text to display on this track.
   */
  void setText(int trackIndex, const QString &text);

  /**
   * @brief Gets the text content of a specific track.
   * @param trackIndex Index of the track (0-based).
   * @return The text content, or empty string if track doesn't exist.
   */
  QString text(int trackIndex) const;

  /**
   * @brief Inserts a character at the cursor position for a track.
   * @param trackIndex Index of the track.
   * @param character The character to insert.
   */
  void insertCharacter(int trackIndex, const QString &character);

  /**
   * @brief Deletes a character at the cursor position for a track.
   * @param trackIndex Index of the track.
   * @param before If true, deletes character before cursor (Backspace
   * behavior).
   */
  void deleteCharacter(int trackIndex, bool before = true);

  /**
   * @brief Returns the number of active tracks.
   */
  int trackCount() const;

  // =========================================================================
  // Synchronization Parameters
  // =========================================================================

  /**
   * @brief Sets the scrolling speed in pixels per second.
   * @param pixelsPerSecond The speed value (typically 50-200).
   */
  void setSpeed(int pixelsPerSecond);

  /** @brief Returns the current scrolling speed. */
  int speed() const;

  // =========================================================================
  // Position Calculations
  // =========================================================================

  /**
   * @brief Calculates the cursor index for a given time position.
   * @param positionMs Time position in milliseconds.
   * @return Character index where the cursor should be.
   */
  int cursorIndex(qint64 positionMs) const;

  /**
   * @brief Calculates the duration of one character in milliseconds.
   * @return Duration in ms based on current speed and font metrics.
   */
  qint64 charDurationMs() const;

  /**
   * @brief Returns the character width in pixels (cached).
   */
  int charWidth() const;

  /**
   * @brief Returns the current playback position.
   */
  qint64 currentPosition() const;

public slots:
  /**
   * @brief Synchronizes the manager to a new playback position.
   * @param positionMs Current playback position in milliseconds.
   *
   * This is the main sync point - call this when video position changes.
   * Emits trackDataChanged for each track with updated cursor positions.
   */
  void sync(qint64 positionMs);

  /**
   * @brief Requests a seek operation based on user interaction.
   * @param trackIndex Track where the interaction occurred.
   * @param deltaPixels Pixel offset from current position (positive = forward).
   */
  void requestSeek(int trackIndex, int deltaPixels);

signals:
  /**
   * @brief Emitted when track data changes and UI needs update.
   * @param data Complete data structure for UI rendering.
   */
  void trackDataChanged(const RythmoTrackData &data);

  /**
   * @brief Emitted when text content of a track changes.
   * @param trackIndex Which track changed.
   * @param text New text content.
   */
  void textChanged(int trackIndex, const QString &text);

  /** @brief Emitted when speed parameter changes. */
  void speedChanged(int speed);

  /**
   * @brief Emitted to request video seek.
   * @param positionMs Target position in milliseconds.
   */
  void seekRequested(qint64 positionMs);

private:
  /**
   * @brief Ensures the tracks vector has at least (index + 1) elements.
   * @param trackIndex Required track index.
   */
  void ensureTrackExists(int trackIndex);

  /**
   * @brief Invalidates the cached font metrics.
   */
  void invalidateFontCache();

  /**
   * @brief Gets or creates the cached font.
   */
  QFont getFont() const;

  // =========================================================================
  // State
  // =========================================================================

  QVector<QString> m_tracks; ///< Dynamic list of track texts
  int m_speed;               ///< Scrolling speed (pixels/second)
  qint64 m_currentPosition;  ///< Current playback position (ms)

  // Insertion tracking (for correct character order)
  qint64 m_lastInsertPosition; ///< Position when last insert occurred
  int m_insertOffset;          ///< Offset for consecutive inserts

  // Font metrics cache
  mutable QFont m_cachedFont;
  mutable int m_cachedCharWidth;

  // Configuration
  static constexpr int DEFAULT_FONT_SIZE = 16;
  static constexpr int DEFAULT_SPEED = 100;
};

#endif // RYTHMOMANAGER_H
