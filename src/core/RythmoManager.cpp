/**
 * @file RythmoManager.cpp
 * @brief Implementation of the RythmoManager class.
 */

#include "RythmoManager.h"

RythmoTrackStyle::RythmoTrackStyle()
    : font(QFontDatabase::systemFont(QFontDatabase::FixedFont)),
      textColor(Qt::white),
      backgroundColor(
          QColor(40, 40, 40)), // Classic style has usually a dark background
      globalSize(16) {
  font.setPointSize(globalSize);
  font.setBold(true);
}

RythmoManager::RythmoManager(QObject *parent)
    : QObject(parent), m_speed(DEFAULT_SPEED) {
  // Initialize with at least 2 tracks (common use case)
  m_tracks.reserve(2);
}

// =============================================================================
// Track Management
// =============================================================================

void RythmoManager::ensureTrackExists(int trackIndex) {
  if (trackIndex < 0) {
    return;
  }

  while (m_tracks.size() <= trackIndex) {
    m_tracks.append(QString());
  }
}

void RythmoManager::setText(int trackIndex, const QString &text) {
  if (trackIndex < 0) {
    return;
  }

  ensureTrackExists(trackIndex);

  m_tracks[trackIndex] = text;
}

QString RythmoManager::text(int trackIndex) const {
  if (trackIndex < 0 || trackIndex >= m_tracks.size()) {
    return QString();
  }
  return m_tracks[trackIndex];
}

void RythmoManager::setTrackStyle(int trackIndex,
                                  const RythmoTrackStyle &style) {
  if (trackIndex < 0)
    return;

  ensureTrackExists(trackIndex);
  m_trackStyles[trackIndex] = style;
  emit trackStyleChanged(trackIndex, style);
}

RythmoTrackStyle RythmoManager::trackStyle(int trackIndex) const {
  return m_trackStyles.value(trackIndex, RythmoTrackStyle());
}

int RythmoManager::trackCount() const { return m_tracks.size(); }

// =============================================================================
// Synchronization Parameters
// =============================================================================

void RythmoManager::setSpeed(int pixelsPerSecond) {
  if (pixelsPerSecond > 0) {
    m_speed = pixelsPerSecond;
  }
}

int RythmoManager::speed() const { return m_speed; }
