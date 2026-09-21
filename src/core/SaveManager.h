#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "RythmoManager.h"

/**
 * @struct TrackSaveData
 * @brief Saves track content and style.
 */
struct TrackSaveData {
  QString text;
  RythmoTrackStyle style;
};

/**
 * @struct TrackAudioSaveData
 * @brief Saves audio input and gain for a track.
 */
struct TrackAudioSaveData {
  QString audioInput;
  float audioGain = 1.0f;
  QString audioFilePath;
  qint64 recordStartMs = 0;
  qint64 recordDurationMs = 0;
  bool hasRecording = false;
};

/**
 * @struct SaveData
 * @brief Plain data structure for session state.
 */
struct SaveData {
  QString videoUrl;
  float videoVolume = 1.0f;
  int trackCount = 1;
  int scrollSpeed = 100;
  bool isTextWhite = false;

  QList<TrackSaveData> tracks;
  QList<TrackAudioSaveData> audioTracks;
};

/**
 * @class SaveManager
 * @brief Handles serialization, obfuscation, and file I/O for .dbi files.
 */
class SaveManager : public QObject {
  Q_OBJECT

public:
  explicit SaveManager(QObject *parent = nullptr);

  /**
   * @brief Saves the session data to a .dbi file.
   * @param filePath Target file path.
   * @param data Data to save.
   * @return True if successful.
   */
  bool save(const QString &filePath, const SaveData &data);
  bool saveWithMedia(const QString &zipPath, const SaveData &data, 
                     const QStringList &tempAudioPaths, QString *errorMessage = nullptr);

  /**
   * @brief Checks if the 'zip' utility is available (Unix only).
   * @param errorMessage Optional pointer to store install instructions.
   * @return True if zip is available (always true on Windows).
   */
  static bool isZipAvailable(QString *errorMessage = nullptr);

  /**
   * @brief Checks if an archive extraction tool is available: 'unzip', or
   *        'tar' on Windows (native since Windows 10). The first one found wins.
   * @param errorMessage Optional pointer to store install instructions.
   */
  static bool isUnzipAvailable(QString *errorMessage = nullptr);

  /**
   * @brief Extracts an archive written by saveWithMedia() into @p destDir.
   *
   * Requires free space of the archive size + 20 % on the destination volume,
   * and at least one .dbi at the root of @p destDir afterwards. The caller
   * validates the paths stored in that .dbi (resolveProjectPath, strict).
   * Blocking: run it off the GUI thread for large archives.
   */
  bool extractArchive(const QString &zipPath, const QString &destDir,
                      QString *errorMessage = nullptr);

  /**
   * @brief Loads session data from a .dbi file.
   * @param filePath Source file path.
   * @param data Reference to store loaded data.
   * @return True if successful and integrity check passed.
   */
  bool load(const QString &filePath, SaveData &data);

  /**
   * @brief Clamps values to their valid ranges.
   */
  static SaveData sanitize(const SaveData &data);

  /**
   * @brief Resolves a path read from a project file, relative to the project
   *        directory. Returns an empty string if the path is refused.
   *
   * A relative path is resolved (symlinks followed) and must end up strictly
   * inside @p projectDir, compared component by component.
   *
   * @param strictRelative True for a project extracted from an archive: only
   *        relative paths are accepted. False for a standalone .dbi, which is
   *        a project the user created on their own machine: an absolute path
   *        is accepted as-is. This is the one deliberate relaxation of the
   *        check; it also covers autosave files, which store absolute temp paths.
   * @param allowOutside Skips the containment check on relative paths. Only for
   *        videoUrl of a standalone .dbi: save() stores it relative to the .dbi
   *        (e.g. "../Videos/film.mp4"), so existing projects point outside.
   */
  static QString resolveProjectPath(const QString &projectDir,
                                    const QString &storedPath,
                                    bool strictRelative,
                                    bool allowOutside = false);

private:
  QByteArray applyXorMask(const QByteArray &data);
  QByteArray calculateChecksum(const QByteArray &data);

  const QByteArray m_header = "DubInstanteFile";
  const quint8 m_version = 1;
  const quint8 m_xorKey = 0x5A; // Simple static key for obfuscation
};

#endif // SAVEMANAGER_H
