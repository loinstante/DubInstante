#include "SaveManager.h"
#include "Constants.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStorageInfo>
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
  root["track_count"] = cleanData.trackCount;
  root["scroll_speed"] = cleanData.scrollSpeed;
  root["is_text_white"] = cleanData.isTextWhite;

  // Save audio tracks
  QJsonArray audioTracksArray;
  for (const auto &audioTrack : cleanData.audioTracks) {
    QJsonObject audioObj;
    audioObj["audio_input"] = audioTrack.audioInput;
    audioObj["audio_gain"] = audioTrack.audioGain;
    audioObj["audioFilePath"] = audioTrack.audioFilePath;
    audioObj["recordStartMs"] = audioTrack.recordStartMs;
    audioObj["recordDurationMs"] = audioTrack.recordDurationMs;
    audioObj["hasRecording"] = audioTrack.hasRecording;
    audioTracksArray.append(audioObj);
  }
  root["audio_tracks"] = audioTracksArray;

  QJsonArray tracksArray;
  for (const auto &trackData : cleanData.tracks) {
    QJsonObject trackObj;
    trackObj["text"] = trackData.text;

    // Save style parameters
    QJsonObject styleObj;
    styleObj["font_size"] = trackData.style.globalSize;
    styleObj["font_family"] = trackData.style.font.family();
    styleObj["font_bold"] = trackData.style.font.bold();
    styleObj["text_color"] = trackData.style.textColor.name(QColor::HexArgb);
    styleObj["bg_color"] =
        trackData.style.backgroundColor.name(QColor::HexArgb);

    trackObj["style"] = styleObj;
    tracksArray.append(trackObj);
  }
  root["tracks"] = tracksArray;

  QJsonDocument doc(root);
  QByteArray jsonPayload = doc.toJson(QJsonDocument::Compact);
  QByteArray maskedPayload = applyXorMask(jsonPayload);
  QByteArray checksum = calculateChecksum(jsonPayload);

  // Atomic write: the previous save survives a disk-full or crash mid-write
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Failed to open file for writing:" << filePath;
    return false;
  }

  quint32 payloadSize =
      qToLittleEndian(static_cast<quint32>(maskedPayload.size()));

  bool ok = file.write(m_header) == m_header.size();
  ok = ok && file.putChar(static_cast<char>(m_version));
  ok = ok && file.putChar(0); // Flags
  ok = ok && file.write(reinterpret_cast<const char *>(&payloadSize),
                        sizeof(payloadSize)) == sizeof(payloadSize);
  ok = ok && file.write(maskedPayload) == maskedPayload.size();
  ok = ok && file.write(checksum) == checksum.size();

  if (!ok) {
    qWarning() << "Failed to write save data:" << filePath;
    file.cancelWriting();
    return false;
  }
  return file.commit();
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

namespace {

// "tar" first on Windows (native since Windows 10, reads zip), "unzip" elsewhere.
// Empty when none can be started.
QString findExtractTool() {
#ifdef Q_OS_WIN
  const QStringList candidates{"tar", "unzip"};
#else
  const QStringList candidates{"unzip"};
#endif
  for (const QString &tool : candidates) {
    QProcess probe;
    probe.start(tool, tool == "tar" ? QStringList{"--version"}
                                    : QStringList{"-v"});
    if (probe.waitForStarted()) {
      probe.waitForFinished(5000);
      return tool;
    }
  }
  return QString();
}

} // namespace

bool SaveManager::isUnzipAvailable(QString *errorMessage) {
  if (!findExtractTool().isEmpty())
    return true;

  if (errorMessage) {
#if defined(Q_OS_WIN)
    *errorMessage = QObject::tr(
        "Aucun utilitaire d'extraction n'est disponible.\n\n"
        "'tar' est fourni avec Windows 10 (version 1803) et suivants.\n"
        "Mettez Windows à jour ou installez 'unzip'.");
#elif defined(Q_OS_MAC)
    *errorMessage = QObject::tr(
        "L'utilitaire 'unzip' est introuvable.\n\n"
        "Veuillez l'installer pour utiliser cette fonctionnalité.\n"
        "Lien : https://formulae.brew.sh/formula/unzip");
#else
    *errorMessage =
        QObject::tr("L'utilitaire 'unzip' est introuvable.\n\n"
                    "Veuillez l'installer via votre terminal :\n"
                    "Debian/Ubuntu : sudo apt install unzip\n"
                    "Fedora : sudo dnf install unzip\n"
                    "Arch : sudo pacman -S unzip\n\n"
                    "Ou consultez : https://command-not-found.com/unzip");
#endif
  }
  return false;
}

