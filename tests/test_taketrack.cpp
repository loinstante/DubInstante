// Comp model: which take is heard where.

#include "../src/core/TakeTrack.h"
#include "check.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <cstdio>

static Take makeTake(int id, qint64 start, qint64 duration, qint64 in = 0) {
  return Take{id, QString("take_%1.wav").arg(id), start, in, duration};
}

static bool isSeg(const Segment &s, int id, qint64 start, qint64 offset, qint64 dur) {
  return s.takeId == id && s.timelineStartMs == start && s.sourceOffsetMs == offset &&
         s.durationMs == dur;
}

// A punch-in inside an older take: new take heard on its range, old one around it.
static int checkPunchInMiddle() {
  TakeTrack track;
  track.addRecording(makeTake(1, 5000, 15000), 5000, 20000);
  track.addRecording(makeTake(2, 10000, 5000), 10000, 15000);

  const QList<Segment> segs = track.segments();
  CHECK(segs.size() == 3);
  CHECK(isSeg(segs[0], 1, 5000, 0, 5000));
  CHECK(isSeg(segs[1], 2, 10000, 0, 5000));
  CHECK(isSeg(segs[2], 1, 15000, 10000, 5000));
  CHECK(track.takesAt(12000) == QList<int>({2, 1}));
  CHECK(track.takesAt(4999).isEmpty());
  return 0;
}

// Pre-roll audio sits in the file but is never heard.
static int checkPreRoll() {
  TakeTrack track;
  track.addRecording(makeTake(1, 0, 20000), 0, 20000);
  // Played from 8 s, punch-in at 10 s: the first 2 s of take 2 are pre-roll.
  track.addRecording(makeTake(2, 8000, 7000, 2000), 10000, 15000);

  const QList<Segment> segs = track.segments();
  CHECK(segs.size() == 3);
  CHECK(isSeg(segs[0], 1, 0, 0, 10000));
  CHECK(isSeg(segs[1], 2, 10000, 2000, 5000));
  CHECK(isSeg(segs[2], 1, 15000, 15000, 5000));

  // Picking take 2 on a region it only partly covers: never its pre-roll.
  track.choose(0, 2);
  CHECK(isSeg(track.segments()[0], 2, 10000, 2000, 5000));
  return 0;
}

// Comping: split, then pick an older take on one side.
static int checkSplitAndChoose() {
  TakeTrack track;
  track.addRecording(makeTake(1, 5000, 15000), 5000, 20000);
  track.addRecording(makeTake(2, 10000, 5000), 10000, 15000);
  track.split(12000);
  track.choose(11000, 1);

  const QList<Segment> segs = track.segments();
  CHECK(segs.size() == 4);
  CHECK(isSeg(segs[1], 1, 10000, 5000, 2000));
  CHECK(isSeg(segs[2], 2, 12000, 2000, 3000));

  // Splitting on an existing cut or at 0 changes nothing.
  const int regions = track.regions().size();
  track.split(12000);
  track.split(0);
  CHECK(track.regions().size() == regions);

  track.removeCut(12000);
  CHECK(track.regions().size() == regions - 1);
  CHECK(track.segments()[1].takeId == 1);

  // Choosing an unknown take is refused.
  track.choose(11000, 99);
  CHECK(track.segments()[1].takeId == 1);
  return 0;
}

// Deleting the heard take falls back to the newest take still covering it.
static int checkRemoveTake() {
  TakeTrack track;
  track.addRecording(makeTake(1, 0, 30000), 0, 30000);
  track.addRecording(makeTake(2, 5000, 20000), 5000, 25000);
  track.addRecording(makeTake(3, 10000, 5000), 10000, 15000);

  track.removeTake(3);
  CHECK(track.take(3) == nullptr);
  const QList<Segment> segs = track.segments();
  CHECK(segs.size() == 5);
  CHECK(segs[1].takeId == 2 && segs[2].takeId == 2 && segs[3].takeId == 2);

  track.removeTake(2);
  track.removeTake(1);
  CHECK(track.segments().isEmpty());
  CHECK(track.regions().size() == 1);   // cuts inside silence are merged away
  return 0;
}

// A comp region keeps its cut even when the chosen take ends earlier.
static int checkShorterTakeLeavesSilence() {
  TakeTrack track;
  track.addRecording(makeTake(1, 0, 10000), 0, 10000);
  track.addRecording(makeTake(2, 0, 4000), 0, 4000);
  track.choose(5000, 2);
  const QList<Segment> segs = track.segments();
  CHECK(segs.size() == 1);
  CHECK(isSeg(segs[0], 2, 0, 0, 4000));
  return 0;
}

// Data read from a file is repaired rather than trusted.
static int checkFromParts() {
  const QList<Take> takes = {makeTake(1, 0, 5000), makeTake(1, 0, 9000),
                             makeTake(2, 0, -1), makeTake(3, 1000, 2000, 5000)};
  const QList<CompRegion> regions = {{3000, 7}, {-50, 1}, {1000, 0}, {2000, 0}};
  const TakeTrack track = TakeTrack::fromParts(takes, regions);

  CHECK(track.takes().size() == 1);          // duplicate, negative, in >= duration dropped
  CHECK(track.take(1)->durationMs == 5000);
  CHECK(track.regions().size() == 2);        // unknown take 7 -> silence, merged
  CHECK(track.regions()[0].startMs == 0 && track.regions()[0].takeId == 1);
  CHECK(track.regions()[1].startMs == 1000 && track.regions()[1].takeId == 0);
  CHECK(track.maxTakeId() == 1);
  return 0;
}

// 16-bit stereo 48 kHz: 192000 bytes per second
static bool writeWav(const QString &path, quint32 declaredDataSize, int actualDataSize,
                     bool extraChunk) {
  const auto le32 = [](quint32 v) {
    QByteArray b(4, 0);
    qToLittleEndian(v, b.data());
    return b;
  };
  QByteArray fmt = QByteArray::fromHex("0100020080bb0000") + le32(192000) +
                   QByteArray::fromHex("04001000");
  QByteArray body = "WAVEfmt " + le32(fmt.size()) + fmt;
  if (extraChunk)
    body += "LIST" + le32(3) + QByteArray(4, 'x');   // odd size: padded
  body += "data" + le32(declaredDataSize) + QByteArray(actualDataSize, 0);
  QFile f(path);
  return f.open(QIODevice::WriteOnly) &&
         f.write("RIFF" + le32(body.size()) + body) > 0;
}

static int checkWavDuration() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString wav = dir.filePath("a.wav");
  CHECK(writeWav(wav, 96000, 96000, true));
  CHECK(wavDurationMs(wav) == 500);
  // Writer killed mid-take: header never updated, the file length decides
  CHECK(writeWav(wav, 0, 192000, false));
  CHECK(wavDurationMs(wav) == 1000);
  QFile junk(dir.filePath("b.wav"));
  CHECK(junk.open(QIODevice::WriteOnly) && junk.write("not a wav") > 0);
  junk.close();
  CHECK(wavDurationMs(junk.fileName()) == -1);
  CHECK(wavDurationMs(dir.filePath("missing.wav")) == -1);
  return 0;
}

int main() {
  CHECK(checkPunchInMiddle() == 0);
  CHECK(checkPreRoll() == 0);
  CHECK(checkSplitAndChoose() == 0);
  CHECK(checkRemoveTake() == 0);
  CHECK(checkShorterTakeLeavesSilence() == 0);
  CHECK(checkFromParts() == 0);
  CHECK(checkWavDuration() == 0);
  std::printf("test_taketrack: OK\n");
  return 0;
}
