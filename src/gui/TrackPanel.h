/**
 * @file TrackPanel.h
 * @brief UI panel for audio track configuration.
 *
 * This widget provides controls for configuring an audio recording track:
 * device selection, volume, and recording controls.
 *
 * @note Part of the GUI layer - pure UI, delegates to AudioRecorder.
 */

#ifndef TRACKPANEL_H
#define TRACKPANEL_H

#include <QAudioDevice>
#include <QWidget>

class QComboBox;
class QSpinBox;
class ClickableSlider;
class QLabel;
class AudioRecorder;

/**
 * @class TrackPanel
 * @brief Control panel widget for a single audio track.
 *
 * Features:
 * - Audio input device selection
 * - Volume/gain control
 * - Recording start/stop
 */
class TrackPanel : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Constructs a TrackPanel.
   * @param title Display title for the panel.
   * @param recorder AudioRecorder instance to control.
   * @param parent Parent widget.
   */
  explicit TrackPanel(const QString &title, AudioRecorder *recorder,
                      QWidget *parent = nullptr);
  ~TrackPanel() override = default;

  // =========================================================================
  // Configuration
  // =========================================================================

  /** @brief Sets the audio input device. */
  void setDevice(const QAudioDevice &device);

  /** @brief Sets the input volume (0.0 to 1.0). */
  void setVolume(float volume);

  /** @brief Returns the current gain (0.0 to 1.0). */
  float gain() const;

  /** @brief Returns currently selected device. */
  QAudioDevice selectedDevice() const;

  /** @brief Returns the associated recorder. */
  AudioRecorder *recorder() const;

  // =========================================================================
  // Recording Control
  // =========================================================================

  /** @brief Starts recording to the specified file. */
  void startRecording(const QUrl &outputUrl);

  /** @brief Stops the current recording. */
  void stopRecording();

signals:
  /** @brief Emitted when volume slider changes. */
  void volumeChanged(float volume);

private:
  void setupUi(const QString &title);
  void setupConnections();
  void populateDeviceList();

  QString m_title;
  AudioRecorder *m_recorder;

  // UI Elements
  QComboBox *m_inputDeviceCombo;
  ClickableSlider *m_volumeSlider;
  QSpinBox *m_gainSpinBox;
};

#endif // TRACKPANEL_H