bool SaveManager::extractArchive(const QString &zipPath, const QString &destDir,
                                 QString *errorMessage) {
  const auto fail = [errorMessage](const QString &message) {
    qWarning() << "extractArchive:" << message;
    if (errorMessage)
      *errorMessage = message;
    return false;
  };

  const QFileInfo zipInfo(zipPath);
  if (!zipInfo.isFile() || !zipInfo.isReadable())
    return fail(QObject::tr("Impossible de lire l'archive :\n%1")
                    .arg(QDir::toNativeSeparators(zipPath)));

  if (!QDir().mkpath(destDir))
    return fail(QObject::tr("Impossible de créer le dossier d'extraction."));

  // Written with zip -0: the extracted size is close to the archive size
  const qint64 needed = zipInfo.size() + zipInfo.size() / 5;
  const QStorageInfo storage(destDir);
  if (storage.isValid() && storage.isReady() &&
      storage.bytesAvailable() < needed)
    return fail(QObject::tr("Espace disque insuffisant pour extraire "
                            "l'archive.\nIl faut environ %1 Mo libres.")
                    .arg(needed / (1024 * 1024) + 1));

  const QString tool = findExtractTool();
  if (tool.isEmpty()) {
    QString help; // message d'installation par plateforme
    isUnzipAvailable(&help);
    return fail(help);
  }

  // Absolute paths: an archive named "-x.zip" must not be parsed as an option
  const QString zipAbs = zipInfo.absoluteFilePath();
  const QString destAbs = QDir(destDir).absolutePath();
  const QStringList args =
      tool == "tar" ? QStringList{"-xf", zipAbs, "-C", destAbs}
                    : QStringList{"-o", zipAbs, "-d", destAbs};

  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(tool, args);
  if (!process.waitForStarted())
    return fail(QObject::tr("Impossible de lancer '%1'.").arg(tool));

  // Videos can weigh several GB, and the archive may sit on a network share
  constexpr int kExtractTimeoutMs = 2 * 60 * 60 * 1000;
  if (!process.waitForFinished(kExtractTimeoutMs)) {
    process.kill();
    process.waitForFinished();
    return fail(QObject::tr(
        "L'extraction a échoué (délai dépassé ou erreur interne)."));
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    const QString output =
        QString::fromLocal8Bit(process.readAll()).trimmed().left(400);
    return fail(QObject::tr("L'extraction a échoué (code %1).\n"
                            "L'archive est peut-être corrompue ou incomplète.\n\n%2")
                    .arg(process.exitCode())
                    .arg(output));
  }

  if (QDir(destAbs).entryList({"*.dbi"}, QDir::Files).isEmpty())
    return fail(QObject::tr("Cette archive ne contient pas de projet DubInstante."));

  return true;
}

