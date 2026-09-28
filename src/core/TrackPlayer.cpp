#include "TrackPlayer.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoSink>
#include <algorithm>

TrackPlayer::TrackPlayer(const QAudioDevice &device, QObject *parent)
    : QObject(parent) {
  for (Deck &deck : m_decks) {
    deck.output = new QAudioOutput(this);
    deck.output->setDevice(device);
    deck.output->setVolume(0.8f); // matches TrackWidget default slider
    deck.player = new QMediaPlayer(this);
    deck.player->setAudioOutput(deck.output);
    deck.player->setVideoSink(new QVideoSink(deck.player)); // no window for audio
  }
}

void TrackPlayer::setSegments(const QList<Segment> &segments) {
  m_segments = segments;
  for (Deck &deck : m_decks) {
    pause(deck);
    deck.segment = -1; // sources are kept: load() skips reopening the same file
  }
}

void TrackPlayer::setVolume(float volume) {
  for (Deck &deck : m_decks)
    deck.output->setVolume(volume);
}

void TrackPlayer::setDevice(const QAudioDevice &device) {
  for (Deck &deck : m_decks)
    deck.output->setDevice(device);
}

void TrackPlayer::setSilentFrom(qint64 timelineMs) { m_silentFrom = timelineMs; }

void TrackPlayer::releaseFiles() {
  for (Deck &deck : m_decks) {
    deck.player->stop();
    deck.player->setSource(QUrl()); // Windows refuses to delete an open file
    deck.segment = -1;
  }
}

void TrackPlayer::sync(qint64 masterMs, bool playing) {
  int current = segmentAt(masterMs);
  if (m_silentFrom >= 0 && masterMs >= m_silentFrom)
    current = -1;

  if (!playing || current < 0) {
    for (Deck &deck : m_decks)
      pause(deck);
    // Ready for the next start: the segment under the playhead, or the next one
    const auto next = std::upper_bound(
        m_segments.begin(), m_segments.end(), masterMs,
        [](qint64 t, const Segment &s) { return t < s.timelineStartMs; });
    const int upcoming = current >= 0 ? current : int(next - m_segments.begin());
    if (upcoming < m_segments.size() && m_decks[0].segment != upcoming &&
        m_decks[1].segment != upcoming)
      load(m_decks[m_active], upcoming);
    return;
  }

  Deck *deck = &m_decks[m_active];
  if (deck->segment != current) {
    const bool playingOn = deck->segment >= 0 && continues(deck->segment, current) &&
                           deck->player->playbackState() == QMediaPlayer::PlayingState;
    if (playingOn) {
      deck->segment = current; // a cut between two pieces of one file: no gap
    } else {
      Deck &other = m_decks[1 - m_active];
      load(other, current);
      pause(*deck);
      m_active = 1 - m_active;
      deck = &other;
    }
  }

  const Segment &segment = m_segments[current];
  const qint64 target = segment.sourceOffsetMs + masterMs - segment.timelineStartMs;
  if (deck->player->playbackState() != QMediaPlayer::PlayingState) {
    deck->player->setPosition(target);
    deck->player->play();
  } else if (qAbs(deck->player->position() - target) > 150 &&
             (!m_resyncThrottle.isValid() || m_resyncThrottle.elapsed() >= 1000)) {
    deck->player->setPosition(target);
    m_resyncThrottle.restart();
  }

  const int next = current + 1;
  if (next < m_segments.size() && !continues(current, next))
    load(m_decks[1 - m_active], next);
}

int TrackPlayer::segmentAt(qint64 timelineMs) const {
  const auto it = std::upper_bound(
      m_segments.begin(), m_segments.end(), timelineMs,
      [](qint64 t, const Segment &s) { return t < s.timelineStartMs; });
  if (it == m_segments.begin())
    return -1;
  const int index = int(it - m_segments.begin()) - 1;
  return timelineMs < m_segments[index].timelineEndMs() ? index : -1;
}

bool TrackPlayer::continues(int from, int to) const {
  const Segment &a = m_segments[from];
  const Segment &b = m_segments[to];
  return a.file == b.file && b.timelineStartMs == a.timelineEndMs() &&
         b.sourceOffsetMs == a.sourceOffsetMs + a.durationMs;
}

void TrackPlayer::load(Deck &deck, int segment) {
  if (deck.segment == segment)
    return;
  const QUrl source = QUrl::fromLocalFile(m_segments[segment].file);
  if (deck.player->source() != source)
    deck.player->setSource(source);
  deck.segment = segment;
}

void TrackPlayer::pause(Deck &deck) {
  if (deck.player->playbackState() == QMediaPlayer::PlayingState)
    deck.player->pause();
}
