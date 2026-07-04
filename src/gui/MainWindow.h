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

#include <QAudioDevice>
#include <QAudioOutput>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QTimer>
#include <QActionGroup>
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
#include <QVector>

#include <QAction>
#include <QLineEdit>
#include <QMenuBar>
#include <QToolButton>
#include <QWidgetAction>

// Forward declarations - Core layer
class PlaybackEngine;
class RythmoManager;
class AudioRecorder;
class ExportService;
class SaveManager;

// GUI includes
class VideoWidget;
class RythmoOverlay;
class TrackWidget;
class ClickableSlider;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;

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

  static constexpr int MAX_TRACKS = 4;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;

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
  void showExportDialog();

  // Error handling
  void onError(const QString &errorMessage);

  // Global settings & features
  void onOpenGlobalSettings();
  void onAutoSaveTriggered();
  void onOutputDeviceTriggered(QAction *action);
  void applyTheme();
  void updateAudioMenu();
  
  // Countdown
  void onCountdownTick();
  void startRecordingProcess();

  // Preview playback sync
  void handlePreviewSync(qint64 masterPosition);
  void handlePreviewStateChange(QMediaPlayer::PlaybackState state);

private:
  void setupUi();
  void createMenus();
  void setupConnections();
  void setupShortcuts();
  void applyShortcuts();
  void loadStylesheet();
  void enterFullscreenRecording();
  void exitFullscreenRecording();
  void updateVolumeIcon(int value);
  void releasePreviewSource(int trackIndex);
  void refreshPreviewSources();
  void showPostRecordBar();
  void hidePostRecordBar();

  // Dynamic track management
  void setTrackCount(int count);
  void connectTrack(int index);

  // =========================================================================
  // Core Services (Business Logic)
  // =========================================================================

  PlaybackEngine *m_playbackEngine;
  RythmoManager *m_rythmoManager;
  QVector<AudioRecorder *> m_audioRecorders;
  ExportService *m_exportService;
  SaveManager *m_saveManager;

  // =========================================================================
  // GUI Components
  // =========================================================================

  VideoWidget *m_videoWidget;
  RythmoOverlay *m_rythmoOverlay;
  QVector<TrackWidget *> m_trackPanels;
  QHBoxLayout *m_tracksLayout;

  // Playback controls
  QPushButton *m_stepBackButton;
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;
  QPushButton *m_stepForwardButton;
  ClickableSlider *m_positionSlider;
  QLabel *m_timeLabel;
  QLabel *m_recordDurationLabel;

  // Volume controls
  QPushButton *m_volumeMuteButton;
  QPushButton *m_volumeDownButton;
  QPushButton *m_volumeUpButton;
  ClickableSlider *m_volumeSlider;
  QSpinBox *m_volumeSpinBox;

  // Recording controls
  QPushButton *m_recordButton;
  QPushButton *m_speedDownButton;
  QPushButton *m_speedUpButton;
  QPushButton *m_speedResetButton;
  QSpinBox *m_speedSpinBox;
  QCheckBox *m_textColorCheck;
  QProgressBar *m_exportProgressBar;
  QPushButton *m_exportCancelBtn;

  // Track count controls
  QLabel *m_trackCountLabel;

  // Fullscreen recording
  QFrame *m_videoFrame;
  QWidget *m_fullscreenContainer;
  QMenu *m_shortcutsMenu;

  // Menus and Actions
  QAction *m_actionOpenMp4;
  QAction *m_actionLoadProject;
  QAction *m_actionSaveProject;
  QAction *m_actionManualExport;

  QAction *m_actionExpertMode;
  QAction *m_actionFullscreen;
  QAction *m_actionGlobalSettings;

  QAction *m_actionPersonalizeRythmo;
  QAction *m_actionExportRythmo;

  // Post-record notification bar
  QWidget *m_postRecordBar;

  // =========================================================================
  // State
  // =========================================================================

  int m_trackCount;
  int m_previousVolume;
  bool m_isRecording;
  bool m_isFullscreenRecording;
  QStringList m_tempAudioPaths;
  QElapsedTimer m_recordingTimer;
  QTimer *m_recordDurationTimer;
  qint64 m_lastRecordedDurationMs;
  qint64 m_recordingStartTimeMs;

  // Per-track recording state
  QVector<bool> m_hasRecording;
  QVector<qint64> m_trackRecordStartMs;
  QVector<qint64> m_trackRecordDurationMs;

  // Preview playback
  QVector<QMediaPlayer *> m_previewPlayers;
  QVector<QAudioOutput *> m_previewOutputs;
  QVector<QElapsedTimer> m_trackSyncThrottle;

  // Advanced settings members
  QTimer *m_autoSaveTimer;
  QMenu *m_audioMenu;
  QActionGroup *m_outputDevicesGroup;

  // Countdown overlay members
  QLabel *m_countdownLabel;
  QTimer *m_countdownTimer;
  int m_countdownRemaining;

  // Shortcuts members
  QShortcut *m_shRecordStart;
  QShortcut *m_shRecordStop;
  QKeySequence m_shortcutPlayPause;
  QKeySequence m_shortcutFrameBack;
  QKeySequence m_shortcutFrameForward;
  QKeySequence m_shortcutSeekBack5s;
  QKeySequence m_shortcutSeekForward5s;
  QKeySequence m_shortcutVolumeUp;
  QKeySequence m_shortcutVolumeDown;
  QKeySequence m_shortcutVolumeMute;
};

#endif // MAINWINDOW_H
