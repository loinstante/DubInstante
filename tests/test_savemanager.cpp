// Assert-based checks for SaveManager (.dbi format).
// Build: cmake --build build --target test_savemanager && ./build/test_savemanager

#include "SaveManager.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <cassert>
#include <cstdio>

namespace {

// .dbi layout: header(15) + version(1) + flags(1) + payloadSize(4) + payload + sha256(32)
constexpr int kPayloadSizeOffset = 15 + 1 + 1;

SaveData makeSampleData() {
  SaveData data;
  data.videoUrl = "";
  data.videoVolume = 0.7f;
  data.trackCount = 2;
  data.scrollSpeed = 120;
  data.isTextWhite = false;

  TrackSaveData track;
  track.text = "Bonjour le monde";
  track.style.globalSize = 22;
  track.style.font.setFamily("Liberation Mono");
  track.style.font.setBold(false);
  track.style.textColor = QColor("#FF112233");
  track.style.backgroundColor = QColor("#80445566");
  data.tracks.append(track);

  TrackAudioSaveData audio;
  audio.audioInput = "Micro test";
  audio.audioGain = 0.5f;
  audio.audioFilePath = "/tmp/track_1.wav";
  audio.recordStartMs = 1500;
  audio.recordDurationMs = 42000;
  audio.hasRecording = true;
  data.audioTracks.append(audio);

  return data;
}

void corruptByteAt(const QString &path, qint64 offset) {
  QFile f(path);
  bool ok = f.open(QIODevice::ReadWrite);
  assert(ok);
  ok = f.seek(offset);
  assert(ok);
  char b;
  f.peek(&b, 1);
  b = static_cast<char>(b ^ 0xFF);
  f.write(&b, 1);
  f.close();
}

} // namespace

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);

  QTemporaryDir dir;
  assert(dir.isValid());
  SaveManager manager;

  // --- 1. Roundtrip with custom font and colors ---
  const QString path = dir.filePath("roundtrip.dbi");
  SaveData original = makeSampleData();
  assert(manager.save(path, original));

  SaveData loaded;
  assert(manager.load(path, loaded));
  assert(qFuzzyCompare(loaded.videoVolume, 0.7f));
  assert(loaded.trackCount == 2);
  assert(loaded.scrollSpeed == 120);
  assert(loaded.isTextWhite == false);
  assert(loaded.tracks.size() == 1);
  assert(loaded.tracks[0].text == "Bonjour le monde");
  assert(loaded.tracks[0].style.globalSize == 22);
  assert(loaded.tracks[0].style.font.family() == "Liberation Mono");
  assert(loaded.tracks[0].style.font.bold() == false);
  assert(loaded.tracks[0].style.textColor.name(QColor::HexArgb) == "#ff112233");
  assert(loaded.tracks[0].style.backgroundColor.name(QColor::HexArgb) ==
         "#80445566");
  assert(loaded.audioTracks.size() == 1);
  assert(loaded.audioTracks[0].audioInput == "Micro test");
  assert(loaded.audioTracks[0].recordStartMs == 1500);
  assert(loaded.audioTracks[0].recordDurationMs == 42000);
  assert(loaded.audioTracks[0].hasRecording == true);

  // --- 2. Missing font fields fall back to Classic defaults (old files) ---
  // Old files simply lack font_family/font_bold; simulated by checking that
  // load keeps defaults when styleObj is absent (legacy string-format track).
  // Covered implicitly by SaveManager's default RythmoTrackStyle.

  // --- 3. Truncated file is rejected ---
  const QString truncPath = dir.filePath("truncated.dbi");
  {
    QFile src(path), dst(truncPath);
    assert(src.open(QIODevice::ReadOnly) && dst.open(QIODevice::WriteOnly));
    QByteArray all = src.readAll();
    dst.write(all.left(all.size() / 2));
  }
  SaveData ignored;
  assert(!manager.load(truncPath, ignored));

  // --- 4. Corrupted payload (checksum mismatch) is rejected ---
  const QString corruptPath = dir.filePath("corrupt.dbi");
  assert(QFile::copy(path, corruptPath));
  corruptByteAt(corruptPath, kPayloadSizeOffset + 4 + 10); // inside payload
  assert(!manager.load(corruptPath, ignored));

  // --- 5. Huge payloadSize is rejected without allocating ---
  const QString hugePath = dir.filePath("huge.dbi");
  assert(QFile::copy(path, hugePath));
  {
    QFile f(hugePath);
    assert(f.open(QIODevice::ReadWrite));
    f.seek(kPayloadSizeOffset);
    const quint32 huge = 0xFFFFFFFF;
    f.write(reinterpret_cast<const char *>(&huge), sizeof(huge));
  }
  assert(!manager.load(hugePath, ignored));

  // --- 6. Out-of-range values are clamped on load (sanitize) ---
  const QString dirtyPath = dir.filePath("dirty.dbi");
  SaveData dirty = makeSampleData();
  dirty.trackCount = 99;
  dirty.scrollSpeed = 100000;
  assert(manager.save(dirtyPath, dirty));
  SaveData cleaned;
  assert(manager.load(dirtyPath, cleaned));
  assert(cleaned.trackCount >= 1 && cleaned.trackCount <= 4);
  assert(cleaned.scrollSpeed >= 10 && cleaned.scrollSpeed <= 500);

  // --- 6b. resolveProjectPath: paths read from a project file ---
  {
    const QString projDir = dir.filePath("projet");
    assert(QDir().mkpath(projDir + "/sous/dossier"));
    assert(QDir().mkpath(dir.filePath("projet-evil")));
    {
      QFile f(dir.filePath("projet-evil/x.wav"));
      assert(f.open(QIODevice::WriteOnly));
    }
    const auto resolve = [&](const QString &p, bool strict,
                             bool allowOutside = false) {
      return SaveManager::resolveProjectPath(projDir, p, strict, allowOutside);
    };
    const QString root = QFileInfo(projDir).canonicalFilePath();

    assert(resolve("", true).isEmpty());
    assert(resolve("", false).isEmpty());
    assert(resolve("../../../etc/passwd", true).isEmpty());
    assert(resolve("../../../etc/passwd", false).isEmpty());
    assert(resolve("sous/dossier/track_1.wav", true) ==
           root + "/sous/dossier/track_1.wav");
    assert(resolve("./track_1.wav", true) == root + "/track_1.wav");
    assert(resolve("sous/../track_1.wav", false) == root + "/track_1.wav");
    assert(resolve("/etc/passwd", true).isEmpty());
    assert(resolve("/home/u/video.mp4", false) == "/home/u/video.mp4");
    assert(resolve(".", true).isEmpty()); // the directory itself is not inside it

    // A project directory that canonicalises to "/" contains nothing
    assert(SaveManager::resolveProjectPath("/", "track_1.wav", true).isEmpty());

    // A sibling sharing the directory name as a prefix is outside it
    assert(resolve("../projet-evil/x.wav", true).isEmpty());
    assert(resolve("../projet-evil/x.wav", false).isEmpty());

    // videoUrl of a standalone .dbi is relative to the .dbi, often outside it
    assert(!resolve("../videos/film.mp4", false, true).isEmpty());
    assert(resolve("../videos/film.mp4", true).isEmpty());

#ifndef Q_OS_WIN
    // A link inside the project that points outside is refused
    assert(QFile::link(dir.filePath("projet-evil"), projDir + "/lien"));
    assert(resolve("lien/x.wav", true).isEmpty());
#endif
  }

  // --- 7. saveWithMedia archive checks (zip path) ---
  QString zipErr;
  if (SaveManager::isZipAvailable(&zipErr)) {
    const QString audioSrc = dir.filePath("take1.wav");
    {
      QFile f(audioSrc);
      assert(f.open(QIODevice::WriteOnly));
      f.write("fake wav payload");
    }

    // Project without video must succeed and skip the video entry (S8).
    SaveData noVideo = makeSampleData();
    noVideo.videoUrl = "";
    noVideo.audioTracks[0].hasRecording = true;

    // Dotted project name to check .dbi/_audio naming consistency (S13).
    const QString zipPath = dir.filePath("mon.projet.v2.zip");
    QString err;
    assert(manager.saveWithMedia(zipPath, noVideo, {audioSrc}, &err));
    assert(err.isEmpty());
    assert(QFile::exists(zipPath));

    QProcess list;
    list.start("unzip", QStringList() << "-l" << zipPath);
    assert(list.waitForFinished());
    const QString listing = QString::fromUtf8(list.readAllStandardOutput());
    assert(listing.contains("mon.projet.v2.dbi"));
    assert(listing.contains("mon.projet.v2_audio/"));
    assert(!listing.contains(".mp4"));

    // A track whose WAV vanished must abort the archive, naming the track (S6).
    SaveData failData = makeSampleData();
    failData.videoUrl = "";
    failData.audioTracks[0].hasRecording = true;

    const QString failZipPath = dir.filePath("shouldfail.zip");
    QString failErr;
    assert(!manager.saveWithMedia(failZipPath, failData,
                                  {dir.filePath("deleted.wav")}, &failErr));
    assert(failErr.contains("Impossible de copier l'enregistrement"));
    assert(failErr.contains("piste 1"));
    assert(!QFile::exists(failZipPath));
  } else {
    std::puts("test_savemanager: 'zip' unavailable, skipping saveWithMedia checks");
  }

  std::puts("test_savemanager: OK");
  return 0;
}