bool SaveManager::saveWithMedia(const QString &zipPath, const SaveData &data,
                                const QStringList &tempAudioPaths, QString *errorMessage) {

  QString videoSource = data.videoUrl;
  if (videoSource.startsWith("file://")) {
    videoSource = QUrl(videoSource).toLocalFile();
  }

  // 0. Pre-check space on the destination volume: temp copy + stored zip ~= 2x video
  const qint64 videoSize = QFileInfo(videoSource).size();
  QStorageInfo storage(QFileInfo(zipPath).absolutePath());
  if (storage.isValid() && storage.isReady() &&
      storage.bytesAvailable() < 2 * videoSize + 64LL * 1024 * 1024) {
    qWarning() << "Not enough space on destination volume for zip archive";
    if (errorMessage)
      *errorMessage = QObject::tr(
          "Espace disque insuffisant sur le volume de destination.\n"
          "L'archive nécessite environ %1 Mo libres.")
          .arg((2 * videoSize + 64LL * 1024 * 1024) / (1024 * 1024));
    return false;
  }

  // 1. Create temporary directory on the destination volume:
  // the system temp is often a RAM-backed tmpfs too small for large videos
  QTemporaryDir tempDir(QFileInfo(zipPath).absolutePath() + "/.dbi_tmp_XXXXXX");
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

  // Same for the takes: the caller's paths are temp files of this machine
  const QString audioDirName = QFileInfo(zipPath).completeBaseName() + "_audio";
  for (int i = 0; i < zipData.audioTracks.size(); ++i) {
    if (zipData.audioTracks[i].hasRecording && i < tempAudioPaths.size())
      zipData.audioTracks[i].audioFilePath =
          QString("%1/track_%2.wav").arg(audioDirName).arg(i + 1);
  }

  if (!save(dbiPath, zipData)) {
    if (errorMessage)
      *errorMessage = QObject::tr("Échec de la sauvegarde du fichier .dbi");
    return false;
  }

  // 3. Copy video file (projects without video legitimately have no source)
  if (!videoSource.isEmpty()) {
    QString videoDest = tempDir.filePath(videoFileName);
    if (!QFile::copy(videoSource, videoDest)) {
      qWarning() << "Failed to copy video file to temp dir:" << videoSource;
      if (errorMessage)
        *errorMessage =
            QObject::tr("Impossible de copier la vidéo dans l'archive.");
      return false;
    }
  }

  // 3.5 Copy audio tracks
  QDir tempQDir(tempDir.path());
  bool hasAnyAudio = false;
  
  for (int i = 0; i < data.audioTracks.size(); ++i) {
      if (data.audioTracks[i].hasRecording && i < tempAudioPaths.size()) {
          hasAnyAudio = true;
          break;
      }
  }
  
  if (hasAnyAudio) {
      tempQDir.mkdir(audioDirName);
      QDir tempAudioDir(tempQDir.absoluteFilePath(audioDirName));
      
      for (int i = 0; i < data.audioTracks.size(); ++i) {
          if (data.audioTracks[i].hasRecording && i < tempAudioPaths.size()) {
              QString sourcePath = tempAudioPaths[i];
              QString destFilename = QString("track_%1.wav").arg(i + 1);
              if (!QFile::exists(sourcePath) ||
                  !QFile::copy(sourcePath, tempAudioDir.absoluteFilePath(destFilename))) {
                  qWarning() << "Failed to copy audio track to temp dir:" << sourcePath;
                  if (errorMessage)
                      *errorMessage = QObject::tr("Impossible de copier l'enregistrement de la piste %1 dans l'archive.").arg(i + 1);
                  return false;
              }
          }
      }
  }

  // 4. Create ZIP archive
  QProcess zipProcess;
  zipProcess.setWorkingDirectory(tempDir.path());
  QStringList args;
  QString zipTarget = zipPath;
  const auto discardPartial = [&]() {
    if (zipTarget != zipPath)
      QFile::remove(zipTarget);
  };

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
  // On macOS and Linux, 'zip' is standard.
  // -0 stores without recompressing: the video payload is already compressed
  // zip updates an existing archive instead of replacing it (stale video, stale
  // takes): build next to the destination, then swap once it is complete.
  zipTarget = tempDir.path() + ".zip";
  zipProcess.setProgram("zip");
  args << "-0" << "-r" << zipTarget << ".";
#endif

  zipProcess.setArguments(args);
  zipProcess.start();

  // Wait indefinitely for the process to finish
  if (!zipProcess.waitForFinished(-1)) {
    qWarning() << "Zip process failed to finish";
    discardPartial();
    if (errorMessage)
      *errorMessage = QObject::tr(
          "Le processus de compression a échoué (timeout ou erreur interne).");
    return false;
  }

  if (zipProcess.exitCode() != 0) {
    qWarning() << "Zip process failed with code:" << zipProcess.exitCode();
    qDebug() << zipProcess.readAllStandardError();
    discardPartial();
    if (errorMessage)
      *errorMessage = QObject::tr("Erreur lors de la compression (Code: %1)")
                          .arg(zipProcess.exitCode());
    return false;
  }

  if (zipTarget != zipPath) {
    QFile::remove(zipPath);
    if (!QFile::rename(zipTarget, zipPath)) {
      discardPartial();
      if (errorMessage)
        *errorMessage = QObject::tr("Impossible d'écrire l'archive :\n%1")
                            .arg(QDir::toNativeSeparators(zipPath));
      return false;
    }
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

  constexpr quint32 kMaxPayloadSize = 64 * 1024 * 1024;
  if (payloadSize == 0 || payloadSize > kMaxPayloadSize ||
      static_cast<qint64>(payloadSize) > file.size()) {
    qWarning() << "Invalid payload size:" << payloadSize;
    return false;
  }

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
  data.trackCount = root.value("track_count").toInt(1);
  data.scrollSpeed = root.value("scroll_speed").toInt(100);
  data.isTextWhite = root.value("is_text_white").toBool(false);

  // Load audio tracks
  data.audioTracks.clear();
  QJsonArray audioTracksArray = root.value("audio_tracks").toArray();
  for (const auto &val : audioTracksArray) {
    TrackAudioSaveData audioData;
    QJsonObject audioObj = val.toObject();
    audioData.audioInput = audioObj.value("audio_input").toString("");
    audioData.audioGain = (float)audioObj.value("audio_gain").toDouble(1.0);
    audioData.audioFilePath = audioObj["audioFilePath"].toString();
    audioData.recordStartMs = audioObj["recordStartMs"].toInteger();
    audioData.recordDurationMs = audioObj["recordDurationMs"].toInteger();
    audioData.hasRecording = audioObj["hasRecording"].toBool();
    data.audioTracks.append(audioData);
  }

  // Load rythmo tracks
  QJsonArray tracksArray = root.value("tracks").toArray();
  data.tracks.clear();
  for (const auto &val : tracksArray) {
    TrackSaveData trackData;
    if (val.isString()) {
      // Backwards Compatibility (v0.8.0 or older)
      trackData.text = val.toString();
      // Style remains default (Classic)
    } else if (val.isObject()) {
      // New format (v0.9.0+)
      QJsonObject trackObj = val.toObject();
      trackData.text = trackObj.value("text").toString("");

      QJsonObject styleObj = trackObj.value("style").toObject();
      if (!styleObj.isEmpty()) {
        trackData.style.globalSize = styleObj.value("font_size").toInt(16);
        QString fontFamily = styleObj.value("font_family").toString();
        if (!fontFamily.isEmpty()) {
          trackData.style.font.setFamily(fontFamily);
        }
        trackData.style.font.setBold(styleObj.value("font_bold").toBool(true));
        trackData.style.font.setPointSize(trackData.style.globalSize);
        trackData.style.textColor =
            QColor(styleObj.value("text_color").toString("#FFFFFFFF"));
        trackData.style.backgroundColor =
            QColor(styleObj.value("bg_color").toString("#FF282828"));
      }
    }
    data.tracks.append(trackData);
  }

  file.close();
  data = sanitize(data);
  return true;
}

SaveData SaveManager::sanitize(const SaveData &data) {
  SaveData clean = data;
  clean.videoVolume = qBound(0.0f, clean.videoVolume, 1.0f);
  clean.trackCount = qBound(1, clean.trackCount, MAX_TRACKS);
  clean.scrollSpeed = qBound(10, clean.scrollSpeed, 500);

  for (int i = 0; i < clean.audioTracks.size(); ++i) {
    clean.audioTracks[i].audioGain =
        qBound(0.0f, clean.audioTracks[i].audioGain, 1.0f);
  }

  return clean;
}

QString SaveManager::resolveProjectPath(const QString &projectDir,
                                        const QString &storedPath,
                                        bool strictRelative,
                                        bool allowOutside) {
  if (storedPath.isEmpty())
    return QString();

#ifdef Q_OS_WIN
  constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;
  if (storedPath.startsWith("\\\\") || storedPath.startsWith("//"))
    return QString(); // UNC
#else
  constexpr Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif

  if (QFileInfo(storedPath).isAbsolute())
    return strictRelative ? QString() : storedPath;

  // Resolve against the canonical root so that an existing target (canonical)
  // and a missing one (cleaned) are compared on the same footing.
  const QString root = QFileInfo(projectDir).canonicalFilePath();
  if (root.isEmpty())
    return QString();

  const QString abs = QDir(root).absoluteFilePath(storedPath);
  if (allowOutside)
    return QDir::cleanPath(abs);

  const QString canonical = QFileInfo(abs).canonicalFilePath();
  const QString resolved = canonical.isEmpty() ? QDir::cleanPath(abs) : canonical;

  // Qt returns '/'-separated paths on every platform.
  const QStringList rootParts = root.split('/', Qt::SkipEmptyParts);
  if (rootParts.isEmpty())
    return QString(); // projet à la racine du système : rien n'est « dedans »
  const QStringList parts = resolved.split('/', Qt::SkipEmptyParts);
  if (parts.size() <= rootParts.size())
    return QString();
  for (int i = 0; i < rootParts.size(); ++i) {
    if (parts[i].compare(rootParts[i], cs) != 0)
      return QString();
  }
  return resolved;
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
