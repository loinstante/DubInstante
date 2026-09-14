// Assert-based check for ExportService::removePartialOutput.
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
#include <cassert>
#include <cstdio>

namespace {

// Runs an export with PATH set to pathEnv and checks the pre-existing output survives.
void checkPreexistingOutputSurvives(const QByteArray &pathEnv) {
  QTemporaryDir dir;
  assert(dir.isValid());

  const QString videoPath = dir.filePath("source.mp4");
  const QString audioPath = dir.filePath("track1.wav");
  const QString outputPath = dir.filePath("output_double.mp4");

  QFile(videoPath).open(QIODevice::WriteOnly);
  QFile(audioPath).open(QIODevice::WriteOnly);

  {
    QFile f(outputPath);
    assert(f.open(QIODevice::WriteOnly));
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

  assert(gotSignal);
  assert(!finishedSuccess);
  assert(!service.isExporting());
  assert(QFile::exists(outputPath));
  assert(QFileInfo(outputPath).lastModified() == mtimeBefore);
  {
    QFile f(outputPath);
    assert(f.open(QIODevice::ReadOnly));
    assert(f.readAll() == "previous export content");
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  // ffmpeg missing from PATH: errorOccurred(FailedToStart) only.
  checkPreexistingOutputSurvives("/nonexistent-path-for-test");

  // ffmpeg killed before opening its output: errorOccurred(Crashed) then finished,
  // so removePartialOutput runs twice for the same export.
  QTemporaryDir binDir;
  assert(binDir.isValid());
  {
    QFile script(binDir.filePath("ffmpeg"));
    assert(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nkill -9 $$\n");
    script.close();
    assert(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                 QFileDevice::ExeOwner));
  }
  checkPreexistingOutputSurvives(QDir::toNativeSeparators(binDir.path()).toLocal8Bit());

  std::puts("test_exportservice: OK");
  return 0;
}
