#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QKeySequence>

/**
 * @class SettingsManager
 * @brief Singleton class to handle persistent application settings using QSettings.
 */
class SettingsManager : public QObject {
    Q_OBJECT

public:
    static SettingsManager& instance();

    // Theme: "light", "dark", "system"
    QString theme() const;
    void setTheme(const QString &theme);

    // Auto-save config
    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);
    
    int autoSaveInterval() const; // in minutes
    void setAutoSaveInterval(int minutes);

    // Countdown before recording (in seconds: 0, 3, 5)
    int countdownDuration() const;
    void setCountdownDuration(int seconds);

    // Default global microphone
    QString defaultMicrophone() const;
    void setDefaultMicrophone(const QString &micName);

    // Track-specific persistent microphone mapping
    QString trackMicrophone(int trackIdx) const;
    void setTrackMicrophone(int trackIdx, const QString &micName);

    // Preferred output profiles: format "Label|DeviceName"
    QStringList preferredOutputs() const;
    void setPreferredOutputs(const QStringList &outputs);

    // Active output device name
    QString activeOutputProfile() const;
    void setActiveOutputProfile(const QString &profileName);

    // Expert mode persistence
    bool expertMode() const;
    void setExpertMode(bool enabled);

    // Shortcuts management
    QKeySequence shortcut(const QString &actionId) const;
    void setShortcut(const QString &actionId, const QKeySequence &sequence);
    QKeySequence defaultShortcut(const QString &actionId) const;

private:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override = default;

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    // One-time migration of shortcuts whose default value changed between versions
    void migrateLegacyShortcuts();
};

#endif // SETTINGSMANAGER_H
