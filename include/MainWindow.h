#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "PlayerController.h"
#include "VideoWidget.h"
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

private slots:
  void onOpenFile();
  void updatePosition(qint64 position);
  void updateDuration(qint64 duration);
  void updatePlayPauseButton(QMediaPlayer::PlaybackState state);
  void handleError(const QString &errorMessage);

private:
  void setupUi();
  void setupConnections();
  QString formatTime(qint64 milliseconds) const;

  VideoWidget *m_videoWidget;
  PlayerController *m_playerController;

  QPushButton *m_openButton;
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;
  QSlider *m_positionSlider;
  QSlider *m_volumeSlider;
  QPushButton *m_volumeButton;
  QLabel *m_timeLabel;

  int m_previousVolume; // Remember volume before muting
};

#endif // MAINWINDOW_H
