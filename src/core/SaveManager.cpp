#include "SaveManager.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtGlobal>

SaveManager::SaveManager(QObject *parent) : QObject(parent) {}

bool SaveManager::save(const QString &filePath, const SaveData &data) {
  SaveData cleanData = sanitize(data);

  // Convert video path to relative if it's a local file
  QString videoPath = cleanData.videoUrl;
  if (!videoPath.isEmpty() && QFileInfo(videoPath).isAbsolute()) {
    QDir saveDir = QFileInfo(filePath).dir();
    cleanData.videoUrl = saveDir.relativeFilePath(videoPath);
  }

  QJsonObject root;
  root["video_url"] = cleanData.videoUrl;
  root["video_volume"] = cleanData.videoVolume;
  root["audio_input_1"] = cleanData.audioInput1;
  root["audio_gain_1"] = cleanData.audioGain1;
  root["audio_input_2"] = cleanData.audioInput2;
  root["audio_gain_2"] = cleanData.audioGain2;
  root["enable_track_2"] = cleanData.enableTrack2;
  root["scroll_speed"] = cleanData.scrollSpeed;
  root["is_text_white"] = cleanData.isTextWhite;
  root["tracks"] = QJsonArray::fromStringList(cleanData.tracks);

  QJsonDocument doc(root);
  QByteArray jsonPayload = doc.toJson(QJsonDocument::Compact);
  QByteArray maskedPayload = applyXorMask(jsonPayload);
  QByteArray checksum = calculateChecksum(jsonPayload);

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Failed to open file for writing:" << filePath;
    return false;
  }

  // Header
  file.write(m_header);

  // Version & Flags
  file.putChar(static_cast<char>(m_version));
  file.putChar(0); // Flags

  // Payload Size & Data (little-endian for cross-platform portability)
  quint32 payloadSize =
      qToLittleEndian(static_cast<quint32>(maskedPayload.size()));
  file.write(reinterpret_cast<const char *>(&payloadSize), sizeof(payloadSize));
  file.write(maskedPayload);

  // Checksum
  file.write(checksum);

  file.close();
  return true;
}

bool SaveManager::isZipAvailable(QString *errorMessage) {
#ifdef Q_OS_WIN
  Q_UNUSED(errorMessage);
  return true; // PowerShell is always available on Windows
#else
  QProcess checkZip;
  checkZip.start("zip", QStringList() << "-h");
  if (!checkZip.waitForStarted()) {
    if (errorMessage) {
#ifdef Q_OS_MAC
      *errorMessage = QObject::tr(
          "L'utilitaire 'zip' est introuvable.\n\n"
          "Veuillez l'installer pour utiliser cette fonctionnalité.\n"
          "Lien : https://formulae.brew.sh/formula/zip");
#else
      *errorMessage =
          QObject::tr("L'utilitaire 'zip' est introuvable.\n\n"
                      "Veuillez l'installer via votre terminal :\n"
                      "Debian/Ubuntu : sudo apt install zip\n"
                      "Fedora : sudo dnf install zip\n"
                      "Arch : sudo pacman -S zip\n\n"
                      "Ou consultez : https://command-not-found.com/zip");
#endif
    }
    return false;
  }
  checkZip.waitForFinished();
  return true;
#endif
}

bool SaveManager::saveWithMedia(const QString &zipPath, const SaveData &data,
                                QString *errorMessage) {

  // 1. Create temporary directory
  QTemporaryDir tempDir;
  if (!tempDir.isValid()) {
    qWarning() << "Failed to create temporary directory";
    if (errorMessage)
      *errorMessage = QObject::tr("Impossible de créer le dossier temporaire.");
    return false;
  }

  // 2. Save .dbi file
  QString dbiName = QFileInfo(zipPath).completeBaseName() + ".dbi";
  QString dbiPath = tempDir.filePath(dbiName);

  // We need a modified data object where video path is relative to the ZIP root
  SaveData zipData = data;
  QFileInfo videoInfo(data.videoUrl);
  QString videoFileName = videoInfo.fileName();
  zipData.videoUrl = videoFileName; // Point to local file inside ZIP

  if (!save(dbiPath, zipData)) {
    if (errorMessage)
      *errorMessage = QObject::tr("Échec de la sauvegarde du fichier .dbi");
    return false;
  }

  // 3. Copy video file
  QString videoSource = data.videoUrl;
  if (videoSource.startsWith("file://")) {
    videoSource = QUrl(videoSource).toLocalFile();
  }

  QString videoDest = tempDir.filePath(videoFileName);
  if (!QFile::copy(videoSource, videoDest)) {
    qWarning() << "Failed to copy video file to temp dir:" << videoSource;
    if (errorMessage)
      *errorMessage =
          QObject::tr("Impossible de copier la vidéo dans l'archive.");
    return false;
  }

  // 4. Create ZIP archive
  QProcess zipProcess;
  zipProcess.setWorkingDirectory(tempDir.path());
  QStringList args;

#ifdef Q_OS_WIN
  // On Windows, use PowerShell's Compress-Archive
  QString sourcePath = QDir::toNativeSeparators(tempDir.path() + "/*");
  QString destPath = QDir::toNativeSeparators(zipPath);

  // Remove existing file first
  if (QFile::exists(zipPath)) {
    QFile::remove(zipPath);
  }

  // Escape single quotes for PowerShell ('' is the escape sequence inside '')
  QString escapedSource = sourcePath;
  escapedSource.replace(QLatin1String("'"), QLatin1String("''"));
  QString escapedDest = destPath;
  escapedDest.replace(QLatin1String("'"), QLatin1String("''"));

  zipProcess.setProgram("powershell");
  args << "-Command"
       << QString("Compress-Archive -Path '%1' -DestinationPath '%2' -Force")
              .arg(escapedSource, escapedDest);
#else
  // On macOS and Linux, 'zip' is standard
  zipProcess.setProgram("zip");
  args << "-r" << zipPath << ".";
#endif

  zipProcess.setArguments(args);
  zipProcess.start();

  // Wait indefinitely for the process to finish
  if (!zipProcess.waitForFinished(-1)) {
    qWarning() << "Zip process failed to finish";
    if (errorMessage)
      *errorMessage = QObject::tr(
          "Le processus de compression a échoué (timeout ou erreur interne).");
    return false;
  }

  if (zipProcess.exitCode() != 0) {
    qWarning() << "Zip process failed with code:" << zipProcess.exitCode();
    qDebug() << zipProcess.readAllStandardError();
    if (errorMessage)
      *errorMessage = QObject::tr("Erreur lors de la compression (Code: %1)")
                          .arg(zipProcess.exitCode());
    return false;
  }

  return true;
}

