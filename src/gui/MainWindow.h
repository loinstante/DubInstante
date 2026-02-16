/**
 * @file MainWindow.h
 * @brief Main application window.
 *
 * This is the primary UI class that orchestrates all components.
 * It creates and connects Core services to GUI widgets but contains
 * NO business logic itself.
 *
 * @note Part of the GUI layer - wiring only, no calculations.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCheckBox>
#include <QElapsedTimer>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>

// Forward declarations - Core layer
class PlaybackEngine;
class RythmoManager;
class AudioRecorder;
class ExportService;
class SaveManager;

// Forward declarations - GUI layer
class VideoWidget;
class RythmoOverlay;
class TrackPanel;
class ClickableSlider;

// Forward declaration - Utils
struct ExportConfig;

/**
 * @class MainWindow
 * @brief Main application window orchestrating Core and GUI components.
 *
 * Responsibilities:
 * - Create and own Core services
 * - Create and layout GUI widgets
 * - Wire signals/slots between Core and GUI
 * - Handle top-level menu and keyboard shortcuts
 *
 * This class should be "thin" - it connects components but doesn't
 * contain business logic.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  // File operations
  void onOpenFile();
  void onSaveProject();
  void onLoadProject();

  // Playback UI updates
  void onPositionChanged(qint64 position);
  void onDurationChanged(qint64 duration);
  void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

  // Recording
  void toggleRecording();

  // Export
  void onExportProgress(int percentage);
  void onExportFinished(bool success, const QString &message);

  // Error handling
  void onError(const QString &errorMessage);

private:
  void setupUi();
  void setupConnections();
  void setupShortcuts();
  void loadStylesheet();
  void enterFullscreenRecording();
  void exitFullscreenRecording();
  void showShortcutsPopup();

  // =========================================================================
  // Core Services (Business Logic)
  // =========================================================================

  PlaybackEngine *m_playbackEngine;
  RythmoManager *m_rythmoManager;
  AudioRecorder *m_audioRecorder1;
  AudioRecorder *m_audioRecorder2;
  ExportService *m_exportService;
  SaveManager *m_saveManager;

  // =========================================================================
  // GUI Components
  // =========================================================================

  VideoWidget *m_videoWidget;
  RythmoOverlay *m_rythmoOverlay;
  TrackPanel *m_track1Panel;
  TrackPanel *m_track2Panel;

  // Playback controls
  QPushButton *m_openButton;
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;
  ClickableSlider *m_positionSlider;
  QLabel *m_timeLabel;

  // Volume controls
  QPushButton *m_volumeButton;
  ClickableSlider *m_volumeSlider;
  QSpinBox *m_volumeSpinBox;

  // Recording controls
  QPushButton *m_recordButton;
  QSpinBox *m_speedSpinBox;
  QCheckBox *m_textColorCheck;
  QCheckBox *m_fullscreenRecordingCheck;
  QProgressBar *m_exportProgressBar;

  // Track 2 controls
  QWidget *m_track2Container;
  QCheckBox *m_enableTrack2Check;

  // Fullscreen recording
  QFrame *m_videoFrame;
  QWidget *m_fullscreenContainer;
  QPushButton *m_shortcutsButton;
  QMenu *m_shortcutsMenu;

  // =========================================================================
  // State
  // =========================================================================

  int m_previousVolume;
  bool m_isRecording;
  bool m_isFullscreenRecording;
  QString m_tempAudioPath1;
  QString m_tempAudioPath2;
  QElapsedTimer m_recordingTimer;
  qint64 m_lastRecordedDurationMs;
  qint64 m_recordingStartTimeMs;
};

#endif // MAINWINDOW_H
