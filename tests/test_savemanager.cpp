// CHECK-based tests for SaveManager (.dbi format); active in Release too.
// Run: cmake --build build && ctest --test-dir build --output-on-failure

#include "SaveManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
  // Take 1 over 1.5-43.5 s, take 2 punched in at 10 s after 2 s of pre-roll,
  // then a user cut at 20 s inside take 1
  audio.takes.addRecording(Take{1, "/tmp/track_1_take_1.wav", 1500, 0, 42000}, 1500, 43500);
  audio.takes.addRecording(Take{2, "/tmp/track_1_take_2.wav", 8000, 2000, 6000}, 10000, 14000);
  audio.takes.split(20000);
  data.audioTracks.append(audio);
  data.nextTakeId = 3;

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

bool sameComp(const TakeTrack &a, const TakeTrack &b) {
  if (a.regions().size() != b.regions().size() || a.takes().size() != b.takes().size())
    return false;
  for (int i = 0; i < a.regions().size(); ++i) {
    if (a.regions()[i].startMs != b.regions()[i].startMs ||
        a.regions()[i].takeId != b.regions()[i].takeId)
      return false;
  }
  for (int i = 0; i < a.takes().size(); ++i) {
    const Take &x = a.takes()[i];
    const Take &y = b.takes()[i];
    if (x.id != y.id || x.file != y.file || x.startMs != y.startMs || x.inMs != y.inMs ||
        x.durationMs != y.durationMs)
      return false;
  }
  return true;
}

// Writes a .dbi by hand, as an older (or newer) build would.
bool writeRawDbi(const QString &path, const QJsonObject &root, quint8 version) {
  const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
  QByteArray masked = json;
  for (char &c : masked)
    c = static_cast<char>(c ^ 0x5A);
  const quint32 size = static_cast<quint32>(masked.size());
  QByteArray out("DubInstanteFile");
  out.append(static_cast<char>(version));
  out.append('\0');
  for (int i = 0; i < 4; ++i)
    out.append(static_cast<char>((size >> (8 * i)) & 0xFF));
  out.append(masked);
  out.append(QCryptographicHash::hash(json, QCryptographicHash::Sha256));
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(out) == out.size();
}

