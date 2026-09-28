#include "TakeTrack.h"

#include <QSet>
#include <algorithm>
#include <limits>

TakeTrack::TakeTrack() { m_regions.append(CompRegion{0, 0}); }

TakeTrack TakeTrack::fromParts(const QList<Take> &takes,
                               const QList<CompRegion> &regions) {
  TakeTrack track;
  QSet<int> ids;
  for (const Take &t : takes) {
    if (t.id <= 0 || ids.contains(t.id) || t.durationMs <= 0 || t.inMs < 0 ||
        t.inMs >= t.durationMs)
      continue;
    ids.insert(t.id);
    track.m_takes.append(t);
  }

  QList<CompRegion> sorted = regions;
  std::sort(sorted.begin(), sorted.end(),
            [](const CompRegion &a, const CompRegion &b) {
              return a.startMs < b.startMs;
            });
  track.m_regions.clear();
  track.m_regions.append(CompRegion{0, 0});
  for (CompRegion r : sorted) {
    r.startMs = qMax<qint64>(0, r.startMs);
    if (!ids.contains(r.takeId))
      r.takeId = 0;
    if (r.startMs == track.m_regions.last().startMs)
      track.m_regions.last().takeId = r.takeId;
    else
      track.m_regions.append(r);
  }
  track.mergeSilence();
  return track;
}

const Take *TakeTrack::take(int id) const {
  for (const Take &t : m_takes)
    if (t.id == id)
      return &t;
  return nullptr;
}

int TakeTrack::maxTakeId() const {
  int maxId = 0;
  for (const Take &t : m_takes)
    maxId = qMax(maxId, t.id);
  return maxId;
}

void TakeTrack::addRecording(const Take &take, qint64 punchInMs,
                             qint64 punchOutMs) {
  m_takes.append(take);
  const qint64 from = qMax(punchInMs, take.audibleStartMs());
  const qint64 to = qMin(punchOutMs, take.audibleEndMs());
  if (from < to)
    assignRange(from, to, take.id);
  mergeSilence();
}

void TakeTrack::split(qint64 timeMs) {
  if (timeMs <= 0)
    return;
  const int i = regionIndexAt(timeMs);
  if (m_regions[i].startMs == timeMs)
    return;
  m_regions.insert(i + 1, CompRegion{timeMs, m_regions[i].takeId});
}

void TakeTrack::removeCut(qint64 timeMs) {
  const int i = regionIndexAt(timeMs);
  if (i > 0 && m_regions[i].startMs == timeMs)
    m_regions.removeAt(i);
}

void TakeTrack::choose(qint64 timeMs, int takeId) {
  if (takeId != 0 && !take(takeId))
    return;
  m_regions[regionIndexAt(timeMs)].takeId = takeId;
}

void TakeTrack::removeTake(int takeId) {
  const auto it = std::find_if(m_takes.begin(), m_takes.end(),
                               [takeId](const Take &t) { return t.id == takeId; });
  if (it == m_takes.end())
    return;
  m_takes.erase(it);

  for (int i = 0; i < m_regions.size(); ++i) {
    if (m_regions[i].takeId != takeId)
      continue;
    const qint64 start = m_regions[i].startMs;
    const qint64 end = regionEndMs(i);
    int fallback = 0;
    for (int k = m_takes.size() - 1; k >= 0; --k) {
      if (m_takes[k].audibleStartMs() < end && m_takes[k].audibleEndMs() > start) {
        fallback = m_takes[k].id;
        break;
      }
    }
    m_regions[i].takeId = fallback;
  }
  mergeSilence();
}

void TakeTrack::setTakeFile(int takeId, const QString &file) {
  for (Take &t : m_takes)
    if (t.id == takeId)
      t.file = file;
}

int TakeTrack::regionIndexAt(qint64 timeMs) const {
  const auto it = std::upper_bound(
      m_regions.begin(), m_regions.end(), timeMs,
      [](qint64 t, const CompRegion &r) { return t < r.startMs; });
  return qMax(0, int(it - m_regions.begin()) - 1);
}

qint64 TakeTrack::regionEndMs(int index) const {
  return index + 1 < m_regions.size() ? m_regions[index + 1].startMs
                                      : std::numeric_limits<qint64>::max();
}

QList<int> TakeTrack::takesAt(qint64 timeMs) const {
  QList<int> ids;
  for (int k = m_takes.size() - 1; k >= 0; --k)
    if (timeMs >= m_takes[k].audibleStartMs() && timeMs < m_takes[k].audibleEndMs())
      ids.append(m_takes[k].id);
  return ids;
}

QList<Segment> TakeTrack::segments() const {
  QList<Segment> out;
  for (int i = 0; i < m_regions.size(); ++i) {
    const Take *t = take(m_regions[i].takeId);
    if (!t)
      continue;
    const qint64 from = qMax(m_regions[i].startMs, t->audibleStartMs());
    const qint64 to = qMin(regionEndMs(i), t->audibleEndMs());
    if (from >= to)
      continue;
    out.append(Segment{t->id, t->file, from, from - t->startMs, to - from});
  }
  return out;
}

void TakeTrack::assignRange(qint64 fromMs, qint64 toMs, int takeId) {
  split(fromMs);
  split(toMs);
  for (CompRegion &r : m_regions)
    if (r.startMs >= fromMs && r.startMs < toMs)
      r.takeId = takeId;
}

// User cuts between two takes are kept; only cuts inside silence are noise.
void TakeTrack::mergeSilence() {
  for (int i = m_regions.size() - 1; i > 0; --i)
    if (m_regions[i].takeId == 0 && m_regions[i - 1].takeId == 0)
      m_regions.removeAt(i);
}
