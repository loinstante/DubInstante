#ifndef EXPORTER_H
#define EXPORTER_H

#include <QObject>
#include <QProcess>
#include <QString>

/**
 * @brief Handles post-processing exports using FFmpeg.
 */
class Exporter : public QObject {
  Q_OBJECT

public:
  explicit Exporter(QObject *parent = nullptr);

  /**
   * @brief Merges video and audio files.
   * @param videoPath Absolute path to source video.
   * @param audioPath Absolute path to recorded audio.
   * @param outputPath Absolute path for the result.
   * @param secondAudioPath Optional absolute path to second recorded audio.
   */
  void merge(const QString &videoPath, const QString &audioPath,
             const QString &outputPath, qint64 durationMs = -1,
             qint64 startTimeMs = 0, float originalVolume = 1.0f,
             const QString &secondAudioPath = QString());

  void setTotalDuration(qint64 durationMs);

  // Helper to check if ffmpeg is available
  bool isFFmpegAvailable() const;

signals:
  void progressChanged(int percentage);
  void finished(bool success, const QString &message);

private:
  QProcess *m_process;
  qint64 m_totalDurationMs;
};

#endif // EXPORTER_H
