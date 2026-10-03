/**
 * @file TrackPlayer.h
 * @brief Plays the comp of one track in sync with the video.
 *
 * Two players alternate: while one plays a segment, the other already holds
 * the next one, so crossing a comp cut does not wait for a file to open.
 *
 * @note Part of the Core layer - no UI dependencies allowed.
 */

#ifndef TRACKPLAYER_H
#define TRACKPLAYER_H

#include "TakeTrack.h"

#include <QAudioDevice>
#include <QElapsedTimer>
#include <QObject>

class QAudioOutput;
class QMediaPlayer;

class TrackPlayer : public QObject {
  Q_OBJECT

public:
  explicit TrackPlayer(const QAudioDevice &device, QObject *parent = nullptr);

  void setSegments(const QList<Segment> &segments);
  void setVolume(float volume);
  void setDevice(const QAudioDevice &device);
  /// Silent from this timeline position on: the punch-in point of a track
  /// being recorded, heard up to there during the pre-roll. -1 = never.
  void setSilentFrom(qint64 timelineMs);
  /// Follows the video: call on every position and playback state change.
  void sync(qint64 masterMs, bool playing);
  /// Stops and closes every file (before they are deleted or replaced).
  void releaseFiles();

private:
  struct Deck {
    QMediaPlayer *player = nullptr;
    QAudioOutput *output = nullptr;
    int segment = -1;
  };

  int segmentAt(qint64 timelineMs) const;
  bool continues(int from, int to) const;
  void load(Deck &deck, int segment);
  static void pause(Deck &deck);

  // ponytail: QMediaPlayer starts a few tens of ms late at a cut; the export
  // is exact. Mix the takes ourselves through QAudioSink if testers hear it.
  Deck m_decks[2];
  int m_active = 0;
  QList<Segment> m_segments;
  qint64 m_silentFrom = -1;
  QElapsedTimer m_resyncThrottle;
};

#endif // TRACKPLAYER_H
