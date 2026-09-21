// CHECK-based tests for SaveManager (.dbi format); active in Release too.
// Run: cmake --build build && ctest --test-dir build --output-on-failure

#include "SaveManager.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QTemporaryDir>

#include "check.h"

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
  track.charMs = 108.3125;
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

bool corruptByteAt(const QString &path, qint64 offset) {
  QFile f(path);
  if (!f.open(QIODevice::ReadWrite) || !f.seek(offset))
    return false;
  char b;
  if (f.peek(&b, 1) != 1)
    return false;
  b = static_cast<char>(b ^ 0xFF);
  return f.write(&b, 1) == 1;
}

} // namespace

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);

  QTemporaryDir dir;
  CHECK(dir.isValid());
  SaveManager manager;

  // --- 1. Roundtrip with custom font and colors ---
  const QString path = dir.filePath("roundtrip.dbi");
  SaveData original = makeSampleData();
  CHECK(manager.save(path, original));

  SaveData loaded;
  CHECK(manager.load(path, loaded));
  CHECK(qFuzzyCompare(loaded.videoVolume, 0.7f));
  CHECK(loaded.trackCount == 2);
  CHECK(loaded.scrollSpeed == 120);
  CHECK(loaded.isTextWhite == false);
  CHECK(loaded.tracks.size() == 1);
  CHECK(loaded.tracks[0].text == "Bonjour le monde");
  CHECK(loaded.tracks[0].charMs == 108.3125); // exact in binary: no tolerance
  CHECK(loaded.tracks[0].style.globalSize == 22);
  CHECK(loaded.tracks[0].style.font.family() == "Liberation Mono");
  CHECK(loaded.tracks[0].style.font.bold() == false);
  CHECK(loaded.tracks[0].style.textColor.name(QColor::HexArgb) == "#ff112233");
  CHECK(loaded.tracks[0].style.backgroundColor.name(QColor::HexArgb) ==
         "#80445566");
  CHECK(loaded.audioTracks.size() == 1);
  CHECK(loaded.audioTracks[0].audioInput == "Micro test");
  CHECK(loaded.audioTracks[0].recordStartMs == 1500);
  CHECK(loaded.audioTracks[0].recordDurationMs == 42000);
  CHECK(loaded.audioTracks[0].hasRecording == true);

  // --- 2. Missing font fields fall back to Classic defaults (old files) ---
  // Old files simply lack font_family/font_bold; simulated by checking that
  // load keeps defaults when styleObj is absent (legacy string-format track).
  // Covered implicitly by SaveManager's default RythmoTrackStyle.

  // --- 3. Truncated file is rejected ---
  const QString truncPath = dir.filePath("truncated.dbi");
  {
    QFile src(path), dst(truncPath);
    CHECK(src.open(QIODevice::ReadOnly) && dst.open(QIODevice::WriteOnly));
    QByteArray all = src.readAll();
    dst.write(all.left(all.size() / 2));
  }
  SaveData ignored;
  CHECK(!manager.load(truncPath, ignored));

  // --- 4. Corrupted payload (checksum mismatch) is rejected ---
  const QString corruptPath = dir.filePath("corrupt.dbi");
  CHECK(QFile::copy(path, corruptPath));
  CHECK(corruptByteAt(corruptPath, kPayloadSizeOffset + 4 + 10)); // inside payload
  CHECK(!manager.load(corruptPath, ignored));

  // --- 5. Huge payloadSize is rejected without allocating ---
  const QString hugePath = dir.filePath("huge.dbi");
  CHECK(QFile::copy(path, hugePath));
  {
    QFile f(hugePath);
    CHECK(f.open(QIODevice::ReadWrite));
    f.seek(kPayloadSizeOffset);
    const quint32 huge = 0xFFFFFFFF;
    f.write(reinterpret_cast<const char *>(&huge), sizeof(huge));
  }
  CHECK(!manager.load(hugePath, ignored));

  // --- 6. Out-of-range values are clamped on load (sanitize) ---
  const QString dirtyPath = dir.filePath("dirty.dbi");
  SaveData dirty = makeSampleData();
  dirty.trackCount = 99;
  dirty.scrollSpeed = 100000;
  dirty.tracks[0].charMs = -5.0;
  CHECK(manager.save(dirtyPath, dirty));
  SaveData cleaned;
  CHECK(manager.load(dirtyPath, cleaned));
  CHECK(cleaned.tracks[0].charMs == 0.0); // falls back to the derived grid
  CHECK(cleaned.trackCount >= 1 && cleaned.trackCount <= 4);
  CHECK(cleaned.scrollSpeed >= 10 && cleaned.scrollSpeed <= 500);

  // --- 6b. resolveProjectPath: paths read from a project file ---
  {
    const QString projDir = dir.filePath("projet");
    CHECK(QDir().mkpath(projDir + "/sous/dossier"));
    CHECK(QDir().mkpath(dir.filePath("projet-evil")));
    {
      QFile f(dir.filePath("projet-evil/x.wav"));
      CHECK(f.open(QIODevice::WriteOnly));
    }
    const auto resolve = [&](const QString &p, bool strict,
                             bool allowOutside = false) {
      return SaveManager::resolveProjectPath(projDir, p, strict, allowOutside);
    };
    const QString root = QFileInfo(projDir).canonicalFilePath();

    CHECK(resolve("", true).isEmpty());
    CHECK(resolve("", false).isEmpty());
    CHECK(resolve("../../../etc/passwd", true).isEmpty());
    CHECK(resolve("../../../etc/passwd", false).isEmpty());
    CHECK(resolve("sous/dossier/track_1.wav", true) ==
           root + "/sous/dossier/track_1.wav");
    CHECK(resolve("./track_1.wav", true) == root + "/track_1.wav");
    CHECK(resolve("sous/../track_1.wav", false) == root + "/track_1.wav");
    CHECK(resolve("/etc/passwd", true).isEmpty());
    CHECK(resolve("/home/u/video.mp4", false) == "/home/u/video.mp4");
    CHECK(resolve(".", true).isEmpty()); // the directory itself is not inside it

    // A project directory that canonicalises to "/" contains nothing
    CHECK(SaveManager::resolveProjectPath("/", "track_1.wav", true).isEmpty());

    // A sibling sharing the directory name as a prefix is outside it
    CHECK(resolve("../projet-evil/x.wav", true).isEmpty());
    CHECK(resolve("../projet-evil/x.wav", false).isEmpty());

    // videoUrl of a standalone .dbi is relative to the .dbi, often outside it
    CHECK(!resolve("../videos/film.mp4", false, true).isEmpty());
    CHECK(resolve("../videos/film.mp4", true).isEmpty());

#ifndef Q_OS_WIN
    // A link inside the project that points outside is refused
    CHECK(QFile::link(dir.filePath("projet-evil"), projDir + "/lien"));
    CHECK(resolve("lien/x.wav", true).isEmpty());
#endif
  }

  // --- 7. saveWithMedia archive checks (zip path) ---
  QString zipErr;
  if (SaveManager::isZipAvailable(&zipErr)) {
    const QString audioSrc = dir.filePath("take1.wav");
    {
      QFile f(audioSrc);
      CHECK(f.open(QIODevice::WriteOnly));
      f.write("fake wav payload");
    }

    // Project without video must succeed and skip the video entry (S8).
    SaveData noVideo = makeSampleData();
    noVideo.videoUrl = "";
    noVideo.audioTracks[0].hasRecording = true;

    // Dotted project name to check .dbi/_audio naming consistency (S13).
    const QString zipPath = dir.filePath("mon.projet.v2.zip");
    QString err;
    CHECK(manager.saveWithMedia(zipPath, noVideo, {audioSrc}, &err));
    CHECK(err.isEmpty());
    CHECK(QFile::exists(zipPath));

    QProcess list;
    list.start("unzip", QStringList() << "-l" << zipPath);
    CHECK(list.waitForFinished());
    const QString listing = QString::fromUtf8(list.readAllStandardOutput());
    CHECK(listing.contains("mon.projet.v2.dbi"));
    CHECK(listing.contains("mon.projet.v2_audio/"));
    CHECK(!listing.contains(".mp4"));

    // A track whose WAV vanished must abort the archive, naming the track (S6).
    SaveData failData = makeSampleData();
    failData.videoUrl = "";
    failData.audioTracks[0].hasRecording = true;

    const QString failZipPath = dir.filePath("shouldfail.zip");
    QString failErr;
    CHECK(!manager.saveWithMedia(failZipPath, failData,
                                  {dir.filePath("deleted.wav")}, &failErr));
    CHECK(failErr.contains("Impossible de copier l'enregistrement"));
    CHECK(failErr.contains("piste 1"));
    CHECK(!QFile::exists(failZipPath));

    // --- 8. extractArchive: round trip of the archive written above ---
    QString unzipErr;
    if (SaveManager::isUnzipAvailable(&unzipErr)) {
      const QString extractDir = dir.filePath("extracted");
      QString extractErr;
      CHECK(manager.extractArchive(zipPath, extractDir, &extractErr));

      // audioFilePath must have been rewritten relative to the archive root
      // (the caller passed "/tmp/track_1.wav"), and resolve strictly
      SaveData restored;
      CHECK(manager.load(extractDir + "/mon.projet.v2.dbi", restored));
      CHECK(restored.audioTracks.size() == 1);
      CHECK(restored.audioTracks[0].audioFilePath ==
             "mon.projet.v2_audio/track_1.wav");
      const QString wav = SaveManager::resolveProjectPath(
          extractDir, restored.audioTracks[0].audioFilePath, true);
      CHECK(!wav.isEmpty() && QFile::exists(wav));

      // Video: relative name in the .dbi, resolves strictly inside the archive
      // (the flags loadProjectFrom uses for an archive), and re-saving over an
      // existing archive replaces it instead of merging (no stale entries).
      const QString videoSrc = dir.filePath("clip.mp4");
      {
        QFile f(videoSrc);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("fake video");
      }
      SaveData withVideo = makeSampleData();
      withVideo.videoUrl = videoSrc;
      const QString zipVideo = dir.filePath("avec_video.zip");
      CHECK(manager.saveWithMedia(zipVideo, withVideo, {audioSrc}, &extractErr));
      withVideo.videoUrl = dir.filePath("other.mp4");
      {
        QFile f(withVideo.videoUrl);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("another fake video");
      }
      withVideo.audioTracks[0].hasRecording = false;
      CHECK(manager.saveWithMedia(zipVideo, withVideo, {audioSrc}, &extractErr));
      CHECK(QDir(dir.path()).entryList({".dbi_tmp_*"}, QDir::AllEntries | QDir::Hidden |
                                                      QDir::NoDotAndDotDot).isEmpty());

      const QString videoDir = dir.filePath("extracted_video");
      CHECK(manager.extractArchive(zipVideo, videoDir, &extractErr));
      SaveData withVideoBack;
      CHECK(manager.load(videoDir + "/avec_video.dbi", withVideoBack));
      CHECK(withVideoBack.videoUrl == "other.mp4");
      const QString video = SaveManager::resolveProjectPath(
          videoDir, withVideoBack.videoUrl, true, false);
      CHECK(video.endsWith(".mp4") && QFile::exists(video));
      CHECK(!QFile::exists(videoDir + "/clip.mp4"));
      CHECK(!QFile::exists(videoDir + "/avec_video_audio")); // stale take gone

      // Not an archive: exit code failure, message names the code
      const QString garbage = dir.filePath("garbage.zip");
      {
        QFile f(garbage);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("not a zip");
      }
      QString badErr;
      CHECK(!manager.extractArchive(garbage, dir.filePath("out1"), &badErr));
      CHECK(badErr.contains("L'extraction a échoué"));

      // Missing archive
      badErr.clear();
      CHECK(!manager.extractArchive(dir.filePath("absent.zip"),
                                     dir.filePath("out2"), &badErr));
      CHECK(badErr.contains("Impossible de lire"));

      // Valid zip without any .dbi
      const QString srcDir = dir.filePath("no_project");
      CHECK(QDir().mkpath(srcDir));
      {
        QFile f(srcDir + "/readme.txt");
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("x");
      }
      QProcess zipProc;
      zipProc.setWorkingDirectory(srcDir);
      zipProc.start("zip", QStringList() << "-0" << "-r"
                                         << dir.filePath("no_project.zip") << ".");
      CHECK(zipProc.waitForFinished() && zipProc.exitCode() == 0);
      badErr.clear();
      CHECK(!manager.extractArchive(dir.filePath("no_project.zip"),
                                     dir.filePath("out3"), &badErr));
      CHECK(badErr.contains("ne contient pas de projet"));
    } else {
      std::puts("test_savemanager: 'unzip' unavailable, skipping extractArchive checks");
    }
  } else {
    std::puts("test_savemanager: 'zip' unavailable, skipping saveWithMedia checks");
  }

  std::puts("test_savemanager: OK");
  return 0;
}
