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
#include <QCloseEvent>
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
#include <memory>
#include <QShortcut>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QVector>

#include <QAction>
#include <QLineEdit>
#include <QLockFile>
#include <QMenuBar>
#include <QToolButton>
#include <QWidgetAction>

// Forward declarations - Core layer
class PlaybackEngine;
class RythmoManager;
class AudioRecorder;
class ExportService;
class SaveManager;
struct SaveData;

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
 * Large orchestrator: it also carries project save/load, recording and export
 * flow logic, not just signal wiring.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  bool event(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private slots:
  // File operations
  void onOpenFile();
  void onSaveProject();
  void onQuickSaveProject();
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
  enum class PendingSave { None, Save, SaveAs };

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
  void showPostRecordBar(const QString &message = QString());
  void checkRecordedTake(int trackIndex);
  void hidePostRecordBar();
  SaveData collectSaveData();
  void saveProjectTo(const QString &fileName, bool saveWithVideo);
  bool deferSaveDuringTake(PendingSave save);
  QString autosaveFilePath() const;
  void discardAutosave();
  void checkForAutosaveRecovery();
  // strictRelative: project extracted from a .zip, see SaveManager::resolveProjectPath
  bool loadProjectFrom(const QString &path, bool strictRelative = false);
  // Extracts a .zip into a fresh work dir (handed back through workDir) and
  // returns the path of its .dbi, or an empty string after showing the error.
  QString extractProjectArchive(const QString &zipPath,
                                std::unique_ptr<QTemporaryDir> &workDir);
  void openVideoDialog();
  void setDirty(bool dirty);
  void updateWindowTitle();
  bool maybeSaveChanges();
  void cleanupTempAudioFiles();
  void purgeStaleTempFiles();

  // Dynamic track management
  void setTrackCount(int count);
  void connectTrack(int index);

  // Playback helpers
  qint64 frameStepMs() const;

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
  QProgressBar *m_exportProgressBar;
  QPushButton *m_exportCancelBtn;

  // Fullscreen recording
  QFrame *m_videoFrame;
  QWidget *m_fullscreenContainer;

  // Menus and Actions
  QAction *m_actionOpenMp4;
  QAction *m_actionLoadProject;
  QAction *m_actionSaveProject;
  QAction *m_actionManualExport;

  QAction *m_actionFullscreen;
  QAction *m_actionGlobalSettings;

  QAction *m_actionPersonalizeRythmo;

  // Post-record notification bar
  QWidget *m_postRecordBar;
  QLabel *m_postRecordLabel;

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
  qint64 m_lastRecordedStartMs;
  qint64 m_recordingStartTimeMs;

  // Project state & Autosave recovery
  bool m_isDirty;
  QString m_currentVideoPath;
  QString m_currentProjectPath;
  // Extracted .zip project: PlaybackEngine plays its video in place, so it
  // lives as long as the project stays open.
  std::unique_ptr<QTemporaryDir> m_archiveWorkDir;
  int m_lastLoadLostTracksCount;
  QLockFile m_autosaveLock{autosaveFilePath() + QStringLiteral(".lock")};
  bool m_ownsAutosave = false;

  // Per-track recording state
  QVector<bool> m_hasRecording;
  QVector<qint64> m_trackRecordStartMs;
  QVector<qint64> m_trackRecordDurationMs;
  // Armed tracks whose recorder has not reached StoppedState yet, and takes found empty
  QList<int> m_pendingTakeChecks;
  QList<int> m_failedTakeTracks;
  PendingSave m_pendingSave = PendingSave::None;

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
  QShortcut *m_shProjectSave;
  QShortcut *m_shProjectSaveAs;
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
