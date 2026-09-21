/**
 * @file RythmoManager.h
 * @brief Per-track store for Rythmo text and styles.
 *
 * Computes nothing: rendering and editing live in RythmoWidget, which pushes
 * every text change back through setText().
 *
 * @note Part of the Core layer - no UI dependencies allowed.
 */

#ifndef RYTHMOMANAGER_H
#define RYTHMOMANAGER_H

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @struct RythmoTrackStyle
 * @brief Style data for a single track.
 */
struct RythmoTrackStyle {
  QFont font;
  QColor textColor;
  QColor backgroundColor;
  int globalSize;

  RythmoTrackStyle();
};

/**
 * @class RythmoManager
 * @brief Stores the text and style of each Rythmo track.
 *
 * Pure storage: no synchronization, cursor or seek logic. RythmoWidget owns
 * rendering and editing and pushes text changes back through setText().
 */
class RythmoManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(int speed READ speed WRITE setSpeed)

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
   * @brief Sets the style for a specific track.
   * @param trackIndex Index of the track.
   * @param style The new style to apply.
   */
  void setTrackStyle(int trackIndex, const RythmoTrackStyle &style);

  /**
   * @brief Gets the current style of a track.
   * @param trackIndex Index of the track.
   * @return The style of the track, or a default style if it doesn't exist.
   */
  RythmoTrackStyle trackStyle(int trackIndex) const;

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

signals:
  /**
   * @brief Emitted when the style of a track changes.
   * @param trackIndex Which track changed.
   * @param style The new style applied.
   */
  void trackStyleChanged(int trackIndex, const RythmoTrackStyle &style);

private:
  /**
   * @brief Ensures the tracks vector has at least (index + 1) elements.
   * @param trackIndex Required track index.
   */
  void ensureTrackExists(int trackIndex);

  // =========================================================================
  // State
  // =========================================================================

  QVector<QString> m_tracks;                 ///< Dynamic list of track texts
  QMap<int, RythmoTrackStyle> m_trackStyles; ///< Track specific styles
  int m_speed;                               ///< Scrolling speed (pixels/second)

  // Configuration
  static constexpr int DEFAULT_FONT_SIZE = 16;
  static constexpr int DEFAULT_SPEED = 100;
};

#endif // RYTHMOMANAGER_H
