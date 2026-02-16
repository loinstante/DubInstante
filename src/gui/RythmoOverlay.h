/**
 * @file RythmoOverlay.h
 * @brief Container widget for multiple RythmoWidget tracks.
 *
 * This widget manages the display of one or two Rythmo tracks,
 * positioned as overlays on the video display area.
 *
 * @note Part of the GUI layer - pure layout and forwarding.
 */

#ifndef RYTHMOOVERLAY_H
#define RYTHMOOVERLAY_H

#include "RythmoWidget.h"

#include <QVBoxLayout>
#include <QWidget>

/**
 * @class RythmoOverlay
 * @brief Container for multiple RythmoWidget tracks.
 *
 * Features:
 * - Manages Track 1 and optional Track 2
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

  /** @brief Returns pointer to Track 1 widget. */
  RythmoWidget *track1() const;

  /** @brief Returns pointer to Track 2 widget. */
  RythmoWidget *track2() const;

  /** @brief Shows or hides Track 2. */
  void setTrack2Visible(bool visible);

  /** @brief Returns whether Track 2 is visible. */
  bool isTrack2Visible() const;

public slots:
  // =========================================================================
  // Proxy Methods (forward to both tracks)
  // =========================================================================

  /** @brief Syncs both tracks to the given position. */
  void sync(qint64 positionMs);

  /** @brief Sets playing state for both tracks. */
  void setPlaying(bool playing);

  /** @brief Sets scrolling speed for both tracks. */
  void setSpeed(int speed);

  /** @brief Sets text color for both tracks. */
  void setTextColor(const QColor &color);

  /** @brief Enable/disable text editing on both tracks. */
  void setEditable(bool editable);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  RythmoWidget *m_rythmo1;
  RythmoWidget *m_rythmo2;
  QVBoxLayout *m_layout;
};

#endif // RYTHMOOVERLAY_H
