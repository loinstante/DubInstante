// CHECK-based tests for SettingsManager shortcut defaults + one-time shortcut migration.
// Build: cmake --build build --target test_settingsmanager && ./build/test_settingsmanager
// Headless: QT_QPA_PLATFORM=offscreen ./build/test_settingsmanager

#include "SettingsManager.h"

#include <QGuiApplication>
#include <QKeySequence>
#include <QSettings>
#include <QTemporaryDir>

#include "check.h"
#include <cstdio>

int main(int argc, char *argv[]) {
  // QKeySequence::Save/SaveAs resolve through the platform theme, which
  // needs a QGuiApplication (a plain QCoreApplication segfaults here).
  QGuiApplication app(argc, argv);

  QTemporaryDir tempDir;
  CHECK(tempDir.isValid());
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QCoreApplication::setOrganizationName("DubInstanteTest");
  QCoreApplication::setApplicationName("SettingsManagerTest");

  // Existing profile, written before SettingsManager::instance() runs the
  // migration: record_stop still on the old Ctrl+S default, and the user gave
  // Ctrl+Shift+S (the new "Save as" default) to mute.
  {
    QSettings raw;
    raw.setValue("shortcuts/record_stop", QKeySequence("Ctrl+S").toString());
    raw.setValue("shortcuts/audio_volume_mute", QKeySequence("Ctrl+Shift+S").toString());
  }

  SettingsManager &sm = SettingsManager::instance();

  // Literal sequences, not the StandardKey enums: SaveAs is empty on offscreen,
  // KDE and Windows, which an enum-to-enum comparison would not catch.
  CHECK(sm.defaultShortcut("record_stop") == QKeySequence(Qt::Key_Escape));
  CHECK(sm.defaultShortcut("project_save") == QKeySequence("Ctrl+S"));
  CHECK(sm.defaultShortcut("project_save_as") == QKeySequence("Ctrl+Shift+S"));

  QSettings raw;

  // The stale Ctrl+S override is gone: record_stop falls back to Escape.
  CHECK(!raw.contains("shortcuts/record_stop"));
  CHECK(sm.shortcut("record_stop") == QKeySequence(Qt::Key_Escape));

  // Ctrl+S was freed before the collision check, so project_save keeps it.
  CHECK(!raw.contains("shortcuts/project_save"));
  CHECK(sm.shortcut("project_save") == QKeySequence("Ctrl+S"));

  // Ctrl+Shift+S stays with mute; "Save as" was explicitly left unassigned.
  CHECK(sm.shortcut("audio_volume_mute") == QKeySequence("Ctrl+Shift+S"));
  CHECK(raw.value("shortcuts/project_save_as").toString() == "none");
  CHECK(sm.shortcut("project_save_as").isEmpty());

  CHECK(raw.value("shortcuts_migration_version").toInt() == 1);

  printf("test_settingsmanager: OK\n");
  return 0;
}
