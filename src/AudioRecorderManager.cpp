#include "AudioRecorderManager.h"
#include <QMediaFormat>

AudioRecorderManager::AudioRecorderManager(QObject *parent) : QObject(parent) {
  m_audioInput = new QAudioInput(this);
  m_recorder = new QMediaRecorder(this);

  // Force format to be simple/compatible (e.g. Wave or safe default)
  QMediaFormat format;
  format.setFileFormat(QMediaFormat::Wave);
  format.setAudioCodec(QMediaFormat::AudioCodec::Wave);
  m_recorder->setMediaFormat(format);

  m_captureSession.setAudioInput(m_audioInput);
  m_captureSession.setRecorder(m_recorder);

  // Default device
  m_audioInput->setDevice(QMediaDevices::defaultAudioInput());

  // Connect signals
  connect(m_recorder, &QMediaRecorder::errorOccurred, this,
          [this](QMediaRecorder::Error error, const QString &errorString) {
            Q_UNUSED(error);
            qWarning() << "AudioRecorderManager Error:" << errorString;
            emit errorOccurred(errorString);
          });

  connect(m_recorder, &QMediaRecorder::durationChanged, this,
          &AudioRecorderManager::durationChanged);
  connect(m_recorder, &QMediaRecorder::recorderStateChanged, this,
          &AudioRecorderManager::recorderStateChanged);
}

QList<QAudioDevice> AudioRecorderManager::availableDevices() const {
  return QMediaDevices::audioInputs();
}

void AudioRecorderManager::setDevice(const QAudioDevice &device) {
  m_audioInput->setDevice(device);
}

void AudioRecorderManager::setVolume(float volume) {
  m_audioInput->setVolume(volume);
}

void AudioRecorderManager::startRecording(const QUrl &outputUrl) {
  m_recorder->setOutputLocation(outputUrl);
  m_recorder->record();
}

void AudioRecorderManager::stopRecording() { m_recorder->stop(); }

QMediaRecorder::RecorderState AudioRecorderManager::recorderState() const {
  return m_recorder->recorderState();
}
