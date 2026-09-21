#include "SettingsManager.h"

SettingsManager& SettingsManager::instance() {
    static SettingsManager inst;
    return inst;
}

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
    migrateLegacyShortcuts();
}

void SettingsManager::migrateLegacyShortcuts() {
    QSettings settings;
    if (settings.value("shortcuts_migration_version", 0).toInt() >= 1) {
        return;
    }

    settings.beginGroup("shortcuts");

    // record_stop used to default to Ctrl+S, which now belongs to "project_save".
    // Only the old default is dropped, not a deliberate user rebind.
    if (QKeySequence(settings.value("record_stop").toString()) == QKeySequence("Ctrl+S")) {
        settings.remove("record_stop");
    }

    // A new default must not steal a key the user already gave to another
    // action: that action would silently stop working. Leave the new one unset.
    const QStringList storedActions = settings.childKeys();
    const QStringList changedDefaults = {"record_stop", "project_save", "project_save_as"};
    for (const QString &actionId : changedDefaults) {
        if (settings.contains(actionId)) {
            continue;
        }
        const QKeySequence newDefault = defaultShortcut(actionId);
        for (const QString &other : storedActions) {
            if (QKeySequence(settings.value(other).toString()) == newDefault) {
                settings.setValue(actionId, "none");
                break;
            }
        }
    }

    settings.endGroup();
    settings.setValue("shortcuts_migration_version", 1);
}

QString SettingsManager::theme() const {
    QSettings settings;
    return settings.value("theme", "system").toString();
}

void SettingsManager::setTheme(const QString &theme) {
    QSettings settings;
    settings.setValue("theme", theme);
}

bool SettingsManager::autoSaveEnabled() const {
    QSettings settings;
    return settings.value("autosave/enabled", true).toBool();
}

void SettingsManager::setAutoSaveEnabled(bool enabled) {
    QSettings settings;
    settings.setValue("autosave/enabled", enabled);
}

int SettingsManager::autoSaveInterval() const {
    QSettings settings;
    return settings.value("autosave/interval", 5).toInt();
}

void SettingsManager::setAutoSaveInterval(int minutes) {
    QSettings settings;
    settings.setValue("autosave/interval", minutes);
}

int SettingsManager::countdownDuration() const {
    QSettings settings;
    return settings.value("countdown", 3).toInt();
}

void SettingsManager::setCountdownDuration(int seconds) {
    QSettings settings;
    settings.setValue("countdown", seconds);
}

QString SettingsManager::defaultMicrophone() const {
    QSettings settings;
    return settings.value("audio/default_microphone", "").toString();
}

void SettingsManager::setDefaultMicrophone(const QString &micName) {
    QSettings settings;
    settings.setValue("audio/default_microphone", micName);
}

QString SettingsManager::trackMicrophone(int trackIdx) const {
    QSettings settings;
    return settings.value(QString("tracks/mic_%1").arg(trackIdx), "").toString();
}

void SettingsManager::setTrackMicrophone(int trackIdx, const QString &micName) {
    QSettings settings;
    settings.setValue(QString("tracks/mic_%1").arg(trackIdx), micName);
}

QStringList SettingsManager::preferredOutputs() const {
    QSettings settings;
    return settings.value("audio/preferred_outputs").toStringList();
}

void SettingsManager::setPreferredOutputs(const QStringList &outputs) {
    QSettings settings;
    settings.setValue("audio/preferred_outputs", outputs);
}

QString SettingsManager::activeOutputProfile() const {
    QSettings settings;
    return settings.value("audio/active_output_profile", "").toString();
}

void SettingsManager::setActiveOutputProfile(const QString &profileName) {
    QSettings settings;
    settings.setValue("audio/active_output_profile", profileName);
}

bool SettingsManager::expertMode() const {
    QSettings settings;
    return settings.value("expert_mode", false).toBool();
}

void SettingsManager::setExpertMode(bool enabled) {
    QSettings settings;
    settings.setValue("expert_mode", enabled);
}

QKeySequence SettingsManager::defaultShortcut(const QString &actionId) const {
    if (actionId == "video_play_pause") return QKeySequence(Qt::Key_Space);
    if (actionId == "video_frame_back") return QKeySequence(Qt::Key_Left);
    if (actionId == "video_frame_forward") return QKeySequence(Qt::Key_Right);
    if (actionId == "video_seek_back_5s") return QKeySequence(Qt::SHIFT | Qt::Key_Left);
    if (actionId == "video_seek_forward_5s") return QKeySequence(Qt::SHIFT | Qt::Key_Right);
    if (actionId == "record_start") return QKeySequence("Ctrl+R");
    if (actionId == "record_stop") return QKeySequence(Qt::Key_Escape);
    if (actionId == "project_save") return QKeySequence(QKeySequence::Save); // Ctrl+S
    if (actionId == "project_save_as") {
        // Qt only binds SaveAs on GNOME and macOS: empty on KDE and Windows
        const QKeySequence saveAs(QKeySequence::SaveAs);
        return saveAs.isEmpty() ? QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S) : saveAs;
    }
    if (actionId == "audio_volume_up") return QKeySequence(Qt::Key_Up);
    if (actionId == "audio_volume_down") return QKeySequence(Qt::Key_Down);
    if (actionId == "audio_volume_mute") return QKeySequence("M");
    return QKeySequence();
}

QKeySequence SettingsManager::shortcut(const QString &actionId) const {
    QSettings settings;
    if (!settings.contains("shortcuts/" + actionId)) {
        return defaultShortcut(actionId);
    }
    QString val = settings.value("shortcuts/" + actionId).toString();
    if (val == "none" || val.isEmpty()) {
        return QKeySequence();
    }
    return QKeySequence(val);
}

void SettingsManager::setShortcut(const QString &actionId, const QKeySequence &sequence) {
    QSettings settings;
    if (sequence.isEmpty()) {
        settings.setValue("shortcuts/" + actionId, "none");
    } else {
        settings.setValue("shortcuts/" + actionId, sequence.toString());
    }
}
