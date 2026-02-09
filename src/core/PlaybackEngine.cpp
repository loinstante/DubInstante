/**
 * @file PlaybackEngine.cpp
 * @brief Implementation of the PlaybackEngine class.
 */

#include "PlaybackEngine.h"

#include <QMediaMetaData>
#include <QVideoSink>

PlaybackEngine::PlaybackEngine(QObject *parent)
    : QObject(parent), m_mediaPlayer(new QMediaPlayer(this)),
      m_audioOutput(new QAudioOutput(this)) {
  m_mediaPlayer->setAudioOutput(m_audioOutput);
  m_audioOutput->setVolume(1.0f);

  // Forward signals from QMediaPlayer
  connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this,
          &PlaybackEngine::positionChanged);
  connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this,
          &PlaybackEngine::durationChanged);
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this,
          &PlaybackEngine::playbackStateChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
          &PlaybackEngine::mediaStatusChanged);
  connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged, this,
          &PlaybackEngine::metaDataChanged);

  // Forward volume signal from audio output
  connect(m_audioOutput, &QAudioOutput::volumeChanged, this,
          &PlaybackEngine::volumeChanged);

  // Convert error signal to string-based signal
  connect(m_mediaPlayer, &QMediaPlayer::errorOccurred, this,
          [this](QMediaPlayer::Error error, const QString &errorString) {
            Q_UNUSED(error)
            emit errorOccurred(errorString);
          });
}

void PlaybackEngine::setVideoSink(QVideoSink *sink) {
  m_mediaPlayer->setVideoOutput(sink);
}

void PlaybackEngine::openFile(const QUrl &url) {
  m_mediaPlayer->setSource(url);
  // Reverted immediate pause() to prevent GStreamer crash
}

void PlaybackEngine::setVolume(float volume) {
  m_audioOutput->setVolume(volume);
}

void PlaybackEngine::play() { m_mediaPlayer->play(); }

void PlaybackEngine::pause() { m_mediaPlayer->pause(); }

void PlaybackEngine::stop() { m_mediaPlayer->stop(); }

void PlaybackEngine::seek(qint64 position) {
  m_mediaPlayer->setPosition(position);
}

qint64 PlaybackEngine::duration() const { return m_mediaPlayer->duration(); }

qint64 PlaybackEngine::position() const { return m_mediaPlayer->position(); }

QMediaPlayer::PlaybackState PlaybackEngine::playbackState() const {
  return m_mediaPlayer->playbackState();
}

float PlaybackEngine::volume() const { return m_audioOutput->volume(); }

qreal PlaybackEngine::videoFrameRate() const {
  QVariant rate =
      m_mediaPlayer->metaData().value(QMediaMetaData::VideoFrameRate);
  if (rate.isValid() && rate.toReal() > 0) {
    return rate.toReal();
  }
  return 25.0; // Default to 25 FPS if unknown
}
