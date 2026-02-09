/**
 * @file AudioRecorder.h
 * @brief Core manager for audio input and recording.
 * 
 * This class handles microphone input capture and recording to file.
 * It wraps Qt's audio capture API (QMediaCaptureSession, QMediaRecorder)
 * with a clean interface.
 * 
 * @note Part of the Core layer - no UI dependencies allowed.
 */

#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QAudioDevice>
#include <QAudioInput>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaRecorder>
#include <QObject>
#include <QUrl>

/**
 * @class AudioRecorder
 * @brief Manages audio input device selection and recording.
 * 
 * Responsibilities:
 * - Enumerate available audio input devices
 * - Select and configure audio input
 * - Start/stop recording to file
 * - Report recording state and errors
 * 
 * @example
 * @code
 * auto recorder = new AudioRecorder(this);
 * recorder->setDevice(recorder->availableDevices().first());
 * recorder->startRecording(QUrl::fromLocalFile("/path/to/output.wav"));
 * // ... later
 * recorder->stopRecording();
 * @endcode
 */
class AudioRecorder : public QObject {
    Q_OBJECT

public:
    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder() override = default;

    // =========================================================================
    // Device Management
    // =========================================================================
    
    /**
     * @brief Returns a list of available audio input devices.
     * @return List of QAudioDevice representing microphones.
     */
    QList<QAudioDevice> availableDevices() const;
    
    /**
     * @brief Sets the audio input device to use.
     * @param device The device to use for recording.
     */
    void setDevice(const QAudioDevice &device);
    
    /**
     * @brief Sets the input volume/gain.
     * @param volume Volume level from 0.0 (mute) to 1.0 (max).
     */
    void setVolume(float volume);

    // =========================================================================
    // Recording Control
    // =========================================================================
    
    /**
     * @brief Starts recording audio to the specified file.
     * @param outputUrl File URL where the audio will be saved.
     */
    void startRecording(const QUrl &outputUrl);
    
    /**
     * @brief Stops the current recording.
     */
    void stopRecording();
    
    /**
     * @brief Returns the current recorder state.
     * @return Current state (Recording, Paused, Stopped).
     */
    QMediaRecorder::RecorderState recorderState() const;

signals:
    /**
     * @brief Emitted when an error occurs during recording.
     * @param error Human-readable error message.
     */
    void errorOccurred(const QString &error);
    
    /**
     * @brief Emitted periodically with the current recording duration.
     * @param duration Recording duration in milliseconds.
     */
    void durationChanged(qint64 duration);
    
    /**
     * @brief Emitted when the recorder state changes.
     * @param state New recorder state.
     */
    void recorderStateChanged(QMediaRecorder::RecorderState state);

private:
    QMediaCaptureSession m_captureSession;
    QAudioInput *m_audioInput;
    QMediaRecorder *m_recorder;
};

#endif // AUDIORECORDER_H
