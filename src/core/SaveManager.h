#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

/**
 * @struct SaveData
 * @brief Plain data structure for session state.
 */
struct SaveData {
  QString videoUrl;
  float videoVolume;
  QString audioInput1;
  float audioGain1;
  QString audioInput2;
  float audioGain2;
  bool enableTrack2;
  int scrollSpeed;
  bool isTextWhite;
  QStringList tracks;
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
                     QString *errorMessage = nullptr);

  /**
   * @brief Checks if the 'zip' utility is available (Unix only).
   * @param errorMessage Optional pointer to store install instructions.
   * @return True if zip is available (always true on Windows).
   */
  static bool isZipAvailable(QString *errorMessage = nullptr);

  /**
   * @brief Loads session data from a .dbi file.
   * @param filePath Source file path.
   * @param data Reference to store loaded data.
   * @return True if successful and integrity check passed.
   */
  bool load(const QString &filePath, SaveData &data);

  /**
   * @brief Normalizes paths and clamps values.
   */
  static SaveData sanitize(const SaveData &data);

private:
  QByteArray applyXorMask(const QByteArray &data);
  QByteArray calculateChecksum(const QByteArray &data);

  const QByteArray m_header = "DubInstanteFile";
  const quint8 m_version = 1;
  const quint8 m_xorKey = 0x5A; // Simple static key for obfuscation
};

#endif // SAVEMANAGER_H