// A 0.12 project (version 1, one take per track) opens as one take heard
// over its whole length; a take without stored duration stays open-ended.
int checkLegacyProject(SaveManager &manager, const QString &dir) {
  const QJsonObject legacy{
      {"track_count", 2},
      {"audio_tracks",
       QJsonArray{QJsonObject{{"audioFilePath", "p_audio/track_1.wav"},
                              {"recordStartMs", 1500},
                              {"recordDurationMs", 42000},
                              {"hasRecording", true}},
                  QJsonObject{{"audioFilePath", "p_audio/track_2.wav"},
                              {"recordStartMs", 3000},
                              {"hasRecording", true}},
                  QJsonObject{{"hasRecording", false}}}}};
  const QString path = dir + "/legacy.dbi";
  CHECK(writeRawDbi(path, legacy, 1));

  SaveData data;
  CHECK(manager.load(path, data));
  CHECK(data.audioTracks.size() == 3);
  const TakeTrack &first = data.audioTracks[0].takes;
  CHECK(first.takes().size() == 1);
  CHECK(first.takes()[0].file == "p_audio/track_1.wav");
  const QList<Segment> segs = first.segments();
  CHECK(segs.size() == 1);
  CHECK(segs[0].timelineStartMs == 1500 && segs[0].durationMs == 42000);

  // Take ids stay unique across tracks
  const TakeTrack &second = data.audioTracks[1].takes;
  CHECK(second.takes().size() == 1);
  CHECK(second.takes()[0].id != first.takes()[0].id);
  CHECK(second.takes()[0].durationMs == kOpenEndedTakeMs);
  CHECK(data.audioTracks[2].takes.isEmpty());

  // A file from a newer build is refused rather than half-read
  CHECK(writeRawDbi(dir + "/future.dbi", legacy, 3));
  SaveData future;
  CHECK(!manager.load(dir + "/future.dbi", future));
  return 0;
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
  CHECK(sameComp(loaded.audioTracks[0].takes, original.audioTracks[0].takes));
  CHECK(loaded.nextTakeId == 3);

  // --- 2. Files written by 0.12 and older ---
  CHECK(checkLegacyProject(manager, dir.path()) == 0);

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

    // The caller rewrites take files relative to the project and lists the copies
    const auto withMedia = [&](SaveData data, const QString &base,
                               QList<ProjectMedia> *media) {
      TakeTrack &takes = data.audioTracks[0].takes;
      for (const Take &take : QList<Take>(takes.takes())) {
        const QString rel = QString("%1_audio/track_1_take_%2.wav").arg(base).arg(take.id);
        takes.setTakeMedia(take.id, rel, 0);
        media->append(ProjectMedia{audioSrc, rel});
      }
      return data;
    };

    // Project without video must succeed and skip the video entry (S8).
    // Dotted project name to check .dbi/_audio naming consistency (S13).
    QList<ProjectMedia> media;
    SaveData noVideo = withMedia(makeSampleData(), "mon.projet.v2", &media);
    noVideo.videoUrl = "";

    const QString zipPath = dir.filePath("mon.projet.v2.zip");
    QString err;
    CHECK(manager.saveWithMedia(zipPath, noVideo, media, &err));
    CHECK(err.isEmpty());
    CHECK(QFile::exists(zipPath));

    QProcess list;
    list.start("unzip", QStringList() << "-l" << zipPath);
    CHECK(list.waitForFinished());
    const QString listing = QString::fromUtf8(list.readAllStandardOutput());
    CHECK(listing.contains("mon.projet.v2.dbi"));
    CHECK(listing.contains("mon.projet.v2_audio/track_1_take_1.wav"));
    CHECK(listing.contains("mon.projet.v2_audio/track_1_take_2.wav"));
    CHECK(!listing.contains(".mp4"));

    // A take whose WAV vanished must abort the archive, naming the take (S6).
    const QString failZipPath = dir.filePath("shouldfail.zip");
    QString failErr;
    CHECK(!manager.saveWithMedia(
        failZipPath, noVideo,
        {ProjectMedia{dir.filePath("deleted.wav"), "shouldfail_audio/track_1_take_1.wav"}},
        &failErr));
    CHECK(failErr.contains("Impossible de copier l'enregistrement"));
    CHECK(failErr.contains("track_1_take_1.wav"));
    CHECK(!QFile::exists(failZipPath));

    // --- 8. extractArchive: round trip of the archive written above ---
    QString unzipErr;
    if (SaveManager::isUnzipAvailable(&unzipErr)) {
      const QString extractDir = dir.filePath("extracted");
      QString extractErr;
      CHECK(manager.extractArchive(zipPath, extractDir, &extractErr));

      // Take files are relative to the archive root and resolve strictly
      SaveData restored;
      CHECK(manager.load(extractDir + "/mon.projet.v2.dbi", restored));
      CHECK(restored.audioTracks.size() == 1);
      CHECK(sameComp(restored.audioTracks[0].takes, noVideo.audioTracks[0].takes));
      for (const Take &take : restored.audioTracks[0].takes.takes()) {
        const QString wav = SaveManager::resolveProjectPath(extractDir, take.file, true);
        CHECK(!wav.isEmpty() && QFile::exists(wav));
      }

      // Video: relative name in the .dbi, resolves strictly inside the archive
      // (the flags loadProjectFrom uses for an archive), and re-saving over an
      // existing archive replaces it instead of merging (no stale entries).
      const QString videoSrc = dir.filePath("clip.mp4");
      {
        QFile f(videoSrc);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("fake video");
      }
      QList<ProjectMedia> videoMedia;
      SaveData withVideo = withMedia(makeSampleData(), "avec_video", &videoMedia);
      withVideo.videoUrl = videoSrc;
      const QString zipVideo = dir.filePath("avec_video.zip");
      CHECK(manager.saveWithMedia(zipVideo, withVideo, videoMedia, &extractErr));
      withVideo.videoUrl = dir.filePath("other.mp4");
      {
        QFile f(withVideo.videoUrl);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("another fake video");
      }
      withVideo.audioTracks[0].takes = TakeTrack();
      CHECK(manager.saveWithMedia(zipVideo, withVideo, {}, &extractErr));
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
