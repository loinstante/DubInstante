#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AudioRecorderManager.h"
#include "ClickableSlider.h"
#include "Exporter.h"
#include "PlayerController.h"
#include "RythmoWidget.h"
#include "VideoWidget.h"

#include <QComboBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onOpenFile();
  void updatePosition(qint64 position);
  void updateDuration(qint64 duration);
  void updatePlayPauseButton(QMediaPlayer::PlaybackState state);
  void handleError(const QString &errorMessage);

  // Phase 2 Slots
  void toggleRecording();
  void onExportFinished(bool success, const QString &message);
  void updateExportProgress(int percentage);

private:
  void setupUi();
  void setupConnections();
  QString formatTime(qint64 milliseconds) const;

  VideoWidget *m_videoWidget;
  PlayerController *m_playerController;

  // Phase 2 Managers
  RythmoWidget *m_rythmoWidget;
  AudioRecorderManager *m_recorderManager;
  Exporter *m_exporter;

  // UI Elements
  QPushButton *m_openButton;
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;
  ClickableSlider *m_positionSlider; // Changed type
  ClickableSlider *m_volumeSlider;   // Changed type
  QPushButton *m_volumeButton;
  QSpinBox *m_volumeSpinBox; // Added
  QLabel *m_timeLabel;
  QLabel *m_cursorTimeLabel; // Added timestamp label

  // Audio / Export Elements
  QComboBox *m_inputDeviceCombo;
  ClickableSlider *m_micVolumeSlider; // Changed type
  QPushButton *m_recordButton;
  // QLineEdit *m_textEditField; // Removed - editing is now in RythmoWidget
  QSpinBox *m_speedSpinBox;
  QProgressBar *m_exportProgressBar;
  QSpinBox *m_micGainSpinBox; // Added

  int m_previousVolume;

  // State
  bool m_isRecording = false;
  QString m_tempAudioPath;
  QElapsedTimer m_recordingTimer;
  qint64 m_lastRecordedDurationMs = 0;
  qint64 m_recordingStartTimeMs = 0;
  bool m_isUpdatingCursorFromVideo = false; // Guard for reverse sync
};

#endif // MAINWINDOW_H
