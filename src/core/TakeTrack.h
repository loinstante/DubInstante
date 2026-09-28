/**
 * @file TakeTrack.h
 * @brief Takes recorded on one track and the comp choosing which one is heard.
 *
 * A take is an immutable recorded file placed on the master timeline. The comp
 * splits the timeline into regions, each playing one take (or silence).
 * Recording assigns the new take to its punch range, so "newest wins" is the
 * default without any implicit rule; the user then re-picks takes per region.
 *
 * @note Part of the Core layer - no UI dependencies allowed. Value type: an
 * edit is a copy, which keeps a future undo stack a before/after snapshot.
 */

#ifndef TAKETRACK_H
#define TAKETRACK_H

#include <QList>
#include <QString>

/**
 * @struct Take
 * @brief One recorded file. Audible range: [startMs + inMs, startMs + durationMs).
 */
struct Take {
  int id = 0;              ///< Unique within the project, never 0
  QString file;            ///< Session WAV at runtime, project-relative path on disk
  qint64 startMs = 0;      ///< Timeline position of the first sample
  qint64 inMs = 0;         ///< Pre-roll: audio before it is never heard
  qint64 durationMs = 0;   ///< Length of the file

  qint64 audibleStartMs() const { return startMs + inMs; }
  qint64 audibleEndMs() const { return startMs + durationMs; }
};

/**
 * @struct CompRegion
 * @brief Region i spans [startMs, next region's startMs), the last one runs forever.
 */
struct CompRegion {
  qint64 startMs = 0;
  int takeId = 0;          ///< 0 = silence
};

/**
 * @struct Segment
 * @brief A piece of take actually heard: what playback and export consume.
 */
struct Segment {
  int takeId = 0;
  QString file;
  qint64 timelineStartMs = 0;
  qint64 sourceOffsetMs = 0;   ///< Position inside the file
  qint64 durationMs = 0;

  qint64 timelineEndMs() const { return timelineStartMs + durationMs; }
};

class TakeTrack {
public:
  TakeTrack();

  /// Rebuilds a track read from a file: unknown take ids fall back to silence,
  /// regions are sorted and start at 0.
  static TakeTrack fromParts(const QList<Take> &takes, const QList<CompRegion> &regions);

  const QList<Take> &takes() const { return m_takes; }
  const QList<CompRegion> &regions() const { return m_regions; }
  const Take *take(int id) const;
  bool isEmpty() const { return m_takes.isEmpty(); }
  int maxTakeId() const;

  /// Adds the take and makes it heard on [punchInMs, punchOutMs), clamped to its audible range.
  void addRecording(const Take &take, qint64 punchInMs, qint64 punchOutMs);
  /// Cuts the region under timeMs in two; both halves keep their take.
  void split(qint64 timeMs);
  /// Removes the cut starting at exactly timeMs; the left region extends over it.
  void removeCut(qint64 timeMs);
  /// The region under timeMs plays takeId.
  void choose(qint64 timeMs, int takeId);
  /// Drops the take; its regions fall back to the newest remaining take covering them.
  void removeTake(int takeId);
  /// Every file path, e.g. to relocate takes after a load.
  void setTakeFile(int takeId, const QString &file);

  int regionIndexAt(qint64 timeMs) const;
  qint64 regionEndMs(int index) const;   ///< Exclusive; max qint64 for the last region
  /// Takes whose audible range contains timeMs, newest first.
  QList<int> takesAt(qint64 timeMs) const;
  /// What is heard, in timeline order.
  QList<Segment> segments() const;

private:
  void assignRange(qint64 fromMs, qint64 toMs, int takeId);
  void mergeSilence();

  QList<Take> m_takes;           ///< Recording order: last = newest
  QList<CompRegion> m_regions;   ///< Never empty, first startMs == 0, strictly increasing
};

#endif // TAKETRACK_H
