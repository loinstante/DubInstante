/**
 * @file RythmoOverlay.cpp
 * @brief Implementation of the RythmoOverlay class.
 */

#include "RythmoOverlay.h"

#include "../core/Constants.h"

#include <QPainter>

RythmoOverlay::RythmoOverlay(QWidget *parent)
    : QWidget(parent), m_layout(new QVBoxLayout(this)) {
  // Configure transparency
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);

  // Configure layout
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  // Position tracks at bottom of overlay
  m_layout->addStretch(1);

  // Create initial track (always at least 1)
  RythmoWidget *firstTrack = new RythmoWidget(this);
  m_tracks.append(firstTrack);
  m_layout->addWidget(firstTrack);
}

// =============================================================================
// Track Access
// =============================================================================

RythmoWidget *RythmoOverlay::track(int index) const {
  if (index >= 0 && index < m_tracks.size()) {
    return m_tracks[index];
  }
  return nullptr;
}

int RythmoOverlay::trackCount() const { return m_tracks.size(); }

void RythmoOverlay::setTrackCount(int count) {
  count = qBound(1, count, MAX_TRACKS);

  // Add tracks if needed
  while (m_tracks.size() < count) {
    RythmoWidget *newTrack = new RythmoWidget(this);
    newTrack->setSpeed(m_currentSpeed);
    newTrack->setPlaying(m_isPlaying);
    newTrack->setEditable(m_isEditable);
    m_tracks.append(newTrack);
    m_layout->addWidget(newTrack);
  }

  // Remove tracks if needed
  while (m_tracks.size() > count) {
    RythmoWidget *removed = m_tracks.takeLast();
    m_layout->removeWidget(removed);
    removed->deleteLater();
  }
}

// =============================================================================
// Proxy Methods
// =============================================================================

void RythmoOverlay::sync(qint64 positionMs) {
  for (RythmoWidget *track : m_tracks) {
    track->sync(positionMs);
  }
}

void RythmoOverlay::setPlaying(bool playing) {
  m_isPlaying = playing;
  for (RythmoWidget *track : m_tracks) {
    track->setPlaying(playing);
  }
}

void RythmoOverlay::setSpeed(int speed) {
  m_currentSpeed = speed;
  for (RythmoWidget *track : m_tracks) {
    track->setSpeed(speed);
  }
}

void RythmoOverlay::setEditable(bool editable) {
  m_isEditable = editable;
  for (RythmoWidget *track : m_tracks) {
    track->setEditable(editable);
  }
}

// =============================================================================
// Paint Event
// =============================================================================

void RythmoOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  // Transparent background - nothing to paint
}