bool SaveManager::load(const QString &filePath, SaveData &data) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  // Header check
  QByteArray header(m_header.size(), 0);
  if (file.read(header.data(), m_header.size()) != m_header.size())
    return false;

  if (header != m_header) {
    qWarning() << "Invalid file header";
    return false;
  }

  // Version & Flags
  char version, flags;
  if (!file.getChar(&version) || !file.getChar(&flags))
    return false;

  if (static_cast<quint8>(version) > m_version) {
    qWarning() << "Unsupported version:" << static_cast<quint8>(version);
    return false;
  }

  // Payload Size (stored as little-endian)
  quint32 payloadSizeLE;
  if (file.read(reinterpret_cast<char *>(&payloadSizeLE),
                sizeof(payloadSizeLE)) != sizeof(payloadSizeLE))
    return false;
  quint32 payloadSize = qFromLittleEndian(payloadSizeLE);

  // Payload
  QByteArray maskedPayload = file.read(payloadSize);
  if (maskedPayload.size() != static_cast<int>(payloadSize))
    return false;

  // Checksum
  QByteArray storedChecksum = file.read(32); // SHA-256 is 32 bytes
  if (storedChecksum.size() != 32)
    return false;

  QByteArray jsonPayload = applyXorMask(maskedPayload);
  QByteArray calculatedChecksum = calculateChecksum(jsonPayload);

  if (calculatedChecksum != storedChecksum) {
    qWarning() << "Integrity check failed (checksum mismatch)";
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(jsonPayload);
  if (doc.isNull() || !doc.isObject()) {
    qWarning() << "Invalid JSON content";
    return false;
  }

  QJsonObject root = doc.object();

  // Robust loading with fallbacks
  data.videoUrl = root.value("video_url").toString("");
  data.videoVolume = (float)root.value("video_volume").toDouble(1.0);
  data.audioInput1 = root.value("audio_input_1").toString("");
  data.audioGain1 = (float)root.value("audio_gain_1").toDouble(1.0);
  data.audioInput2 = root.value("audio_input_2").toString("");
  data.audioGain2 = (float)root.value("audio_gain_2").toDouble(1.0);
  data.enableTrack2 = root.value("enable_track_2").toBool(false);
  data.scrollSpeed = root.value("scroll_speed").toInt(100);
  data.isTextWhite = root.value("is_text_white").toBool(true);

  QJsonArray tracksArray = root.value("tracks").toArray();
  data.tracks.clear();
  for (const auto &val : tracksArray) {
    data.tracks.append(val.toString());
  }

  // Resolve relative path
  if (!data.videoUrl.isEmpty()) {
    QFileInfo videoInfo(data.videoUrl);
    if (videoInfo.isRelative()) {
      QDir saveDir = QFileInfo(filePath).dir();
      data.videoUrl = saveDir.absoluteFilePath(data.videoUrl);
    }
  }

  file.close();
  return true;
}

SaveData SaveManager::sanitize(const SaveData &data) {
  SaveData clean = data;
  clean.videoVolume = qBound(0.0f, clean.videoVolume, 1.0f);
  clean.audioGain1 = qBound(0.0f, clean.audioGain1, 1.0f);
  clean.audioGain2 = qBound(0.0f, clean.audioGain2, 1.0f);
  clean.scrollSpeed = qBound(10, clean.scrollSpeed, 500);

  // We do NOT trim tracks, as whitespace is timing!
  // for (int i = 0; i < clean.tracks.size(); ++i) {
  //   clean.tracks[i] = clean.tracks[i].trimmed();
  // }

  return clean;
}

QByteArray SaveManager::applyXorMask(const QByteArray &data) {
  QByteArray result = data;
  for (int i = 0; i < result.size(); ++i) {
    result[i] = result[i] ^ m_xorKey;
  }
  return result;
}

QByteArray SaveManager::calculateChecksum(const QByteArray &data) {
  return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}
