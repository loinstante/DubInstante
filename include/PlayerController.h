#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QUrl>

class QVideoSink;

/**
 * @brief The PlayerController class encapsulates the playback logic.
 * It uses QMediaPlayer and provides a clean interface for UI components.
 */
class PlayerController : public QObject {
  Q_OBJECT

public:
  explicit PlayerController(QObject *parent = nullptr);

  void setVideoSink(QVideoSink *sink);
  void openFile(const QUrl &url);

  qint64 duration() const;
  qint64 position() const;
  QMediaPlayer::PlaybackState playbackState() const;
  float volume() const;

public slots:
  void play();
  void pause();
  void stop();
  void seek(qint64 position);
  void setVolume(float volume);

signals:
  void positionChanged(qint64 position);
  void durationChanged(qint64 duration);
  void playbackStateChanged(QMediaPlayer::PlaybackState state);
  void mediaStatusChanged(QMediaPlayer::MediaStatus status);
  void volumeChanged(float volume);
  void errorOccurred(const QString &error);

private:
  QMediaPlayer *m_mediaPlayer;
  QAudioOutput *m_audioOutput;
};

#endif // PLAYERCONTROLLER_H
