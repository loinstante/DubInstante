// CHECK-based test for ExportService::removePartialOutput.
// Regression: a failed export must never delete a file that already existed at
// the output path (e.g. a previous successful export) and that ffmpeg never touched.
// Build: cmake --build build --target test_exportservice && ./build/test_exportservice

#include "ExportService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

#include "check.h"
#include <cstdio>

namespace {

// Runs an export with PATH set to pathEnv and checks the pre-existing output survives.
int checkPreexistingOutputSurvives(const QByteArray &pathEnv) {
  QTemporaryDir dir;
  CHECK(dir.isValid());

  const QString videoPath = dir.filePath("source.mp4");
  const QString audioPath = dir.filePath("track1.wav");
  const QString outputPath = dir.filePath("output_double.mp4");

  QFile(videoPath).open(QIODevice::WriteOnly);
  QFile(audioPath).open(QIODevice::WriteOnly);

  {
    QFile f(outputPath);
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("previous export content");
  }
  const QDateTime mtimeBefore = QFileInfo(outputPath).lastModified();

  const QByteArray realPath = qgetenv("PATH");
  qputenv("PATH", pathEnv);

  ExportService service;
  ExportConfig config;
  config.videoPath = videoPath;
  config.segments = {{audioPath, 0, 0, 1000}};
  config.outputPath = outputPath;

  bool finishedSuccess = true;
  bool gotSignal = false;
  QObject::connect(&service, &ExportService::exportFinished,
                    [&](bool success, const QString &) {
                      finishedSuccess = success;
                      gotSignal = true;
                    });

  QEventLoop loop;
  QObject::connect(&service, &ExportService::exportFinished, &loop, &QEventLoop::quit);
  QTimer::singleShot(5000, &loop, &QEventLoop::quit);

  service.startExport(config);
  loop.exec();
  // Let the process-finished notification that follows exportFinished be handled too.
  QTimer::singleShot(200, &loop, &QEventLoop::quit);
  loop.exec();

  qputenv("PATH", realPath);

  CHECK(gotSignal);
  CHECK(!finishedSuccess);
  CHECK(!service.isExporting());
  CHECK(QFile::exists(outputPath));
  CHECK(QFileInfo(outputPath).lastModified() == mtimeBefore);
  {
    QFile f(outputPath);
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == "previous export content");
  }
  return 0;
}

// A missing take is reported by file name: segments of every track are mixed
// together, a track number would not tell which take is gone.
int checkMissingExtraAudioIsNamed() {
  QTemporaryDir dir;
  CHECK(dir.isValid());

  const QString videoPath = dir.filePath("source.mp4");
  const QString audioPath = dir.filePath("track2.wav");
  const QString missingPath = dir.filePath("track3_missing.wav");
  QFile(videoPath).open(QIODevice::WriteOnly);
  QFile(audioPath).open(QIODevice::WriteOnly);

  ExportService service;
  ExportConfig config;
  config.videoPath = videoPath;
  config.segments = {{audioPath, 0, 0, 1000}, {missingPath, 1000, 0, 1000}};
  config.outputPath = dir.filePath("output_double.mp4");

  bool finishedSuccess = true;
  QString message;
  QObject::connect(&service, &ExportService::exportFinished,
                   [&](bool success, const QString &msg) {
                     finishedSuccess = success;
                     message = msg;
                   });

  // Validation fails synchronously, before ffmpeg is started.
  service.startExport(config);

  CHECK(!finishedSuccess);
  CHECK(message.contains(missingPath));
  CHECK(!service.isExporting());
  return 0;
}

// Comp across two tracks: take A heard twice (split input), take B once,
// the range cuts the first and last pieces.
int checkAudioGraph() {
  ExportConfig config;
  config.originalVolume = 0.5f;
  config.rangeStartMs = 1000;
  config.durationMs = 3000;
  config.segments = {
      {"/a.wav", 500, 0, 1000, 1.0f},     // 0.5-1.5 s -> kept from 1 s, 500 ms
      {"/b.wav", 1500, 200, 1000, 0.8f},  // 1.5-2.5 s
      {"/a.wav", 2500, 2000, 2000, 1.0f}, // 2.5-4.5 s -> cut at 4 s
      {"/c.wav", 5000, 0, 1000, 1.0f},    // outside the range
      {"/d.wav", 1500, 0, 1000, 0.0f},    // muted track
  };

  QStringList inputs;
  const QString graph = ExportService::buildAudioGraph(config, &inputs);
  CHECK(inputs == QStringList({"/a.wav", "/b.wav"}));
  const QStringList chains = graph.split(";\n");
  CHECK(chains.size() == 6);
  CHECK(chains[0] == "[0:a]volume=0.5[orig]");
  CHECK(chains[1] == "[1:a]asplit=2[in1_0][in1_1]");
  CHECK(chains[2] ==
        "[in1_0]atrim=start=0.500:duration=0.500,asetpts=PTS-STARTPTS,volume=1[seg0]");
  CHECK(chains[3] == "[2:a]atrim=start=0.200:duration=1.000,asetpts=PTS-STARTPTS,"
                     "adelay=500:all=1,volume=0.8[seg1]");
  CHECK(chains[4] == "[in1_1]atrim=start=2.000:duration=1.500,asetpts=PTS-STARTPTS,"
                     "adelay=1500:all=1,volume=1[seg2]");
  CHECK(chains[5] ==
        "[orig][seg0][seg1][seg2]amix=inputs=4:duration=longest:normalize=0,apad[aout]");

  const QStringList args = ExportService::buildFFmpegArgs(config, "/tmp/g.txt", 6);
  CHECK(args.join(' ').contains("-ss 1.000 -i  -i /a.wav -i /b.wav"));
  CHECK(args.contains("-filter_complex_script"));
  CHECK(ExportService::buildFFmpegArgs(config, "/tmp/g.txt", 7).contains("-/filter_complex"));

  // Everything muted and no original audio: silence instead of an empty amix
  config.originalVolume = 0.0f;
  config.segments = {{"/a.wav", 1500, 0, 1000, 0.0f}};
  CHECK(ExportService::buildAudioGraph(config, &inputs) == "anullsrc=r=48000:cl=stereo[aout]");
  CHECK(inputs.isEmpty());
  return 0;
}

