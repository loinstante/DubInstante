/**
 * @file RythmoManager.cpp
 * @brief Implementation of the RythmoManager class.
 */

#include "RythmoManager.h"

#include <algorithm>

RythmoManager::RythmoManager(QObject *parent)
    : QObject(parent), m_speed(DEFAULT_SPEED), m_currentPosition(0),
      m_lastInsertPosition(-1), m_insertOffset(0), m_cachedCharWidth(-1) {
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

  if (m_tracks[trackIndex] != text) {
    m_tracks[trackIndex] = text;
    emit textChanged(trackIndex, text);

    // Emit full track data for UI update
    RythmoTrackData data;
    data.trackIndex = trackIndex;
    data.text = text;
    data.cursorIndex = cursorIndex(m_currentPosition);
    data.positionMs = m_currentPosition;
    data.speed = m_speed;
    emit trackDataChanged(data);
  }
}

QString RythmoManager::text(int trackIndex) const {
  if (trackIndex < 0 || trackIndex >= m_tracks.size()) {
    return QString();
  }
  return m_tracks[trackIndex];
}

void RythmoManager::insertCharacter(int trackIndex, const QString &character) {
  if (trackIndex < 0) {
    return;
  }

  ensureTrackExists(trackIndex);

  int idx = cursorIndex(m_currentPosition);
  QString &trackText = m_tracks[trackIndex];

  // Reset offset if position changed since last insert
  if (m_currentPosition != m_lastInsertPosition) {
    m_insertOffset = 0;
    m_lastInsertPosition = m_currentPosition;
  }

  int actualIdx = idx + m_insertOffset;

  // Pad with spaces if cursor is beyond text length
  while (trackText.length() < actualIdx) {
    trackText.append(' ');
  }

  trackText.insert(actualIdx, character);
  m_insertOffset++; // Next character goes after this one

  emit textChanged(trackIndex, trackText);
}

void RythmoManager::deleteCharacter(int trackIndex, bool before) {
  if (trackIndex < 0 || trackIndex >= m_tracks.size()) {
    return;
  }

  int idx = cursorIndex(m_currentPosition);
  QString &trackText = m_tracks[trackIndex];

  // Account for insertion offset when calculating position
  int actualIdx = idx + m_insertOffset;

  if (before) {
    // Backspace behavior - delete character before current position
    if (actualIdx > 0 && actualIdx <= trackText.length()) {
      trackText.remove(actualIdx - 1, 1);
      if (m_insertOffset > 0) {
        m_insertOffset--; // Maintain offset alignment
      }
      emit textChanged(trackIndex, trackText);
    }
  } else {
    // Delete behavior - delete character at current position
    if (actualIdx >= 0 && actualIdx < trackText.length()) {
      trackText.remove(actualIdx, 1);
      emit textChanged(trackIndex, trackText);
    }
  }
}

int RythmoManager::trackCount() const { return m_tracks.size(); }

// =============================================================================
// Synchronization Parameters
// =============================================================================

void RythmoManager::setSpeed(int pixelsPerSecond) {
  if (pixelsPerSecond > 0 && m_speed != pixelsPerSecond) {
    m_speed = pixelsPerSecond;
    emit speedChanged(m_speed);

    // Notify all tracks of the change
    for (int i = 0; i < m_tracks.size(); ++i) {
      RythmoTrackData data;
      data.trackIndex = i;
      data.text = m_tracks[i];
      data.cursorIndex = cursorIndex(m_currentPosition);
      data.positionMs = m_currentPosition;
      data.speed = m_speed;
      emit trackDataChanged(data);
    }
  }
}

int RythmoManager::speed() const { return m_speed; }

// =============================================================================
// Position Calculations
// =============================================================================

QFont RythmoManager::getFont() const {
  if (m_cachedCharWidth == -1) {
    m_cachedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_cachedFont.setPointSize(DEFAULT_FONT_SIZE);
    m_cachedFont.setBold(true);
  }
  return m_cachedFont;
}

int RythmoManager::charWidth() const {
  if (m_cachedCharWidth == -1) {
    QFontMetrics fm(getFont());
    m_cachedCharWidth = fm.horizontalAdvance('A');
  }
  return m_cachedCharWidth;
}

int RythmoManager::cursorIndex(qint64 positionMs) const {
  int cw = charWidth();
  if (cw <= 0) {
    return 0;
  }

  double distancePixels = (static_cast<double>(positionMs) / 1000.0) * m_speed;
  return static_cast<int>(distancePixels / cw);
}

qint64 RythmoManager::charDurationMs() const {
  int cw = charWidth();
  if (cw <= 0 || m_speed <= 0) {
    return 40; // Fallback: approximately 1 frame at 25fps
  }

  return static_cast<qint64>((static_cast<double>(cw) / m_speed) * 1000.0);
}

qint64 RythmoManager::currentPosition() const { return m_currentPosition; }

void RythmoManager::invalidateFontCache() { m_cachedCharWidth = -1; }

// =============================================================================
// Synchronization
// =============================================================================

void RythmoManager::sync(qint64 positionMs) {
  if (m_currentPosition == positionMs) {
    return;
  }

  m_currentPosition = positionMs;

  // Emit updated data for all tracks
  int cursor = cursorIndex(positionMs);

  for (int i = 0; i < m_tracks.size(); ++i) {
    RythmoTrackData data;
    data.trackIndex = i;
    data.text = m_tracks[i];
    data.cursorIndex = cursor;
    data.positionMs = positionMs;
    data.speed = m_speed;
    emit trackDataChanged(data);
  }
}

void RythmoManager::requestSeek(int trackIndex, int deltaPixels) {
  Q_UNUSED(trackIndex)

  // Convert pixel delta to time delta
  double timeDeltaMs = (static_cast<double>(deltaPixels) * 1000.0) / m_speed;
  qint64 newPosition =
      std::max(qint64(0), m_currentPosition + static_cast<qint64>(timeDeltaMs));

  emit seekRequested(newPosition);
}
