#ifndef TRACKPANEL_H
#define TRACKPANEL_H

#include "AudioRecorderManager.h"
#include <QAudioDevice>
#include <QWidget>

class QComboBox;
class QSpinBox;
class ClickableSlider;
class QLabel;

class TrackPanel : public QWidget {
  Q_OBJECT
public:
  explicit TrackPanel(const QString &title,
                      AudioRecorderManager *recorderManager,
                      QWidget *parent = nullptr);

  void setDevice(const QAudioDevice &device);
  void setVolume(float volume);
  AudioRecorderManager *recorderManager() const;

  // Recording helpers
  void startRecording(const QUrl &outputUrl);
  void stopRecording();

signals:
  void volumeChanged(float volume);

private:
  void setupUi(const QString &title);
  void setupConnections();

  QString m_title;
  AudioRecorderManager *m_recorderManager;

  // UI Elements
  QComboBox *m_inputDeviceCombo;
  ClickableSlider *m_volumeSlider;
  QSpinBox *m_gainSpinBox;
};

#endif // TRACKPANEL_H
