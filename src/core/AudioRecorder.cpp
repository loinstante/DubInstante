/**
 * @file AudioRecorder.cpp
 * @brief Implementation of the AudioRecorder class.
 */

#include "AudioRecorder.h"

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
    , m_audioInput(new QAudioInput(this))
    , m_recorder(new QMediaRecorder(this))
{
    // Configure the capture session
    m_captureSession.setAudioInput(m_audioInput);
    m_captureSession.setRecorder(m_recorder);

    // Forward recorder signals
    connect(m_recorder, &QMediaRecorder::durationChanged,
            this, &AudioRecorder::durationChanged);
    connect(m_recorder, &QMediaRecorder::recorderStateChanged,
            this, &AudioRecorder::recorderStateChanged);
    
    // Convert recorder error to string signal
    connect(m_recorder, &QMediaRecorder::errorOccurred,
            this, [this](QMediaRecorder::Error error, const QString &errorString) {
                Q_UNUSED(error)
                emit errorOccurred(errorString);
            });
}

// =============================================================================
// Device Management
// =============================================================================

QList<QAudioDevice> AudioRecorder::availableDevices() const
{
    return QMediaDevices::audioInputs();
}

void AudioRecorder::setDevice(const QAudioDevice &device)
{
    m_audioInput->setDevice(device);
}

void AudioRecorder::setVolume(float volume)
{
    m_audioInput->setVolume(volume);
}

// =============================================================================
// Recording Control
// =============================================================================

void AudioRecorder::startRecording(const QUrl &outputUrl)
{
    m_recorder->setOutputLocation(outputUrl);
    m_recorder->record();
}

void AudioRecorder::stopRecording()
{
    m_recorder->stop();
}

QMediaRecorder::RecorderState AudioRecorder::recorderState() const
{
    return m_recorder->recorderState();
}
