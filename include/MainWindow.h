#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AudioRecorderManager.h"
#include "ClickableSlider.h"
#include "Exporter.h"
#include "PlayerController.h"
#include "RythmoOverlay.h"
#include "TrackPanel.h"
#include "VideoWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

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
  // QWidget *m_overlayWidget; // Replaced by RythmoOverlay
  PlayerController *m_playerController;

  // Phase 2 Managers
  RythmoOverlay *m_rythmoOverlay;
  AudioRecorderManager *m_recorderManager;
  AudioRecorderManager *m_recorderManager2;

  TrackPanel *m_track1Panel;
  TrackPanel *m_track2Panel;

  Exporter *m_exporter;

  // UI Elements
  QPushButton *m_openButton;
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;
  ClickableSlider *m_positionSlider;
  ClickableSlider *m_volumeSlider;
  QPushButton *m_volumeButton;
  QSpinBox *m_volumeSpinBox;
  QLabel *m_timeLabel;
  QLabel *m_cursorTimeLabel;

  // Track 2 Container
  QWidget *m_track2Container;
  QCheckBox *m_enableTrack2Check;

  QPushButton *m_recordButton;
  QSpinBox *m_speedSpinBox;
  QCheckBox *m_textColorCheck;
  QProgressBar *m_exportProgressBar;

  int m_previousVolume;

  // State
  bool m_isRecording = false;
  QString m_tempAudioPath;
  QString m_tempAudioPath2; // For Track 2
  QElapsedTimer m_recordingTimer;
  qint64 m_lastRecordedDurationMs = 0;
  qint64 m_recordingStartTimeMs = 0;
};

#endif // MAINWINDOW_H
