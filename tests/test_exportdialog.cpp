// CHECK-based test for ExportDialog's async ffprobe lifetime.
// Regression: closing the dialog after ffprobe exited but before its finished() was
// delivered crashed: ~QProcess delivered it while the dialog's widgets were already gone.
// Build: cmake --build build --target test_exportdialog && ./build/test_exportdialog

#include "ExportDialog.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include "check.h"
#include <cstdio>

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString videoPath = dir.filePath("source.mp4");
  QFile(videoPath).open(QIODevice::WriteOnly);

  // Instant fake ffprobe: exits normally with a video stream and no audio stream,
  // so its handler takes both widget-updating branches.
  {
    QFile script(dir.filePath("ffprobe"));
    CHECK(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\necho video,640,480\n");
    script.close();
    CHECK(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                 QFileDevice::ExeOwner));
  }
  qputenv("PATH", QDir::toNativeSeparators(dir.path()).toLocal8Bit());

  for (int i = 0; i < 3; ++i) {
    auto *dialog = new ExportDialog(videoPath, dir.filePath("track1.wav"), {}, 0, 0, {0},
                                    1.0f, {1.0f}, {false}, {1});
    // No event loop: the probe exits and its finished() stays undelivered.
    QThread::msleep(300);
    delete dialog;
  }

  std::puts("test_exportdialog: OK");
  return 0;
}
