/**
 * @file RythmoOverlay.h
 * @brief Container widget for multiple RythmoWidget tracks.
 *
 * This widget manages the display of one or more Rythmo tracks,
 * positioned as overlays on the video display area.
 *
 * @note Part of the GUI layer - pure layout and forwarding.
 */

#ifndef RYTHMOOVERLAY_H
#define RYTHMOOVERLAY_H

#include "RythmoWidget.h"

#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

/**
 * @class RythmoOverlay
 * @brief Container for multiple RythmoWidget tracks.
 *
 * Features:
 * - Manages 1 to MAX_TRACKS RythmoWidget instances
 * - Provides proxy methods for convenience
 * - Handles layout and visibility of tracks
 */
class RythmoOverlay : public QWidget {
  Q_OBJECT

public:
  explicit RythmoOverlay(QWidget *parent = nullptr);
  ~RythmoOverlay() override = default;

  // =========================================================================
  // Track Access
  // =========================================================================

  /** @brief Returns pointer to track widget at given index, or nullptr. */
  RythmoWidget *track(int index) const;

  /** @brief Returns the current number of tracks. */
  int trackCount() const;

  /** @brief Sets the number of visible tracks (1 to MAX_TRACKS). */
  void setTrackCount(int count);

public slots:
  // =========================================================================
  // Proxy Methods (forward to all tracks)
  // =========================================================================

  /** @brief Syncs all tracks to the given position. */
  void sync(qint64 positionMs);

  /** @brief Sets playing state for all tracks. */
  void setPlaying(bool playing);

  /** @brief Sets scrolling speed for all tracks. */
  void setSpeed(int speed);

  /** @brief Enable/disable text editing on all tracks. */
  void setEditable(bool editable);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QVector<RythmoWidget *> m_tracks;
  QVBoxLayout *m_layout;
  
  // State for newly created tracks
  int m_currentSpeed = 100;
  bool m_isPlaying = false;
  bool m_isEditable = true;
};

#endif // RYTHMOOVERLAY_H
