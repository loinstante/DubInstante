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
  config.audioPath = audioPath;
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

// Export audio lists are renumbered (first recorded track = primary), so a missing take
// must be reported by file name, not by a track number the user would not recognise.
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
  config.audioPath = audioPath;
  config.extraAudioPaths = {QString(), missingPath};
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

}  // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  CHECK(checkMissingExtraAudioIsNamed() == 0);

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
