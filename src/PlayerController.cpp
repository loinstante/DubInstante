#include "PlayerController.h"
#include <QVideoSink>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent), m_mediaPlayer(new QMediaPlayer(this)),
      m_audioOutput(new QAudioOutput(this)) {
  m_mediaPlayer->setAudioOutput(m_audioOutput);
  m_audioOutput->setVolume(1.0);

  // Forward signals from QMediaPlayer
  connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this,
          &PlayerController::positionChanged);
  connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this,
          &PlayerController::durationChanged);
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this,
          &PlayerController::playbackStateChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
          &PlayerController::mediaStatusChanged);

  // Volume signal from audio output
  connect(m_audioOutput, &QAudioOutput::volumeChanged, this,
          &PlayerController::volumeChanged);

  connect(m_mediaPlayer, &QMediaPlayer::errorOccurred, this,
          [this](QMediaPlayer::Error error, const QString &errorString) {
            emit errorOccurred(errorString);
          });
}

void PlayerController::setVideoSink(QVideoSink *sink) {
  m_mediaPlayer->setVideoOutput(sink);
}

void PlayerController::openFile(const QUrl &url) {
  m_mediaPlayer->setSource(url);
}

void PlayerController::setVolume(float volume) {
  m_audioOutput->setVolume(volume);
}

void PlayerController::play() { m_mediaPlayer->play(); }
void PlayerController::pause() { m_mediaPlayer->pause(); }
void PlayerController::stop() { m_mediaPlayer->stop(); }
void PlayerController::seek(qint64 position) {
  m_mediaPlayer->setPosition(position);
}

qint64 PlayerController::duration() const { return m_mediaPlayer->duration(); }
qint64 PlayerController::position() const { return m_mediaPlayer->position(); }
QMediaPlayer::PlaybackState PlayerController::playbackState() const {
  return m_mediaPlayer->playbackState();
}
float PlayerController::volume() const { return m_audioOutput->volume(); }
