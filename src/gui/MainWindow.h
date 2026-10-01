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

#include "../core/TakeTrack.h"

// Forward declarations - Core layer
class PlaybackEngine;
class RythmoManager;
class AudioRecorder;
class ExportService;
class SaveManager;
class TrackPlayer;
struct SaveData;
struct ProjectMedia;

// GUI includes
class VideoWidget;
class RythmoOverlay;
class TrackWidget;
class ClickableSlider;
class TakeTimeline;
class QUndoStack;
class QSplitter;
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
  // Play/pause; during a take it stops the recording, never the video alone
  void togglePlayback();

  // Export
  void onExportProgress(int percentage);
  void onExportFinished(bool success, const QString &message);
  void showExportDialog();
  bool exportLocksTakes();

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

  // Take playback follows the video
  void syncTrackPlayers();

private:
  enum class PendingSave { None, Save, SaveAs };

  void setupUi();
  // Grows the timeline panel to show every track; never shrinks a panel the user enlarged
  void fitTimeline();
  void createMenus();
  void setupConnections();
  void setupShortcuts();
  void applyShortcuts();
  void loadStylesheet();
  void enterFullscreenRecording();
  void exitFullscreenRecording();
  void updateVolumeIcon(int value);
  // Single entry point for every take/comp change; editTakes() records it for undo
  void applyTrackEdit(int trackIndex, const TakeTrack &takes);
  void editTakes(int trackIndex, const TakeTrack &takes, const QString &label);
  void applyRythmoText(int trackIndex, const QString &text);
  // A track count change the user asked for, undoable (loading uses setTrackCount)
  void changeTrackCount(int count);
  // Opening, exporting and undoing stay out of reach while a take is recorded
  void updateEditActions();
  void toggleWindowFullScreen();
  QString sessionDir() const;
  QString sessionTakePath(int trackIndex, int takeId) const;
  // Rewrites take files relative to the project and lists the copies to make
  QList<ProjectMedia> attachProjectMedia(SaveData &saveData, const QString &audioDirName) const;
  void showPostRecordBar(const QString &message = QString());
  void checkRecordedTake(int trackIndex);
  void hidePostRecordBar();
  SaveData collectSaveData();
  void saveProjectTo(const QString &fileName, bool saveWithVideo);
  bool deferSaveDuringTake(PendingSave save);
  QString autosaveFilePath() const;
  QString recordIdleText() const;
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
  TakeTimeline *m_takeTimeline;
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
  QSplitter *m_videoSplitter;
  QFrame *m_videoFrame;
  QWidget *m_fullscreenContainer;

  // Menus and Actions
  QAction *m_actionOpenMp4;
  QAction *m_actionLoadProject;
  QAction *m_actionSaveProject;
  QAction *m_actionManualExport;

  QAction *m_actionFullscreen;
  QAction *m_actionWindowFullScreen;
  QAction *m_actionUndo;
  QAction *m_actionRedo;
  QUndoStack *m_undoStack;
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
  QElapsedTimer m_recordingTimer;
  QTimer *m_recordDurationTimer;
  // Punch range of the last take (pre-roll excluded)
  qint64 m_lastRecordedDurationMs;
  qint64 m_lastRecordedStartMs;

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

  // Takes and comp of each track; their WAVs live in sessionDir()
  QVector<TakeTrack> m_takeTracks;
  QVector<TrackPlayer *> m_trackPlayers;
  int m_nextTakeId = 1;
  // Take being written by each armed track, added once its WAV is finalized
  QHash<int, Take> m_recordingTakes;
  // Armed tracks whose recorder has not reached StoppedState yet, and takes found empty
  QList<int> m_pendingTakeChecks;
  QList<int> m_failedTakeTracks;
  PendingSave m_pendingSave = PendingSave::None;

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
  QShortcut *m_shFullscreenEscape;
  QKeySequence m_shortcutPlayPause;
  QKeySequence m_shortcutFrameBack;
  QKeySequence m_shortcutFrameForward;
  QKeySequence m_shortcutSeekBack5s;
  QKeySequence m_shortcutSeekForward5s;
  QKeySequence m_shortcutVolumeUp;
  QKeySequence m_shortcutVolumeDown;
  QKeySequence m_shortcutVolumeMute;
  QKeySequence m_shortcutTakeSplit;
  QKeySequence m_shortcutGoToStart;
};

#endif // MAINWINDOW_H