double probeDurationSeconds(const QString &path) {
  QProcess probe;
  probe.start(ExportService::toolPath("ffprobe"),
              {"-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path});
  if (!probe.waitForFinished(10000))
    return -1;
  return QString::fromUtf8(probe.readAllStandardOutput()).trimmed().toDouble();
}

bool runFFmpeg(const QStringList &args) {
  QProcess p;
  p.start(ExportService::toolPath("ffmpeg"), QStringList{"-v", "error", "-y"} + args);
  return p.waitForFinished(30000) && p.exitCode() == 0;
}

// Real ffmpeg: the generated graph must run, and the range sets the length.
int checkRealExport() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString video = dir.filePath("source.mp4");
  const QString takeA = dir.filePath("a.wav");
  const QString takeB = dir.filePath("b.wav");
  CHECK(runFFmpeg({"-f", "lavfi", "-i", "testsrc=size=160x120:rate=25:duration=4", "-f",
                   "lavfi", "-i", "sine=frequency=440:duration=4", "-shortest",
                   "-pix_fmt", "yuv420p", video}));
  CHECK(runFFmpeg({"-f", "lavfi", "-i", "sine=frequency=1000:duration=4", takeA}));
  CHECK(runFFmpeg({"-f", "lavfi", "-i", "sine=frequency=200:duration=2", takeB}));

  const auto exportRange = [&](qint64 start, qint64 duration, const QString &out) {
    ExportService service;
    ExportConfig config;
    config.videoPath = video;
    config.outputPath = out;
    config.speedPreset = "ultrafast";
    config.rangeStartMs = start;
    config.durationMs = duration;
    config.segments = {{takeA, 500, 0, 1000}, {takeB, 1500, 500, 1000},
                       {takeA, 2500, 2000, 1000}};
    bool ok = false;
    QString message;
    QEventLoop loop;
    QObject::connect(&service, &ExportService::exportFinished,
                     [&](bool success, const QString &msg) {
                       ok = success;
                       message = msg;
                       loop.quit();
                     });
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    service.startExport(config);
    loop.exec();
    if (!ok)
      std::fprintf(stderr, "export failed: %s\n", qPrintable(message));
    return ok;
  };

  const QString full = dir.filePath("full.mp4");
  CHECK(exportRange(0, -1, full));
  CHECK(qAbs(probeDurationSeconds(full) - 4.0) < 0.2);

  const QString excerpt = dir.filePath("excerpt.mp4");
  CHECK(exportRange(1000, 2000, excerpt));
  CHECK(qAbs(probeDurationSeconds(excerpt) - 2.0) < 0.2);
  return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  CHECK(checkMissingExtraAudioIsNamed() == 0);
  CHECK(checkAudioGraph() == 0);

  if (ExportService::isFFmpegAvailable()) {
    CHECK(checkRealExport() == 0);
  } else {
    std::puts("test_exportservice: ffmpeg unavailable, skipping the real export");
  }

  // ffmpeg missing from PATH: errorOccurred(FailedToStart) only.
  CHECK(checkPreexistingOutputSurvives("/nonexistent-path-for-test") == 0);

  // ffmpeg killed before opening its output: errorOccurred(Crashed) then finished,
  // so removePartialOutput runs twice for the same export.
  QTemporaryDir binDir;
  CHECK(binDir.isValid());
  {
    QFile script(binDir.filePath("ffmpeg"));
    CHECK(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nkill -9 $$\n");
    script.close();
    CHECK(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                 QFileDevice::ExeOwner));
  }
  CHECK(checkPreexistingOutputSurvives(
      QDir::toNativeSeparators(binDir.path()).toLocal8Bit()) == 0);

  std::puts("test_exportservice: OK");
  return 0;
}
