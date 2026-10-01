/**
 * @file MainWindow.cpp
 * @brief Implementation of the MainWindow class.
 */

#include "MainWindow.h"

// Core includes
#include "AudioRecorder.h"
#include "Constants.h"
#include "ExportService.h"
#include "ExportDialog.h"
#include "PlaybackEngine.h"
#include "RythmoManager.h"
#include "SaveManager.h"
#include "TrackPlayer.h"

// GUI includes
#include "ClickableSlider.h"
#include "RythmoOverlay.h"
#include "TakeTimeline.h"
#include "Palette.h"
#include "TrackWidget.h"
#include "TrackSettingsDialog.h"
#include "GlobalSettingsDialog.h"
#include "../core/SettingsManager.h"
#include "VideoWidget.h"
#include <QApplication>
#include <QPalette>
#include <QStyleHints>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QAudioDevice>

// Utils includes
#include "TimeFormatter.h"

#include <QDir>
#include <QEventLoop>
#include <QGraphicsDropShadowEffect>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLocale>
#include <QMessageBox>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

#include <QFutureWatcher>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QUndoStack>
#include <functional>

namespace {

// An undoable edit: the closures reapply the values before and after it.
class EditCommand : public QUndoCommand {
public:
  EditCommand(const QString &text, std::function<void()> undo,
              std::function<void()> redo)
      : QUndoCommand(text), m_undo(std::move(undo)), m_redo(std::move(redo)) {}
  void undo() override { m_undo(); }
  void redo() override { m_redo(); }

private:
  std::function<void()> m_undo;
  std::function<void()> m_redo;
};

// Keystrokes on one band merge into a single step until typing pauses, so
// undo does not go back one letter at a time.
class TypingCommand : public QUndoCommand {
public:
  using Apply = std::function<void(int, const QString &)>;

  TypingCommand(int track, const QString &before, const QString &after, Apply apply)
      //: Undo step: text typed on a rythmo band
      : QUndoCommand(QCoreApplication::translate("MainWindow", "Typing on the band")),
        m_track(track),
        m_before(before), m_after(after), m_apply(std::move(apply)) {
    m_lastKey.start();
  }
  int id() const override { return 1; }
  bool mergeWith(const QUndoCommand *other) override {
    const auto *next = static_cast<const TypingCommand *>(other);
    if (next->m_track != m_track || m_lastKey.elapsed() > 1500)
      return false;
    m_after = next->m_after;
    m_lastKey.restart();
    setObsolete(m_after == m_before);
    return true;
  }
  void undo() override { m_apply(m_track, m_before); }
  void redo() override { m_apply(m_track, m_after); }

private:
  int m_track;
  QString m_before;
  QString m_after;
  Apply m_apply;
  QElapsedTimer m_lastKey;
};

} // namespace
#include <QtConcurrent>
#include <algorithm>
#include <memory>
#include <limits>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      // Initialize Core services
      ,
      m_playbackEngine(new PlaybackEngine(this)),
      m_rythmoManager(new RythmoManager(this)),
      m_exportService(new ExportService(this)),
      m_saveManager(new SaveManager(this)),
      m_postRecordBar(nullptr),
      m_postRecordLabel(nullptr)
      // Initialize state
      ,
      m_trackCount(0), m_previousVolume(100), m_isRecording(false),
      m_isFullscreenRecording(false), m_lastRecordedDurationMs(0),
      m_lastRecordedStartMs(0),
      m_isDirty(false), m_lastLoadLostTracksCount(0),
      m_autoSaveTimer(new QTimer(this)),
      m_outputDevicesGroup(new QActionGroup(this)),
      m_countdownLabel(nullptr),
      m_countdownTimer(new QTimer(this)),
      m_countdownRemaining(0) {
  applyTheme();
  setupUi();
  createMenus();
  setupConnections();
  setupShortcuts();

  // Connect video sink
  m_playbackEngine->setVideoSink(m_videoWidget->videoSink());

  // Setup auto-save connection & configure timer
  connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::onAutoSaveTriggered);
  if (SettingsManager::instance().autoSaveEnabled()) {
    m_autoSaveTimer->start(SettingsManager::instance().autoSaveInterval() * 60 * 1000);
  }

  // Setup countdown connection
  connect(m_countdownTimer, &QTimer::timeout, this, &MainWindow::onCountdownTick);


  // Create initial track (always start with 1)
  setTrackCount(1);

  // Restore preferred active audio output device
  QString activeProfile = SettingsManager::instance().activeOutputProfile();
  if (!activeProfile.isEmpty()) {
    QStringList parts = activeProfile.split("|");
    if (parts.size() >= 2) {
      QString devDesc = parts[1];
      for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        if (device.description() == devDesc) {
          m_playbackEngine->setAudioDevice(device);
          break;
        }
      }
    }
  }

  // Window configuration
  updateWindowTitle();
  resize(900, 600);
  setMinimumSize(800, 540);
  setWindowState(Qt::WindowMaximized);

  // Construire l'interface a émis des signaux de mutation (peuplement des combos,
  // volumes par défaut) : un projet vierge n'est pas modifié pour autant.
  setDirty(false);

  // Une seule instance possède la sauvegarde automatique : sans ce verrou, une
  // seconde instance proposerait de restaurer celle de la première, puis la
  // supprimerait en se fermant. Le verrou d'une instance plantée est repris
  // (processus disparu).
  // ponytail: les instances suivantes ne sont pas protégées contre un crash ;
  // passer à une sauvegarde par instance si l'usage multi-fenêtre se répand.
  QDir().mkpath(QFileInfo(autosaveFilePath()).absolutePath());
  m_autosaveLock.setStaleLockTime(0);
  m_ownsAutosave = m_autosaveLock.tryLock(0);

  // Différé : le constructeur s'exécute avant show(), un dialogue modal n'aurait
  // pas de fenêtre derrière lui — et la restauration peut en ouvrir un second
  // pour relier une vidéo introuvable. La purge passe après, sinon elle
  // supprimerait les WAV d'une sauvegarde de plus de sept jours avant qu'on ait
  // proposé de la restaurer. Réservée elle aussi au propriétaire du verrou : une
  // instance secondaire ne purge pas les prises d'une autre encore ouverte.
  QTimer::singleShot(0, this, [this]() {
    if (!m_ownsAutosave) {
      return;
    }
    checkForAutosaveRecovery();
    purgeStaleTempFiles();
  });
}

// =============================================================================
// UI Setup
// =============================================================================

namespace {

// Types vidéo acceptés. Le sélecteur de fichiers et la barrière de type du
// chargement de projet lisent la même liste : sans ça, un projet enregistré
// avec un .mkv ne se rouvre pas.
const QStringList &videoSuffixes() {
  static const QStringList suffixes{"mp4", "mkv", "mov", "avi", "m4v", "webm", "mxf"};
  return suffixes;
}

bool hasVideoSuffix(const QString &path) {
  return videoSuffixes().contains(QFileInfo(path).suffix().toLower());
}

// Every role Fusion draws with is set explicitly, including the Disabled group:
// a role left unset falls back to the OS theme palette, which is the opposite
// scheme half the time (light frames under the dark theme). The Disabled
// overrides come last, setColor(role, c) having written all three groups.
QPalette buildPalette(bool dark) {
  QPalette p;
  if (dark) {
    p.setColor(QPalette::Window, QColor("#1b1b1b"));
    p.setColor(QPalette::WindowText, QColor("#e8e6e1"));
    p.setColor(QPalette::Base, QColor("#242424"));
    p.setColor(QPalette::AlternateBase, QColor("#2d2d2d"));
    p.setColor(QPalette::ToolTipBase, QColor("#2d2d2d"));
    p.setColor(QPalette::ToolTipText, QColor("#e8e6e1"));
    p.setColor(QPalette::Text, QColor("#e8e6e1"));
    p.setColor(QPalette::PlaceholderText, QColor(Brand::TextMuted));
    p.setColor(QPalette::Button, QColor("#2d2d2d"));
    p.setColor(QPalette::ButtonText, QColor("#e8e6e1"));
    p.setColor(QPalette::BrightText, QColor("#ffffff"));
    p.setColor(QPalette::Link, QColor("#d27a7f"));
    p.setColor(QPalette::Highlight, QColor(Brand::AccentLight));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Light, QColor(Brand::SurfaceAlt));
    p.setColor(QPalette::Midlight, QColor("#2f2f2f"));
    p.setColor(QPalette::Mid, QColor("#292929"));
    p.setColor(QPalette::Dark, QColor("#151515"));
    p.setColor(QPalette::Shadow, QColor("#000000"));
    const QColor disabled("#77756f");
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  } else {
    p.setColor(QPalette::Window, QColor("#eceae6"));
    p.setColor(QPalette::WindowText, QColor("#1d1d1d"));
    p.setColor(QPalette::Base, QColor("#f7f6f3"));
    p.setColor(QPalette::AlternateBase, QColor("#f1f0ec"));
    p.setColor(QPalette::ToolTipBase, QColor("#f7f6f3"));
    p.setColor(QPalette::ToolTipText, QColor("#1d1d1d"));
    p.setColor(QPalette::Text, QColor("#1d1d1d"));
    p.setColor(QPalette::PlaceholderText, QColor("#64625e"));
    p.setColor(QPalette::Button, QColor("#f1f0ec"));
    p.setColor(QPalette::ButtonText, QColor("#3d3c3a"));
    p.setColor(QPalette::BrightText, QColor("#ffffff"));
    p.setColor(QPalette::Link, QColor(Brand::Accent));
    p.setColor(QPalette::Highlight, QColor(Brand::Accent));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Light, QColor("#fbfaf8"));
    p.setColor(QPalette::Midlight, QColor("#eceae6"));
    p.setColor(QPalette::Mid, QColor("#c2c0ba"));
    p.setColor(QPalette::Dark, QColor("#9c9a95"));
    p.setColor(QPalette::Shadow, QColor("#6b6965"));
    const QColor disabled("#9c9a95");
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  }
  return p;
}

} // namespace

void MainWindow::applyTheme() {
  SettingsManager &sm = SettingsManager::instance();
  QString themeMode = sm.theme();

  // Captured before the first setPalette(): afterwards QGuiApplication::palette()
  // returns our own palette and no longer reflects the OS theme.
  // ponytail: "system" is therefore frozen at startup, an OS theme flip needs a
  // restart. Upgrade path: QStyleHints::colorSchemeChanged (Qt >= 6.5, we build on 6.4).
  static const QPalette systemPalette = QGuiApplication::palette();

  bool isDark = false;
  if (themeMode == "dark") {
    isDark = true;
  } else if (themeMode == "system") {
    isDark = (systemPalette.color(QPalette::Window).value() < 128);
  }

  qApp->setPalette(buildPalette(isDark));

  QString stylesheetPath = isDark ? ":/resources/style_dark.qss" : ":/resources/style.qss";
  
  QFile styleFile(stylesheetPath);
  if (styleFile.open(QFile::ReadOnly)) {
    QString styleSheet = QLatin1String(styleFile.readAll());
    setStyleSheet(styleSheet);
  }
}

void MainWindow::updateVolumeIcon(int value) {
  if (value == 0) {
    m_volumeMuteButton->setIcon(QIcon(":/resources/icons/volume_mute.svg"));
  } else if (value < 50) {
    m_volumeMuteButton->setIcon(QIcon(":/resources/icons/volume_low.svg"));
  } else {
    m_volumeMuteButton->setIcon(QIcon(":/resources/icons/volume.svg"));
  }
}

void MainWindow::showPostRecordBar(const QString &message) {
    if (!message.isEmpty() && m_postRecordLabel) {
        m_postRecordLabel->setText(message);
    }
    m_postRecordBar->setVisible(true);
}

void MainWindow::hidePostRecordBar() {
    m_postRecordBar->setVisible(false);
}

void MainWindow::syncTrackPlayers() {
  const bool playing =
      m_playbackEngine->playbackState() == QMediaPlayer::PlayingState;
  const qint64 position = m_playbackEngine->position();
  for (TrackPlayer *player : m_trackPlayers)
    player->sync(position, playing);
}

void MainWindow::applyTrackEdit(int trackIndex, const TakeTrack &takes) {
  if (trackIndex < 0 || trackIndex >= m_takeTracks.size())
    return;
  m_takeTracks[trackIndex] = takes;
  m_trackPlayers[trackIndex]->setSegments(takes.segments());
  m_takeTimeline->setTrack(trackIndex, takes);
  syncTrackPlayers();
  setDirty(true);
}

void MainWindow::editTakes(int trackIndex, const TakeTrack &takes, const QString &label) {
  if (trackIndex < 0 || trackIndex >= m_takeTracks.size())
    return;
  const TakeTrack before = m_takeTracks[trackIndex];
  m_undoStack->push(new EditCommand(
      label, [this, trackIndex, before]() { applyTrackEdit(trackIndex, before); },
      [this, trackIndex, takes]() { applyTrackEdit(trackIndex, takes); }));
}

void MainWindow::applyRythmoText(int trackIndex, const QString &text) {
  m_rythmoManager->setText(trackIndex, text);
  if (RythmoWidget *widget = m_rythmoOverlay->track(trackIndex)) {
    const QSignalBlocker blocker(widget); // not a new edit to record
    widget->setText(text);
    widget->update();
  }
  setDirty(true);
}

void MainWindow::changeTrackCount(int count) {
  count = qBound(1, count, MAX_TRACKS);
  if (count == m_trackCount || m_isRecording)
    return;
  // A removed track keeps its files in the session dir: undo brings it back whole
  const int before = m_trackCount;
  QStringList texts;
  QVector<TakeTrack> takes;
  for (int i = count; i < before; ++i) {
    texts << m_rythmoManager->text(i);
    takes << m_takeTracks[i];
  }
  m_undoStack->push(new EditCommand(
      count < before ? tr("Remove a band") : tr("Add a band"),
      [this, count, before, texts, takes]() {
        setTrackCount(before);
        for (int i = 0; i < texts.size(); ++i) {
          applyRythmoText(count + i, texts[i]);
          applyTrackEdit(count + i, takes[i]);
        }
      },
      [this, count]() { setTrackCount(count); }));
}

void MainWindow::updateEditActions() {
  const bool idle = !m_isRecording;
  m_actionUndo->setEnabled(idle && m_undoStack->canUndo());
  m_actionRedo->setEnabled(idle && m_undoStack->canRedo());
  m_actionOpenMp4->setEnabled(idle);
  m_actionLoadProject->setEnabled(idle);
  m_actionManualExport->setEnabled(idle);
}

void MainWindow::toggleWindowFullScreen() {
  setWindowState(windowState() ^ Qt::WindowFullScreen); // back to maximized or normal
}

// The start time is part of the name: a pid reused after a reboot must not
// point at the session a crash left behind, which recovery copies from.
QString MainWindow::sessionDir() const {
  static const QString dir =
      QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
          .filePath(QStringLiteral("dubinstante_session_%1_%2")
                        .arg(QCoreApplication::applicationPid())
                        .arg(QDateTime::currentMSecsSinceEpoch()));
  return dir;
}

// The track is part of the name: take ids of a hand-edited file may collide
QString MainWindow::sessionTakePath(int trackIndex, int takeId) const {
  return QDir(sessionDir())
      .filePath(QStringLiteral("track%1_take%2.wav").arg(trackIndex + 1).arg(takeId));
}

QList<ProjectMedia> MainWindow::attachProjectMedia(SaveData &saveData,
                                                   const QString &audioDirName) const {
  QList<ProjectMedia> media;
  for (int i = 0; i < saveData.audioTracks.size(); ++i) {
    TakeTrack &takes = saveData.audioTracks[i].takes;
    for (const Take &take : QList<Take>(takes.takes())) {
      const QString relative = QStringLiteral("%1/track_%2_take_%3.wav")
                                   .arg(audioDirName)
                                   .arg(i + 1)
                                   .arg(take.id);
      media.append(ProjectMedia{take.file, relative});
      takes.setTakeMedia(take.id, relative, 0);
    }
  }
  return media;
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  centralWidget->setObjectName("CentralWidget");
  centralWidget->setAttribute(Qt::WA_StyledBackground, true);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // =========================================================================
  // Video Area with Overlay
  // =========================================================================

  m_videoFrame = new QFrame(this);
  m_videoFrame->setObjectName("videoFrame");
  m_videoFrame->setFrameStyle(QFrame::NoFrame);
  m_videoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_videoFrame->setMinimumHeight(100);

  m_videoWidget = new VideoWidget(m_videoFrame);
  m_videoWidget->show();

  m_rythmoOverlay = new RythmoOverlay(m_videoFrame);
  m_rythmoOverlay->show();

  // Time runs left to right in every language, Arabic included: set on the
  // widgets themselves, which the fullscreen recording reparents
  m_videoWidget->setLayoutDirection(Qt::LeftToRight);
  m_rythmoOverlay->setLayoutDirection(Qt::LeftToRight);

  // Takes timeline under the video, resizable against it
  QScrollArea *timelineScroll = new QScrollArea(this);
  timelineScroll->setObjectName("takeTimelineScroll");
  timelineScroll->setWidgetResizable(true);
  timelineScroll->setFrameShape(QFrame::NoFrame);
  timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_takeTimeline = new TakeTimeline(timelineScroll);
  m_takeTimeline->setLayoutDirection(Qt::LeftToRight);
  timelineScroll->setWidget(m_takeTimeline);

  m_videoSplitter = new QSplitter(Qt::Vertical, this);
  m_videoSplitter->setObjectName("videoSplitter");
  m_videoSplitter->setChildrenCollapsible(false);
  m_videoSplitter->addWidget(m_videoFrame);
  m_videoSplitter->addWidget(timelineScroll);
  m_videoSplitter->setStretchFactor(0, 1);
  m_videoSplitter->setStretchFactor(1, 0);
  m_videoSplitter->restoreState(QSettings().value("ui/video_splitter").toByteArray());
  connect(m_videoSplitter, &QSplitter::splitterMoved, this, [this]() {
    QSettings().setValue("ui/video_splitter", m_videoSplitter->saveState());
  });
  connect(m_takeTimeline, &TakeTimeline::contentHeightChanged, this,
          &MainWindow::fitTimeline);

  mainLayout->addWidget(m_videoSplitter, 1);

  // Watch for resize events
  m_videoFrame->installEventFilter(this);

  // Fullscreen container (hidden until recording starts)
  m_fullscreenContainer =
      new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
  m_fullscreenContainer->setObjectName("fullscreenContainer");
  m_fullscreenContainer->setStyleSheet("background-color: black;");
  m_fullscreenContainer->hide();

  // =========================================================================
  // Control Bar
  // =========================================================================

  QWidget *controlBarHost = new QWidget(this);
  QHBoxLayout *controlBarHostLayout = new QHBoxLayout(controlBarHost);
  controlBarHostLayout->setContentsMargins(12, 8, 12, 0);
  controlBarHostLayout->setSpacing(0);

  QWidget *controlBar = new QWidget(controlBarHost);
  controlBar->setObjectName("controlBar");
  controlBar->setMinimumHeight(56);
  controlBar->setAttribute(Qt::WA_StyledBackground, true);
  controlBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  // Transport reads like a player: back on the left, forward on the right
  controlBar->setLayoutDirection(Qt::LeftToRight);

  auto *controlShadow = new QGraphicsDropShadowEffect(controlBar);
  controlShadow->setBlurRadius(20);
  controlShadow->setOffset(0, 4);
  controlShadow->setColor(QColor(20, 20, 20, 28));
  controlBar->setGraphicsEffect(controlShadow);
  controlBarHostLayout->addWidget(controlBar);

  QHBoxLayout *controlBarLayout = new QHBoxLayout(controlBar);
  controlBarLayout->setContentsMargins(18, 8, 18, 8);
  controlBarLayout->setSpacing(12);

  m_stepBackButton = new QPushButton(QIcon(":/resources/icons/arrow_left.svg"), "", controlBar);
  m_stepBackButton->setObjectName("btnStepBack");
  m_stepBackButton->setProperty("cssClass", "iconButton");
  m_stepBackButton->setToolTip(tr("Back (1 frame)"));
  m_stepBackButton->setMinimumSize(32, 32);

  m_playPauseButton = new QPushButton(QIcon(":/resources/icons/play.svg"), " " + tr("PLAY"), controlBar);
  m_playPauseButton->setObjectName("btnPlayPause");
  m_playPauseButton->setProperty("class", "btn");

  m_stopButton = new QPushButton("⏹ " + tr("STOP"), controlBar);
  m_stopButton->setObjectName("btnStop");
  m_stopButton->setProperty("class", "btn");

  m_stepForwardButton = new QPushButton(QIcon(":/resources/icons/arrow_right.svg"), "", controlBar);
  m_stepForwardButton->setObjectName("btnStepForward");
  m_stepForwardButton->setProperty("cssClass", "iconButton");
  m_stepForwardButton->setToolTip(tr("Forward (1 frame)"));
  m_stepForwardButton->setMinimumSize(32, 32);

  m_timeLabel = new QLabel("00:00 / 00:00", controlBar);
  m_timeLabel->setObjectName("timecodeLabel");

  QHBoxLayout *group1Layout = new QHBoxLayout();
  group1Layout->setSpacing(8);
  group1Layout->addWidget(m_stepBackButton);
  group1Layout->addWidget(m_playPauseButton);
  group1Layout->addWidget(m_stopButton);
  group1Layout->addWidget(m_stepForwardButton);
  group1Layout->addWidget(m_timeLabel);

  controlBarLayout->addLayout(group1Layout);
  controlBarLayout->addStretch();

  // Group 2: Speed and Recording
  QHBoxLayout *group2Layout = new QHBoxLayout();
  group2Layout->setContentsMargins(0, 0, 0, 0);
  group2Layout->setSpacing(8);

  QLabel *speedLabel = new QLabel(tr("Scroll Speed"), controlBar);
  speedLabel->setProperty("cssClass", "control-label");
  group2Layout->addWidget(speedLabel);

  m_speedDownButton = new QPushButton("−", controlBar);
  m_speedDownButton->setObjectName("speedDownButton");
  m_speedDownButton->setProperty("cssClass", "stepButton");
  m_speedDownButton->setToolTip(tr("Slower (-10%)"));
  m_speedDownButton->setMinimumSize(28, 28);
  group2Layout->addWidget(m_speedDownButton);

  m_speedSpinBox = new QSpinBox(controlBar);
  m_speedSpinBox->setObjectName("speedSpinBox");
  m_speedSpinBox->setRange(1, 400);
  m_speedSpinBox->setValue(100);
  m_speedSpinBox->setSuffix("%");
  m_speedSpinBox->setFixedWidth(82);
  m_speedSpinBox->setAlignment(Qt::AlignRight);
  m_speedSpinBox->setSingleStep(10);
  group2Layout->addWidget(m_speedSpinBox);

  m_speedUpButton = new QPushButton("+", controlBar);
  m_speedUpButton->setObjectName("speedUpButton");
  m_speedUpButton->setProperty("cssClass", "stepButton");
  m_speedUpButton->setToolTip(tr("Faster (+10%)"));
  m_speedUpButton->setMinimumSize(28, 28);
  group2Layout->addWidget(m_speedUpButton);


  m_recordButton = new QPushButton(recordIdleText(), controlBar);
  m_recordButton->setObjectName("recordButton");
  m_recordButton->setCheckable(true);
  m_recordButton->setMinimumHeight(30);
  m_recordButton->setMinimumWidth(132);
  m_recordButton->setCursor(Qt::PointingHandCursor);
  group2Layout->addWidget(m_recordButton);

  m_recordDurationLabel = new QLabel("00:00", controlBar);
  m_recordDurationLabel->setObjectName("recordDurationLabel");
  m_recordDurationLabel->setVisible(false);
  group2Layout->addWidget(m_recordDurationLabel);

  m_recordDurationTimer = new QTimer(this);
  connect(m_recordDurationTimer, &QTimer::timeout, this, [this]() {
      if (m_isRecording) {
          qint64 elapsed = m_recordingTimer.elapsed();
          m_recordDurationLabel->setText(TimeFormatter::format(elapsed));
      }
  });
  controlBarLayout->addLayout(group2Layout);
  controlBarLayout->addStretch();

  // Group 3: Volume Controls
  QHBoxLayout *group3Layout = new QHBoxLayout();
  group3Layout->setContentsMargins(0, 0, 0, 0);
  group3Layout->setSpacing(8);

  QLabel *masterLabel = new QLabel(tr("Master Vol"), controlBar);
  masterLabel->setProperty("cssClass", "control-label");
  group3Layout->addWidget(masterLabel);

  m_volumeMuteButton =
      new QPushButton(QIcon(":/resources/icons/volume.svg"), "", controlBar);
  m_volumeMuteButton->setObjectName("volumeMuteButton");
  m_volumeMuteButton->setProperty("cssClass", "iconButton");
  m_volumeMuteButton->setCheckable(true);
  m_volumeMuteButton->setToolTip(tr("Mute / Unmute"));
  m_volumeMuteButton->setMinimumSize(30, 30);
  group3Layout->addWidget(m_volumeMuteButton);

  m_volumeDownButton = new QPushButton("−", controlBar);
  m_volumeDownButton->setObjectName("volumeDownButton");
  m_volumeDownButton->setProperty("cssClass", "stepButton");
  m_volumeDownButton->setToolTip(tr("Volume down (-5%)"));
  m_volumeDownButton->setMinimumSize(28, 28);
  group3Layout->addWidget(m_volumeDownButton);

  m_volumeSlider = new ClickableSlider(Qt::Horizontal, controlBar);
  m_volumeSlider->setObjectName("masterVolumeSlider");
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  m_volumeSlider->setFixedWidth(112);
  group3Layout->addWidget(m_volumeSlider);

  m_volumeUpButton = new QPushButton("+", controlBar);
  m_volumeUpButton->setObjectName("volumeUpButton");
  m_volumeUpButton->setProperty("cssClass", "stepButton");
  m_volumeUpButton->setToolTip(tr("Volume up (+5%)"));
  m_volumeUpButton->setMinimumSize(28, 28);
  group3Layout->addWidget(m_volumeUpButton);

  m_volumeSpinBox = new QSpinBox(controlBar);
  m_volumeSpinBox->setObjectName("volumeSpinBox");
  m_volumeSpinBox->setRange(0, 100);
  m_volumeSpinBox->setValue(100);
  m_volumeSpinBox->setFixedWidth(86);
  m_volumeSpinBox->setAlignment(Qt::AlignRight);
  m_volumeSpinBox->setSuffix("%");
  group3Layout->addWidget(m_volumeSpinBox);

  m_exportProgressBar = new QProgressBar(controlBar);
  m_exportProgressBar->setVisible(false);
  m_exportProgressBar->setFixedWidth(132);
  group3Layout->addWidget(m_exportProgressBar);

  m_exportCancelBtn = new QPushButton(tr("Cancel"), controlBar);
  m_exportCancelBtn->setVisible(false);
  group3Layout->addWidget(m_exportCancelBtn);

  controlBarLayout->addLayout(group3Layout);

  // =========================================================================
  // Post-Record Notification Bar
  // =========================================================================

  m_postRecordBar = new QWidget(centralWidget);
  m_postRecordBar->setObjectName("postRecordBar");
  m_postRecordBar->setFixedHeight(34);
  m_postRecordBar->setVisible(false);

  QHBoxLayout *prLayout = new QHBoxLayout(m_postRecordBar);
  prLayout->setContentsMargins(12, 3, 12, 3);

  m_postRecordLabel = new QLabel(tr("✅ Recording finished!"), m_postRecordBar);
  m_postRecordLabel->setProperty("cssClass", "settingsLabel");
  prLayout->addWidget(m_postRecordLabel);
  prLayout->addStretch();

  QPushButton *listenBtn = new QPushButton(tr("▶ Listen"), m_postRecordBar);
  listenBtn->setProperty("cssClass", "presetButton");
  connect(listenBtn, &QPushButton::clicked, this, [this]() {
      m_playbackEngine->seek(m_lastRecordedStartMs);
      m_playbackEngine->play();
      hidePostRecordBar();
  });
  prLayout->addWidget(listenBtn);

  QPushButton *exportBtn = new QPushButton(tr("📤 Export"), m_postRecordBar);
  exportBtn->setProperty("cssClass", "presetButton");
  connect(exportBtn, &QPushButton::clicked, this, [this]() {
      hidePostRecordBar();
      showExportDialog();
  });
  prLayout->addWidget(exportBtn);

  QPushButton *closeBtn = new QPushButton(tr("✕"), m_postRecordBar);
  closeBtn->setProperty("cssClass", "iconButton");
  closeBtn->setFixedSize(24, 24);
  connect(closeBtn, &QPushButton::clicked, this, &MainWindow::hidePostRecordBar);
  prLayout->addWidget(closeBtn);

  // Above the transport: showing it shrinks the video, the controls stay put
  mainLayout->addWidget(m_postRecordBar);
  mainLayout->addWidget(controlBarHost);

  // =========================================================================
  // Mixer Zone
  // =========================================================================

  QWidget *mixerHost = new QWidget(this);
  QHBoxLayout *mixerHostLayout = new QHBoxLayout(mixerHost);
  mixerHostLayout->setContentsMargins(12, 8, 12, 8);
  mixerHostLayout->setSpacing(0);

  // Height follows the track panels
  QWidget *mixerZone = new QWidget(mixerHost);
  mixerZone->setObjectName("mixerZone");
  mixerZone->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  mixerZone->setAttribute(Qt::WA_StyledBackground, true);
  m_tracksLayout = new QHBoxLayout(mixerZone);
  m_tracksLayout->setContentsMargins(10, 10, 10, 10);
  m_tracksLayout->setSpacing(10);

  mixerHostLayout->addWidget(mixerZone);

  mainLayout->addWidget(mixerHost);

  // Created now so its height is reserved, not taken from the layout at the first message
  statusBar();

  // Initial sync
  m_rythmoOverlay->setSpeed(m_speedSpinBox->value());
  m_rythmoManager->setSpeed(m_speedSpinBox->value());
}

void MainWindow::fitTimeline() {
  const QList<int> sizes = m_videoSplitter->sizes();
  const int total = sizes[0] + sizes[1];
  if (total <= 0)
    return;
  const int wanted = qMin(m_takeTimeline->sizeHint().height(), total * 65 / 100);
  if (sizes[1] < wanted)
    m_videoSplitter->setSizes({total - wanted, wanted});
}

void MainWindow::createMenus() {
  // Use QMainWindow's built-in menuBar() to ensure the native macOS menu bar
  // is used. Creating a `new QMenuBar(this)` can bypass native integration.
  QMenuBar *mb = menuBar();

  // === Files Menu ===
  QMenu *filesMenu = mb->addMenu(tr("File"));

  m_actionOpenMp4 = new QAction(tr("Open a video"), this);
  connect(m_actionOpenMp4, &QAction::triggered, this, &MainWindow::onOpenFile);
  filesMenu->addAction(m_actionOpenMp4);

  m_actionLoadProject = new QAction(tr("Open a project"), this);
  connect(m_actionLoadProject, &QAction::triggered, this,
          &MainWindow::onLoadProject);
  filesMenu->addAction(m_actionLoadProject);

  m_actionSaveProject = new QAction(tr("Save .dbi / .zip"), this);
  connect(m_actionSaveProject, &QAction::triggered, this,
          &MainWindow::onSaveProject);
  filesMenu->addAction(m_actionSaveProject);

  m_actionManualExport = new QAction(tr("Export the dub..."), this);
  connect(m_actionManualExport, &QAction::triggered, this,
          &MainWindow::showExportDialog);
  filesMenu->addAction(m_actionManualExport);

  for (QAction *action : {m_actionOpenMp4, m_actionLoadProject, m_actionManualExport})
    action->setAutoRepeat(false);

  // === Edit Menu ===
  QMenu *editMenu = mb->addMenu(tr("Edit"));
  m_undoStack = new QUndoStack(this);

  m_actionUndo = new QAction(tr("Undo"), this);
  connect(m_actionUndo, &QAction::triggered, m_undoStack, &QUndoStack::undo);
  editMenu->addAction(m_actionUndo);

  m_actionRedo = new QAction(tr("Redo"), this);
  connect(m_actionRedo, &QAction::triggered, m_undoStack, &QUndoStack::redo);
  editMenu->addAction(m_actionRedo);

  connect(m_undoStack, &QUndoStack::undoTextChanged, this, [this](const QString &text) {
    m_actionUndo->setText(text.isEmpty() ? tr("Undo") : tr("Undo: %1").arg(text));
  });
  connect(m_undoStack, &QUndoStack::redoTextChanged, this, [this](const QString &text) {
    m_actionRedo->setText(text.isEmpty() ? tr("Redo") : tr("Redo: %1").arg(text));
  });
  connect(m_undoStack, &QUndoStack::indexChanged, this, &MainWindow::updateEditActions);
  updateEditActions();

  // === Application Menu ===
  QMenu *appMenu = mb->addMenu(tr("Application"));

  m_actionFullscreen = new QAction(tr("Fullscreen while recording"), this);
  m_actionFullscreen->setCheckable(true);
  appMenu->addAction(m_actionFullscreen);

  m_actionWindowFullScreen = new QAction(tr("Fullscreen (window)"), this);
  m_actionWindowFullScreen->setAutoRepeat(false);
  connect(m_actionWindowFullScreen, &QAction::triggered, this,
          &MainWindow::toggleWindowFullScreen);
  appMenu->addAction(m_actionWindowFullScreen);



  m_actionGlobalSettings = new QAction(tr("Global settings"), this);
  appMenu->addAction(m_actionGlobalSettings);

  // === Bande Rythmo Menu ===
  //: The scrolling dialogue band of French-style dubbing ("bande rythmo")
  QMenu *rythmoMenu = mb->addMenu(tr("Rythmo Band"));

  // Track count selector using plain QActions (QWidgetAction with embedded
  // widgets doesn't work on macOS native menu bars).
  QAction *actionRemoveTrack = new QAction(tr("Remove a band (−)"), this);
  connect(actionRemoveTrack, &QAction::triggered, this,
          [this]() { changeTrackCount(m_trackCount - 1); });
  rythmoMenu->addAction(actionRemoveTrack);

  QAction *actionAddTrack = new QAction(tr("Add a band (+)"), this);
  connect(actionAddTrack, &QAction::triggered, this,
          [this]() { changeTrackCount(m_trackCount + 1); });
  rythmoMenu->addAction(actionAddTrack);

  rythmoMenu->addSeparator();

  m_actionPersonalizeRythmo = new QAction(tr("Customize"), this);
  rythmoMenu->addAction(m_actionPersonalizeRythmo);

  // === Audio Menu ===
  m_audioMenu = mb->addMenu(tr("Audio"));
  updateAudioMenu();

}

void MainWindow::setupConnections() {
  // =========================================================================
  // Playback Controls
  // =========================================================================

  connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);

  connect(m_stepBackButton, &QPushButton::clicked, this, [this]() {
    m_playbackEngine->seek(qMax(0LL, m_playbackEngine->position() - frameStepMs()));
  });

  connect(m_stepForwardButton, &QPushButton::clicked, this, [this]() {
    m_playbackEngine->seek(qMin(m_playbackEngine->duration(), m_playbackEngine->position() + frameStepMs()));
  });

  connect(m_stopButton, &QPushButton::clicked, this, [this]() {
    m_playbackEngine->stop();
    if (m_isRecording) {
      toggleRecording();
    }
  });

  // =========================================================================
  // PlaybackEngine -> UI
  // =========================================================================

  connect(m_playbackEngine, &PlaybackEngine::positionChanged, this,
          &MainWindow::onPositionChanged);
  connect(m_playbackEngine, &PlaybackEngine::durationChanged, this,
          &MainWindow::onDurationChanged);
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged, this,
          &MainWindow::onPlaybackStateChanged);
  connect(m_playbackEngine, &PlaybackEngine::errorOccurred, this,
          &MainWindow::onError);
  connect(m_playbackEngine, &PlaybackEngine::frameExtracted, m_videoWidget,
          &VideoWidget::forceFrame);

  // Takes follow the video
  connect(m_playbackEngine, &PlaybackEngine::positionChanged, this,
          &MainWindow::syncTrackPlayers);
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged, this,
          &MainWindow::syncTrackPlayers);

  // PlaybackEngine -> RythmoOverlay
  connect(m_playbackEngine, &PlaybackEngine::positionChanged, m_rythmoOverlay,
          &RythmoOverlay::sync);
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged, this,
          [this](QMediaPlayer::PlaybackState state) {
            m_rythmoOverlay->setPlaying(state == QMediaPlayer::PlayingState);
          });

  // =========================================================================
  // Takes Timeline
  // =========================================================================

  connect(m_takeTimeline, &TakeTimeline::seekRequested, m_playbackEngine,
          &PlaybackEngine::seek);
  connect(m_takeTimeline, &TakeTimeline::playRequested, m_playbackEngine,
          &PlaybackEngine::play);
  connect(m_takeTimeline, &TakeTimeline::trackEdited, this,
          [this](int index, const TakeTrack &takes) {
            //: Undo step: choosing which recorded take is heard where
            editTakes(index, takes, tr("Take editing"));
          });
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged, this,
          [this](QMediaPlayer::PlaybackState state) {
            m_takeTimeline->setPlaying(state == QMediaPlayer::PlayingState);
          });

  // =========================================================================
  // Volume Controls
  // =========================================================================

  connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
    m_playbackEngine->setVolume(static_cast<float>(value) / 100.0f);
    if (m_volumeSpinBox->value() != value) {
      m_volumeSpinBox->blockSignals(true);
      m_volumeSpinBox->setValue(value);
      m_volumeSpinBox->blockSignals(false);
    }
    bool isMuted = (value == 0);
    if (m_volumeMuteButton->isChecked() != isMuted) {
      m_volumeMuteButton->blockSignals(true);
      m_volumeMuteButton->setChecked(isMuted);
      m_volumeMuteButton->blockSignals(false);
    }
    if (value > 0) {
      m_previousVolume = value;
    }
    updateVolumeIcon(value);
  });

  connect(m_volumeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            m_playbackEngine->setVolume(static_cast<float>(value) / 100.0f);
            if (m_volumeSlider->value() != value) {
              m_volumeSlider->blockSignals(true);
              m_volumeSlider->setValue(value);
              m_volumeSlider->blockSignals(false);
            }
            updateVolumeIcon(value);
          });

  connect(m_volumeDownButton, &QPushButton::clicked, this, [this]() {
    m_volumeSlider->setValue(qMax(0, m_volumeSlider->value() - 5));
  });

  connect(m_volumeUpButton, &QPushButton::clicked, this, [this]() {
    m_volumeSlider->setValue(qMin(100, m_volumeSlider->value() + 5));
  });

  connect(m_volumeMuteButton, &QPushButton::clicked, this, [this]() {
    if (m_volumeSlider->value() > 0) {
      m_previousVolume = m_volumeSlider->value();
      m_volumeSlider->setValue(0);
      return;
    }

    int restored = qBound(1, m_previousVolume, 100);
    m_volumeSlider->setValue(restored);
  });

  connect(m_playbackEngine, &PlaybackEngine::volumeChanged, this,
          [this](float volume) {
            int val = static_cast<int>(volume * 100);
            if (m_volumeSlider->value() != val) {
              m_volumeSlider->blockSignals(true);
              m_volumeSlider->setValue(val);
              m_volumeSlider->blockSignals(false);
            }
            if (m_volumeSpinBox->value() != val) {
              m_volumeSpinBox->blockSignals(true);
              m_volumeSpinBox->setValue(val);
              m_volumeSpinBox->blockSignals(false);
            }
          });

  // =========================================================================
  // Speed & Display Settings
  // =========================================================================

  connect(m_speedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
          m_rythmoOverlay, &RythmoOverlay::setSpeed);
  connect(m_speedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
          m_rythmoManager, &RythmoManager::setSpeed);
  connect(m_speedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this]() { setDirty(true); });

    connect(m_speedDownButton, &QPushButton::clicked, this, [this]() {
      m_speedSpinBox->setValue(qMax(m_speedSpinBox->minimum(),
            m_speedSpinBox->value() - 10));
    });

    connect(m_speedUpButton, &QPushButton::clicked, this, [this]() {
      m_speedSpinBox->setValue(qMin(m_speedSpinBox->maximum(),
            m_speedSpinBox->value() + 10));
    });


  connect(m_actionPersonalizeRythmo, &QAction::triggered, this, [this]() {
    TrackSettingsDialog *dialog =
        new TrackSettingsDialog(m_rythmoManager, m_trackCount, 0, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
  });

  // =========================================================================
  // Recording
  // =========================================================================

  // Update overlay styles when manager styles change
  connect(m_rythmoManager, &RythmoManager::trackStyleChanged, this,
          [this](int trackIndex, const RythmoTrackStyle &style) {
            RythmoWidget *w = m_rythmoOverlay->track(trackIndex);
            if (w) {
              w->setTrackStyle(style);
            }
            setDirty(true);
          });

  // Recording
  connect(m_recordButton, &QPushButton::clicked, this,
          &MainWindow::toggleRecording);

  // =========================================================================
  // Export
  // =========================================================================

  connect(m_exportService, &ExportService::progressChanged, this,
          &MainWindow::onExportProgress);
  connect(m_exportService, &ExportService::exportFinished, this,
          &MainWindow::onExportFinished);
  connect(m_exportCancelBtn, &QPushButton::clicked, m_exportService,
          &ExportService::cancelExport);

  connect(m_actionGlobalSettings, &QAction::triggered, this,
          &MainWindow::onOpenGlobalSettings);
  connect(m_outputDevicesGroup, &QActionGroup::triggered, this,
          &MainWindow::onOutputDeviceTriggered);
}

// =============================================================================
// Dynamic Track Management
// =============================================================================

void MainWindow::setTrackCount(int count) {
  count = qBound(1, count, MAX_TRACKS);
  if (count == m_trackCount)
    return;

  // Add tracks if needed
  while (m_trackCount < count) {
    int idx = m_trackCount;

    // Create AudioRecorder
    AudioRecorder *recorder = new AudioRecorder(this);
    m_audioRecorders.append(recorder);
    connect(recorder, &AudioRecorder::errorOccurred, this,
            &MainWindow::onError);
    // Validate the take and reload previews only once the WAV is finalized
    // (stop() is asynchronous)
    connect(recorder, &AudioRecorder::recorderStateChanged, this,
            [this, idx](QMediaRecorder::RecorderState state) {
              if (state != QMediaRecorder::StoppedState) {
                return;
              }
              checkRecordedTake(idx);
            });

    // Create TrackWidget
    TrackWidget *panel = new TrackWidget(
        idx + 1, tr("Track %1").arg(idx + 1), this);
    m_trackPanels.append(panel);
    m_tracksLayout->addWidget(panel);

    m_takeTracks.append(TakeTrack());
    m_trackPlayers.append(new TrackPlayer(m_playbackEngine->audioDevice(), this));

    connect(panel, &TrackWidget::volumeChanged, this, [this, idx](int vol) {
        if (idx < m_trackPlayers.size()) {
            m_trackPlayers[idx]->setVolume(static_cast<float>(vol) / 100.0f);
        }
        setDirty(true);
    });

    // Populate combo with real system audio devices
    QList<QAudioDevice> devices = recorder->availableDevices();
    panel->populateInputDevices(devices);

    // Apply persistent/global microphone coupling
    QString cachedMic = SettingsManager::instance().trackMicrophone(idx);
    if (!cachedMic.isEmpty()) {
      panel->setInputDevice(cachedMic);
    } else {
      QString defaultGlobalMic = SettingsManager::instance().defaultMicrophone();
      if (!defaultGlobalMic.isEmpty()) {
        panel->setInputDevice(defaultGlobalMic);
      }
    }

    // Wire real-time audio level monitoring → VU meter
    connect(recorder, &AudioRecorder::levelChanged, panel,
            [panel](float level) {
                panel->setVuLevel(static_cast<int>(level * 100.0f));
            });

    // Wire device selection: when user picks a device in the combo, switch recorder + monitoring
    connect(panel, &TrackWidget::inputDeviceIndexChanged, this,
            [this, idx](int deviceIndex) {
                if (idx >= m_audioRecorders.size()) return;
                setDirty(true);
                AudioRecorder *rec = m_audioRecorders[idx];
                QList<QAudioDevice> devs = rec->availableDevices();
                
                if (deviceIndex < 0 || deviceIndex >= devs.size()) {
                    // "No input" selected → stop monitoring
                    rec->stopMonitoring();
                    SettingsManager::instance().setTrackMicrophone(idx, "");
                    return;
                }
                
                QAudioDevice selectedDev = devs[deviceIndex];
                rec->setDevice(selectedDev);
                // setDevice already restarts monitoring if it was active,
                // but start it if it wasn't
                rec->startMonitoring();

                SettingsManager::instance().setTrackMicrophone(idx, selectedDev.description());
            });

    // Gear button → open settings dialog with this track selected
    connect(panel, &TrackWidget::optionsClicked, this, [this, idx]() {
      TrackSettingsDialog *dialog =
          new TrackSettingsDialog(m_rythmoManager, m_trackCount, idx, this);
      dialog->setAttribute(Qt::WA_DeleteOnClose);
      dialog->show();
    });

    // Initialize RythmoManager text for this track
    m_rythmoManager->setText(idx, "");

    m_trackCount++;
  }

  // Remove tracks if needed
  while (m_trackCount > count) {
    // Remove TrackWidget
    TrackWidget *panel = m_trackPanels.takeLast();
    m_tracksLayout->removeWidget(panel);
    panel->deleteLater();

    // Remove AudioRecorder
    AudioRecorder *recorder = m_audioRecorders.takeLast();
    recorder->stopMonitoring();
    recorder->deleteLater();

    // Its takes stay in the session dir until the project is closed
    m_takeTracks.removeLast();
    m_recordingTakes.remove(m_trackCount - 1);
    TrackPlayer *player = m_trackPlayers.takeLast();
    player->releaseFiles();
    player->deleteLater();

    m_trackCount--;
  }

  // Update RythmoOverlay
  m_rythmoOverlay->setTrackCount(count);
  m_takeTimeline->setTrackCount(count);

  // Connect signals for all current tracks
  // (reconnecting is safe because we use lambdas with captured index)
  for (int i = 0; i < m_trackCount; ++i) {
    connectTrack(i);
  }

  setDirty(true);
}

void MainWindow::connectTrack(int index) {
  RythmoWidget *widget = m_rythmoOverlay->track(index);
  if (!widget)
    return;

  // Disconnect any existing connections on this widget to prevent duplicates
  disconnect(widget, nullptr, this, nullptr);
  disconnect(widget, nullptr, m_playbackEngine, nullptr);

  // Seek and play
  connect(widget, &RythmoWidget::seekRequested, m_playbackEngine,
          &PlaybackEngine::seek);
  connect(widget, &RythmoWidget::playRequested, m_playbackEngine,
          &PlaybackEngine::play);

  // Text changed: RythmoWidget -> RythmoManager
  connect(widget, &RythmoWidget::textChanged, this,
          [this, index](const QString &text) {
            const QString before = m_rythmoManager->text(index);
            if (before == text)
              return;
            m_undoStack->push(new TypingCommand(
                index, before, text,
                [this](int track, const QString &value) { applyRythmoText(track, value); }));
          });
}

qint64 MainWindow::frameStepMs() const {
  const qreal fps = m_playbackEngine ? m_playbackEngine->videoFrameRate() : 0.0;
  if (fps <= 0.0)
    return 40; // aucune métadonnée : 25 fps par défaut
  return qMax(1LL, static_cast<qint64>(qRound(1000.0 / fps)));
}

// =============================================================================
// Slots - File Operations
// =============================================================================

void MainWindow::onOpenFile() {
  if (!maybeSaveChanges())
    return;
  openVideoDialog();
}

// Sans garde de sauvegarde : appelée aussi par loadProjectFrom() pour relier une
// vidéo introuvable, au milieu d'un chargement déjà engagé.
void MainWindow::openVideoDialog() {
  QStringList globs;
  for (const QString &suffix : videoSuffixes())
    globs << QLatin1String("*.") + suffix;

  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Open"), "",
      tr("Video files (%1);;All files (*)")
          .arg(globs.join(QLatin1Char(' '))));

  if (!fileName.isEmpty()) {
    m_playbackEngine->openFile(QUrl::fromLocalFile(fileName));
    m_currentVideoPath = fileName;
    // La dernière prise appartenait à la vidéo précédente : l'export ne doit
    // plus proposer sa plage.
    m_lastRecordedDurationMs = 0;
    m_lastRecordedStartMs = 0;
    setDirty(true);
  }
}

SaveData MainWindow::collectSaveData() {
  SaveData saveData;
  saveData.videoUrl = m_currentVideoPath;
  saveData.videoVolume = m_playbackEngine->volume();
  saveData.trackCount = m_trackCount;
  saveData.scrollSpeed = m_speedSpinBox->value();
  saveData.nextTakeId = m_nextTakeId;

  for (int i = 0; i < m_trackCount; ++i) {
    TrackAudioSaveData audioData;
    audioData.audioInput = m_trackPanels[i]->currentInputDevice();
    audioData.audioGain = m_trackPanels[i]->currentVolume() / 100.0f;
    audioData.takes = m_takeTracks.value(i);
    saveData.audioTracks.append(audioData);
  }

  for (int i = 0; i < m_trackCount; ++i) {
    TrackSaveData trackData;
    trackData.text = m_rythmoManager->text(i);
    trackData.style = m_rythmoManager->trackStyle(i);
    if (const RythmoWidget *w = m_rythmoOverlay->track(i))
      trackData.charMs = w->charMs();
    saveData.tracks.append(trackData);
  }

  return saveData;
}

void MainWindow::onSaveProject() {
  if (deferSaveDuringTake(PendingSave::SaveAs))
    return;

  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(
      this, tr("Save"),
      tr("Include the video in the archive?\n(This creates a .zip file)"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if (reply == QMessageBox::Cancel)
    return;

  bool saveWithVideo = (reply == QMessageBox::Yes);
  QString filter = saveWithVideo ? tr("DubInstante Archive (*.zip)")
                                 : tr("DubInstante Project (*.dbi)");
  QString suffix = saveWithVideo ? ".zip" : ".dbi";

  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save the project"), "", filter);

  if (fileName.isEmpty())
    return;

  if (!fileName.endsWith(suffix, Qt::CaseInsensitive)) {
    fileName += suffix;
  }

  saveProjectTo(fileName, saveWithVideo);
}

void MainWindow::onQuickSaveProject() {
  if (deferSaveDuringTake(PendingSave::Save))
    return;

  if (m_currentProjectPath.isEmpty()) {
    onSaveProject();
    return;
  }
  // A project opened from a .zip is written back as a .zip, never as a bare .dbi
  saveProjectTo(m_currentProjectPath,
                m_currentProjectPath.endsWith(".zip", Qt::CaseInsensitive));
}

bool MainWindow::deferSaveDuringTake(PendingSave save) {
  if (m_countdownTimer->isActive()) {
    // Nothing recorded yet: cancel the pre-roll rather than let it start the
    // take underneath the save dialogs.
    toggleRecording();
  }

  // Until checkRecordedTake() validates them, armed tracks are flagged without
  // audio: saving now would persist them empty and open modal dialogs over the
  // take. The save runs once every take is finalized.
  // ponytail: relies on each recorder reaching StoppedState, like the
  // post-record bar does; add a timeout if a recorder is ever seen hanging.
  if (!m_isRecording && m_pendingTakeChecks.isEmpty())
    return false;

  m_pendingSave = save;
  statusBar()->showMessage(
      tr("The project will be saved when the take ends."), 5000);
  return true;
}

void MainWindow::saveProjectTo(const QString &fileName, bool saveWithVideo) {
  SaveData saveData = collectSaveData();

  // Takes are written next to the project, in <name>_audio/
  QFileInfo fi(fileName);
  QDir dir = fi.absoluteDir();
  const QString audioDirName = fi.completeBaseName() + "_audio";
  const QList<ProjectMedia> media = attachProjectMedia(saveData, audioDirName);

  if (!saveWithVideo) {
    for (const ProjectMedia &file : media) {
      const QString destPath = dir.absoluteFilePath(file.relativePath);
      QFile::remove(destPath);
      // Un échec doit faire échouer la sauvegarde : le projet resterait sinon
      // marqué enregistré et closeEvent détruirait le WAV de session, seule
      // copie de la prise.
      // ponytail: every take is copied on every save; skip unchanged files if
      // saving a heavily comped project gets slow.
      if (!dir.mkpath(audioDirName) || !QFile::copy(file.sourcePath, destPath)) {
        QMessageBox::critical(
            this, tr("Error"),
            tr("Could not copy recording %1 to:\n"
               "%2\n\n"
               "Check the disk space or the permissions. The project was not saved.")
                .arg(QFileInfo(file.relativePath).fileName())
                .arg(QDir::toNativeSeparators(destPath)));
        return;
      }
    }
  }

  if (saveWithVideo) {
    // Check zip availability BEFORE launching thread (for specific error
    // messages)
    QString zipError;
    if (!SaveManager::isZipAvailable(&zipError)) {
      QMessageBox::critical(this, tr("Save error"), zipError);
      return;
    }

    // Show progress dialog
    QProgressDialog *progressDialog = new QProgressDialog(this);
    progressDialog->setLabelText(
        tr("Creating the ZIP archive...\n"
           "This can take a few minutes depending on the video size."));
    progressDialog->setRange(0, 0); // Indeterminate
    progressDialog->setCancelButton(
        nullptr); // Disable cancel for safety during zip
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    // Run in background thread
    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    auto errorMessage = std::make_shared<QString>();
    connect(watcher, &QFutureWatcher<bool>::finished, this,
            [this, watcher, progressDialog, fileName, errorMessage]() {
              bool result = watcher->result();
              progressDialog->close();
              progressDialog->deleteLater();
              watcher->deleteLater();

              if (result) {
                discardAutosave();
                m_currentProjectPath = fileName;
                setDirty(false);
                updateWindowTitle();
                statusBar()->showMessage(tr("Project saved"), 3000);
              } else {
                QMessageBox::critical(
                    this, tr("Error"),
                    errorMessage->isEmpty()
                        ? tr("Could not create the ZIP archive.\n"
                             "Check the disk space or the permissions.")
                        : *errorMessage);
              }
            });

    // Copies: the worker thread must not read members owned by the GUI thread
    QFuture<bool> future = QtConcurrent::run([this, fileName, saveData, media, errorMessage]() {
      return m_saveManager->saveWithMedia(fileName, saveData, media, errorMessage.get());
    });
    watcher->setFuture(future);

  } else {
    if (m_saveManager->save(fileName, saveData)) {
      // Takes deleted since the last save, and the track_N.wav of 0.12 projects
      QSet<QString> kept;
      for (const ProjectMedia &file : media)
        kept.insert(QFileInfo(file.relativePath).fileName());
      QDir audioDir(dir.absoluteFilePath(audioDirName));
      for (const QString &name : audioDir.entryList({"track_*.wav"}, QDir::Files)) {
        if (!kept.contains(name))
          audioDir.remove(name);
      }
      discardAutosave();
      m_currentProjectPath = fileName;
      setDirty(false);
      updateWindowTitle();
      statusBar()->showMessage(tr("Project saved"), 3000);
    } else {
      QMessageBox::critical(this, tr("Error"),
                            tr("Could not save the project."));
    }
  }
}

void MainWindow::onLoadProject() {
  if (exportLocksTakes() || !maybeSaveChanges())
    return;

  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Open a project"), "",
      tr("DubInstante projects (*.dbi *.zip);;Project (*.dbi);;Archive (*.zip)"));

  if (fileName.isEmpty())
    return;

  const bool isArchive = fileName.endsWith(".zip", Qt::CaseInsensitive);
  std::unique_ptr<QTemporaryDir> workDir;
  QString projectFile = fileName;
  if (isArchive) {
    projectFile = extractProjectArchive(fileName, workDir);
    if (projectFile.isEmpty())
      return;
  }

  if (!loadProjectFrom(projectFile, isArchive))
    return;

  // The previous project stays intact until the new one is loaded; reset()
  // then deletes its work dir (none left for a plain .dbi).
  m_archiveWorkDir = std::move(workDir);
  // A later save must rewrite the archive, not the extracted temp .dbi
  m_currentProjectPath = fileName;
  setDirty(false);
  updateWindowTitle();
}

QString MainWindow::extractProjectArchive(
    const QString &zipPath, std::unique_ptr<QTemporaryDir> &workDir) {
  // Never next to the archive: it may be on a read-only stick or share. Nor in
  // the system temp, a RAM-backed tmpfs on most Linux setups — saveWithMedia()
  // avoids it for the same reason: a multi-GB video would be extracted in RAM.
  const QString cacheDir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir().mkpath(cacheDir); // QTemporaryDir does not create parent directories
  workDir = std::make_unique<QTemporaryDir>(cacheDir +
                                            "/dubinstante_prj_XXXXXX");
  if (!workDir->isValid()) {
    workDir.reset();
    QMessageBox::critical(
        this, tr("Error"),
        tr("Could not create the temporary extraction folder."));
    return QString();
  }

  QString error;

  // Off the GUI thread: a video can weigh several GB. The nested loop keeps
  // this function sequential while the window stays responsive.
  QProgressDialog progress(this);
  progress.setLabelText(
      tr("Extracting the archive...\n"
         "This can take a few minutes depending on the video size."));
  progress.setRange(0, 0);
  progress.setCancelButton(nullptr);
  progress.setWindowModality(Qt::WindowModal);
  progress.show();

  QFutureWatcher<bool> watcher;
  QEventLoop loop;
  connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);
  const QString destDir = workDir->path();
  watcher.setFuture(QtConcurrent::run([&]() {
    return m_saveManager->extractArchive(zipPath, destDir, &error);
  }));
  loop.exec();
  progress.close();

  if (!watcher.result()) {
    workDir.reset();
    QMessageBox::critical(this, tr("Error"), error);
    return QString();
  }

  const QFileInfoList projects =
      QDir(destDir).entryInfoList({"*.dbi"}, QDir::Files);
  if (projects.size() == 1)
    return projects.first().absoluteFilePath();

  // Archive renamed then saved again: it holds the old and the new .dbi
  const QString expected = QFileInfo(zipPath).completeBaseName();
  for (const QFileInfo &project : projects) {
    if (project.completeBaseName() == expected)
      return project.absoluteFilePath();
  }

  workDir.reset();
  QMessageBox::critical(this, tr("Error"),
                        tr("This archive holds several projects and none is named after the "
                           "archive (%1).").arg(expected));
  return QString();
}

bool MainWindow::loadProjectFrom(const QString &path, bool strictRelative) {
  SaveData saveData;
  if (!m_saveManager->load(path, saveData)) {
    QMessageBox::critical(
        this, tr("Error"),
        tr("The file is corrupted or from an incompatible version."));
    return false;
  }

  // La plage « Dernier enregistrement » n'est pas sérialisée : celle de la
  // session précédente ne correspond pas au projet chargé.
  m_lastRecordedDurationMs = 0;
  m_lastRecordedStartMs = 0;

  // Apply loaded data
  m_speedSpinBox->setValue(saveData.scrollSpeed);

  // Set track count
  int loadedTrackCount = qBound(1, saveData.trackCount, MAX_TRACKS);
  setTrackCount(loadedTrackCount);

  // Restore rythmo tracks
  for (int i = 0; i < qMin(saveData.tracks.size(), m_trackCount); ++i) {
    m_rythmoManager->setText(i, saveData.tracks[i].text);
    m_rythmoManager->setTrackStyle(i, saveData.tracks[i].style);
    RythmoWidget *w = m_rythmoOverlay->track(i);
    if (w) {
      w->setText(saveData.tracks[i].text);
      // After style and speed: a file without a stored grid derives it from them
      w->setCharMs(saveData.tracks[i].charMs);
    }
  }

  // Paths come from a file that may have been written by someone else
  QFileInfo fi(path);
  QDir dir = fi.absoluteDir();
  int refusedPaths = 0;

  // Restore video and volume. Cleared first: the video of the previous project
  // may live in a work dir this load is about to delete.
  m_currentVideoPath.clear();
  if (!saveData.videoUrl.isEmpty()) {
    QString storedVideo = saveData.videoUrl;
    if (storedVideo.startsWith("file://")) {
      storedVideo = QUrl(storedVideo).toLocalFile();
    }

    // save() stores the video relative to the .dbi, often outside its folder
    // ("../Videos/film.mp4"): a standalone .dbi keeps that behavior, only an
    // archive is held to its own directory.
    QString localPath = SaveManager::resolveProjectPath(
        dir.absolutePath(), storedVideo, strictRelative, !strictRelative);

    // Hors du dossier projet il n'y a plus de confinement : le type de fichier
    // est la dernière barrière. Sans elle, un .dbi reçu d'un tiers désigne
    // n'importe quel fichier lisible, qui repart ensuite dans l'archive
    // produite par saveWithMedia(). Mêmes extensions que le sélecteur, sans son
    // échappatoire « Tous les fichiers » : ici personne ne choisit.
    if (!hasVideoSuffix(localPath))
      localPath.clear();

    if (localPath.isEmpty()) {
      ++refusedPaths;
    } else if (!QFile::exists(localPath)) {
      QMessageBox::warning(
          this, tr("Relink"),
          tr("The video was not found. Please locate it."));
      openVideoDialog(); // Simple relink via open file dialog
    } else {
      m_playbackEngine->openFile(QUrl::fromLocalFile(localPath));
      m_currentVideoPath = localPath;
    }
  }

  m_playbackEngine->setVolume(saveData.videoVolume);

  // Only the autosave stores absolute take paths (the temp WAVs of a crashed
  // session). A project file never does: accepting one would let a .dbi
  // received from someone else pull any readable file into track_N.wav, which
  // then leaves with the next archive.
  const bool strictAudio = strictRelative || path != autosaveFilePath();

  // Every take of the previous project goes: its players release the files first
  for (TrackPlayer *player : m_trackPlayers)
    player->releaseFiles();
  QDir(sessionDir()).removeRecursively();
  QDir().mkpath(sessionDir());
  m_recordingTakes.clear();
  m_nextTakeId = saveData.nextTakeId;

  // Restore audio device selection, gain and takes
  m_lastLoadLostTracksCount = 0;
  for (int i = 0; i < m_trackCount; ++i) {
    if (i >= saveData.audioTracks.size()) {
      applyTrackEdit(i, TakeTrack());
      continue;
    }
    const TrackAudioSaveData &audio = saveData.audioTracks[i];
    m_trackPanels[i]->setInputDevice(audio.audioInput);
    m_trackPanels[i]->setVolume(static_cast<int>(audio.audioGain * 100));

    // Copied into the session: the project folder stays untouched until saved
    TakeTrack takes = audio.takes;
    for (const Take &take : QList<Take>(takes.takes())) {
      const QString savedWavPath =
          SaveManager::resolveProjectPath(dir.absolutePath(), take.file, strictAudio);
      const QString sessionPath = sessionTakePath(i, take.id);
      if (savedWavPath.isEmpty()) {
        takes.removeTake(take.id);
        ++refusedPaths;
      } else if (!QFile::exists(savedWavPath) ||
                 !QFile::copy(savedWavPath, sessionPath)) {
        takes.removeTake(take.id);
        ++m_lastLoadLostTracksCount;
      } else {
        // Also gives a length to 0.12 takes saved without one
        takes.setTakeMedia(take.id, sessionPath, wavDurationMs(sessionPath));
      }
    }
    applyTrackEdit(i, takes);
  }

  if (refusedPaths > 0) {
    QMessageBox::warning(
        this, tr("Project"),
        tr("This project refers to files outside its folder. They were ignored "
           "for safety."));
  }
  // The autosave recovery reports its own losses in the status bar
  if (m_lastLoadLostTracksCount > 0 && path != autosaveFilePath()) {
    QMessageBox::warning(
        this, tr("Project"),
        tr("%n take(s) of this project were not found in its audio folder and "
           "were not loaded. Saving now would remove them from the project.",
           nullptr, m_lastLoadLostTracksCount));
  }

  // Loading pushed its own text edits; nothing before it can be undone into this project
  m_undoStack->clear();
  statusBar()->showMessage(tr("Project loaded"), 3000);
  return true;
}

// =============================================================================
// Slots - Playback Updates
// =============================================================================

void MainWindow::onPositionChanged(qint64 position) {
  m_takeTimeline->setPosition(position);

  m_timeLabel->setText(TimeFormatter::format(position) + " / " +
                       TimeFormatter::format(m_playbackEngine->duration()));
}

void MainWindow::onDurationChanged(qint64 duration) {
  m_takeTimeline->setDuration(duration);
}

void MainWindow::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
  if (state == QMediaPlayer::PlayingState) {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/pause.svg"));
    m_playPauseButton->setText(" " + tr("PAUSE"));
  } else {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/play.svg"));
    m_playPauseButton->setText(" " + tr("PLAY"));
  }
}

// =============================================================================
// Slots - Recording
// =============================================================================

void MainWindow::togglePlayback() {
  // Pausing the video alone would leave the microphone running out of sync
  if (m_isRecording || m_countdownTimer->isActive()) {
    toggleRecording();
  } else if (m_playbackEngine->playbackState() == QMediaPlayer::PlayingState) {
    m_playbackEngine->pause();
  } else {
    m_playbackEngine->play();
  }
}

void MainWindow::toggleRecording() {
  if (m_countdownTimer->isActive()) {
    // Cancel countdown
    m_countdownTimer->stop();
    if (m_countdownLabel) {
      m_countdownLabel->hide();
    }
    m_recordButton->setEnabled(true);
    m_recordButton->setChecked(false);
    m_recordButton->setText(recordIdleText());
    return;
  }

  if (!m_isRecording) {
    QString currentVideo = m_currentVideoPath;
    if (currentVideo.isEmpty()) {
      QMessageBox::warning(this, tr("Dubbing"),
                           tr("Load a video before recording."));
      m_recordButton->setChecked(false);
      return;
    }
    if (exportLocksTakes()) {
      m_recordButton->setChecked(false);
      return;
    }
    if (!m_pendingTakeChecks.isEmpty()) {
      // The previous WAVs are still being finalized: a new take would reuse
      // their recorders before they are validated.
      statusBar()->showMessage(tr("Finishing the previous take…"), 2000);
      m_recordButton->setChecked(false);
      return;
    }

    int duration = SettingsManager::instance().countdownDuration();
    if (duration > 0) {
      m_countdownRemaining = duration;
      
      if (!m_countdownLabel) {
        m_countdownLabel = new QLabel(m_videoFrame);
        m_countdownLabel->setObjectName("countdownLabel");
        m_countdownLabel->setAlignment(Qt::AlignCenter);
      }
      
      int lblWidth = 160;
      int lblHeight = 160;
      m_countdownLabel->setGeometry(
          (m_videoFrame->width() - lblWidth) / 2,
          (m_videoFrame->height() - lblHeight) / 2,
          lblWidth,
          lblHeight
      );
      m_countdownLabel->setText(QString::number(m_countdownRemaining));
      m_countdownLabel->show();
      m_countdownLabel->raise();
      
      m_recordButton->setText(tr("CANCEL"));
      m_recordButton->setChecked(true);
      
      m_countdownTimer->start(1000);
    } else {
      startRecordingProcess();
    }

  } else {
    m_playbackEngine->pause();

    // stop() is asynchronous: each take is validated by checkRecordedTake() once its
    // recorder reports StoppedState. Register every armed track before stopping any,
    // so a recorder stopping synchronously cannot report the result on a partial set.
    // The wall-clock length stands in until the WAV's own is read.
    const qint64 elapsed = m_recordingTimer.elapsed();
    qint64 recordStartMs = m_lastRecordedStartMs;
    for (auto it = m_recordingTakes.begin(); it != m_recordingTakes.end(); ++it) {
      it->durationMs = elapsed;
      recordStartMs = it->startMs;
      m_pendingTakeChecks.append(it.key());
    }

    for (int i = 0; i < m_trackCount; ++i) {
      if (m_recordingTakes.contains(i)) {
        m_audioRecorders[i]->stopRecording();
      }
      m_trackPlayers[i]->setSilentFrom(-1);
      m_trackPanels[i]->setRecordingState("");
    }

    // Exit fullscreen if active
    if (m_isFullscreenRecording) {
      exitFullscreenRecording();
    }

    // Unlock rythmo editing
    m_rythmoOverlay->setEditable(true);

    // m_lastRecordedStartMs already holds the punch-in point: the pre-roll is not part of it
    m_lastRecordedDurationMs = qMax<qint64>(0, recordStartMs + elapsed - m_lastRecordedStartMs);
    setDirty(true);

    m_isRecording = false;
    m_recordDurationTimer->stop();
    m_recordDurationLabel->setVisible(false);
    m_recordButton->setChecked(false);
    m_recordButton->setText(recordIdleText());
    updateEditActions();

    // A recorder that already stopped (failed to start, device lost mid-take)
    // will not emit StoppedState again: validate its take now.
    const QList<int> pending = m_pendingTakeChecks;
    for (int i : pending) {
      if (m_audioRecorders[i]->recorderState() == QMediaRecorder::StoppedState) {
        checkRecordedTake(i);
      }
    }
  }
}

void MainWindow::checkRecordedTake(int trackIndex) {
  if (!m_pendingTakeChecks.removeOne(trackIndex) || !m_recordingTakes.contains(trackIndex)) {
    return;
  }

  // Only a valid take joins the track: a failed one never costs the others
  Take take = m_recordingTakes.take(trackIndex);
  const qint64 measured = wavDurationMs(take.file);
  if (measured > 0)
    take.durationMs = measured;
  // A WAV header alone is 44 bytes: a file this small holds no audio
  const QFileInfo info(take.file);
  if (trackIndex < m_takeTracks.size() && info.exists() && info.size() > 1024 &&
      take.durationMs > take.inMs) {
    TakeTrack takes = m_takeTracks[trackIndex];
    takes.addRecording(take, take.audibleStartMs(), take.audibleEndMs());
    editTakes(trackIndex, takes, tr("Recording"));
  } else {
    QFile::remove(take.file);
    m_failedTakeTracks.append(trackIndex);
  }
  if (!m_pendingTakeChecks.isEmpty()) {
    return;
  }

  // Show post-recording notification bar (replaces showExportDialog)
  QString message = tr("✅ Recording finished!");
  if (!m_failedTakeTracks.isEmpty()) {
    std::sort(m_failedTakeTracks.begin(), m_failedTakeTracks.end());
    QStringList trackNumbers;
    for (int track : m_failedTakeTracks) {
      trackNumbers << QString::number(track + 1);
    }
    message = m_failedTakeTracks.size() == 1
        ? tr("Recording finished, but track %1 produced no audio. Check the "
             "selected microphone.").arg(trackNumbers.first())
        : tr("Recording finished, but tracks %1 produced no audio. Check the "
             "selected microphone.").arg(QLocale().createSeparatedList(trackNumbers));
    m_failedTakeTracks.clear();
  }
  showPostRecordBar(message);
  statusBar()->showMessage(message, 5000);

  const PendingSave pendingSave = m_pendingSave;
  m_pendingSave = PendingSave::None;
  if (pendingSave == PendingSave::Save) {
    onQuickSaveProject();
  } else if (pendingSave == PendingSave::SaveAs) {
    onSaveProject();
  }
}

// =============================================================================
// Fullscreen Recording
// =============================================================================

void MainWindow::enterFullscreenRecording() {
  // Reparent video and rythmo into fullscreen container
  m_videoWidget->setParent(m_fullscreenContainer);
  m_rythmoOverlay->setParent(m_fullscreenContainer);

  // Layout them inside the fullscreen container
  QVBoxLayout *fsLayout = new QVBoxLayout(m_fullscreenContainer);
  fsLayout->setContentsMargins(0, 0, 0, 0);
  fsLayout->setSpacing(0);
  fsLayout->addWidget(m_videoWidget);

  // Overlay must be raised above the video
  m_rythmoOverlay->show();
  m_rythmoOverlay->raise();
  m_videoWidget->show();

  m_isFullscreenRecording = true;

  // Install event filter on fullscreen container for resize sync
  m_fullscreenContainer->installEventFilter(this);

  m_fullscreenContainer->showFullScreen();
}

void MainWindow::exitFullscreenRecording() {
  m_fullscreenContainer->hide();

  // Clean up the layout from the fullscreen container
  QLayout *fsLayout = m_fullscreenContainer->layout();
  if (fsLayout) {
    while (fsLayout->count() > 0) {
      fsLayout->takeAt(0);
    }
    delete fsLayout;
  }

  // Reparent back into the video frame
  m_videoWidget->setParent(m_videoFrame);
  m_rythmoOverlay->setParent(m_videoFrame);

  m_videoWidget->show();
  m_rythmoOverlay->show();
  m_rythmoOverlay->raise();

  // Restore geometry to match the video frame
  m_videoWidget->setGeometry(0, 0, m_videoFrame->width(),
                             m_videoFrame->height());
  m_rythmoOverlay->setGeometry(0, 0, m_videoFrame->width(),
                               m_videoFrame->height());

  m_isFullscreenRecording = false;
}

// =============================================================================
// Shortcuts
// =============================================================================

void MainWindow::setupShortcuts() {
  m_shRecordStart = new QShortcut(this);
  m_shRecordStart->setContext(Qt::ApplicationShortcut);
  connect(m_shRecordStart, &QShortcut::activated, this, [this]() {
    if (!m_isRecording && !m_countdownTimer->isActive()) {
      toggleRecording();
    }
  });

  m_shRecordStop = new QShortcut(this);
  m_shRecordStop->setContext(Qt::ApplicationShortcut);
  connect(m_shRecordStop, &QShortcut::activated, this, [this]() {
    if (m_isRecording || m_countdownTimer->isActive()) {
      toggleRecording();
    }
  });

  m_shProjectSave = new QShortcut(this);
  m_shProjectSave->setContext(Qt::ApplicationShortcut);
  m_shProjectSave->setAutoRepeat(false);
  connect(m_shProjectSave, &QShortcut::activated, this,
          &MainWindow::onQuickSaveProject);

  m_shProjectSaveAs = new QShortcut(this);
  m_shProjectSaveAs->setContext(Qt::ApplicationShortcut);
  m_shProjectSaveAs->setAutoRepeat(false);
  connect(m_shProjectSaveAs, &QShortcut::activated, this,
          &MainWindow::onSaveProject);

  // The fullscreen recording is its own window: Escape leaves it by ending the take
  m_shFullscreenEscape = new QShortcut(m_fullscreenContainer);
  m_shFullscreenEscape->setAutoRepeat(false);
  connect(m_shFullscreenEscape, &QShortcut::activated, this, [this]() {
    if (m_isRecording)
      toggleRecording();
  });

  applyShortcuts();
}

void MainWindow::applyShortcuts() {
  SettingsManager &sm = SettingsManager::instance();
  m_shRecordStart->setKey(sm.shortcut("record_start"));
  m_shRecordStop->setKey(sm.shortcut("record_stop"));
  m_shProjectSave->setKey(sm.shortcut("project_save"));
  m_shProjectSaveAs->setKey(sm.shortcut("project_save_as"));
  // Same key on both would be ambiguous there and fire neither
  const QKeySequence escape(Qt::Key_Escape);
  m_shFullscreenEscape->setKey(m_shRecordStop->key() == escape ? QKeySequence() : escape);

  m_shortcutPlayPause = sm.shortcut("video_play_pause");
  m_shortcutFrameBack = sm.shortcut("video_frame_back");
  m_shortcutFrameForward = sm.shortcut("video_frame_forward");
  m_shortcutSeekBack5s = sm.shortcut("video_seek_back_5s");
  m_shortcutSeekForward5s = sm.shortcut("video_seek_forward_5s");
  m_shortcutVolumeUp = sm.shortcut("audio_volume_up");
  m_shortcutVolumeDown = sm.shortcut("audio_volume_down");
  m_shortcutVolumeMute = sm.shortcut("audio_volume_mute");
  m_shortcutTakeSplit = sm.shortcut("take_split");
  m_shortcutGoToStart = sm.shortcut("video_go_start");

  // Menu actions show their key next to the entry
  m_actionUndo->setShortcut(sm.shortcut("edit_undo"));
  m_actionRedo->setShortcut(sm.shortcut("edit_redo"));
  m_actionLoadProject->setShortcut(sm.shortcut("project_open"));
  m_actionOpenMp4->setShortcut(sm.shortcut("video_open"));
  m_actionManualExport->setShortcut(sm.shortcut("project_export"));
  m_actionWindowFullScreen->setShortcut(sm.shortcut("view_fullscreen"));
}



// =============================================================================
// Slots - Export
// =============================================================================

void MainWindow::onExportProgress(int percentage) {
  m_exportProgressBar->setValue(percentage);
}

void MainWindow::onExportFinished(bool success, const QString &message) {
  m_exportProgressBar->setVisible(false);
  m_exportCancelBtn->setVisible(false);

  if (success) {
    QMessageBox::information(this, tr("Export"), message);
  } else {
    QMessageBox::critical(this, tr("Export"), message);
  }
}

// ffmpeg reads the temp WAVs for the whole export: recording a new take or
// loading a project would replace them underneath it.
bool MainWindow::exportLocksTakes() {
  if (!m_exportService->isExporting())
    return false;
  QMessageBox::warning(
      this, tr("Export in progress"),
      tr("The export reads the current takes. Wait for it to finish or cancel "
         "it before going on."));
  return true;
}

void MainWindow::showExportDialog() {
  QString currentVideo = m_currentVideoPath;
  if (currentVideo.isEmpty()) {
    QMessageBox::warning(this, tr("Export"), tr("No video loaded."));
    return;
  }

  QString ffmpegError;
  if (!ExportService::isFFmpegAvailable(&ffmpegError)) {
    QMessageBox::warning(this, tr("Export"), ffmpegError);
    return;
  }
  
  QList<ExportTrack> tracks;
  QStringList missingTracks;
  for (int i = 0; i < m_trackCount; ++i) {
    ExportTrack track;
    track.number = i + 1;
    track.volume = m_trackPanels[i]->currentVolume() / 100.0f;
    track.muted = track.volume < 0.01f;
    bool missing = false;
    for (const Segment &segment : m_takeTracks[i].segments()) {
      if (!QFile::exists(segment.file)) {
        missing = true;
        continue;
      }
      track.segments.append(ExportSegment{segment.file, segment.timelineStartMs,
                                          segment.sourceOffsetMs, segment.durationMs});
    }
    if (missing)
      missingTracks.append(tr("Track %1").arg(i + 1));
    if (!track.segments.isEmpty())
      tracks.append(track);
  }
  if (tracks.isEmpty()) {
    QMessageBox::warning(this, tr("Export"), tr("No audio recording to export."));
    return;
  }
  if (!missingTracks.isEmpty()) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Export"),
        tr("Takes are missing for: %1.\nExport without them?").arg(QLocale().createSeparatedList(missingTracks)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      return;
    }
  }

  const float originalVol = m_playbackEngine->volume();
  const bool originalMuted = m_volumeMuteButton->isChecked();

  ExportDialog dialog(currentVideo, tracks, m_lastRecordedDurationMs,
                      m_lastRecordedStartMs, originalMuted ? 0.0f : originalVol, this);

  if (dialog.exec() == QDialog::Accepted) {
    ExportConfig config = dialog.exportConfig();
    
    // Set UI progress indicators
    m_exportProgressBar->setVisible(true);
    m_exportProgressBar->setValue(0);
    m_exportCancelBtn->setVisible(true);
    
    m_exportService->startExport(config);
  }
}

// =============================================================================
// Slots - Error Handling
// =============================================================================

void MainWindow::onError(const QString &errorMessage) {
  QMessageBox::critical(this, tr("Error"), errorMessage);
}

// =============================================================================
// Event Handling
// =============================================================================

bool MainWindow::event(QEvent *event) {
  // With nothing to stop, the record_stop key (Space by default) goes to the
  // focused widget instead of the application-wide shortcut: it plays and
  // pauses, or types on a band.
  if (event->type() == QEvent::ShortcutOverride && !m_isRecording &&
      !m_countdownTimer->isActive()) {
    const QKeyCombination combo = static_cast<QKeyEvent *>(event)->keyCombination();
    if (QKeySequence(combo) == m_shRecordStop->key()) {
      event->accept();
      return true;
    }
  }
  const bool handled = QMainWindow::event(event);
  // The splitter only has its real height once the window is laid out
  if (event->type() == QEvent::Show)
    fitTimeline();
  return handled;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::Resize) {
    if (watched->objectName() == "videoFrame") {
      QFrame *frame = qobject_cast<QFrame *>(watched);
      if (frame) {
        if (m_videoWidget) {
          m_videoWidget->setGeometry(0, 0, frame->width(), frame->height());
        }
        if (m_rythmoOverlay) {
          m_rythmoOverlay->setGeometry(0, 0, frame->width(), frame->height());
          m_rythmoOverlay->raise();
        }
        if (m_countdownLabel && m_countdownLabel->isVisible()) {
          int lblWidth = 160;
          int lblHeight = 160;
          m_countdownLabel->setGeometry(
              (frame->width() - lblWidth) / 2,
              (frame->height() - lblHeight) / 2,
              lblWidth,
              lblHeight
          );
          m_countdownLabel->raise();
        }
      }
    } else if (watched->objectName() == "fullscreenContainer" &&
               m_isFullscreenRecording) {
      QWidget *container = qobject_cast<QWidget *>(watched);
      if (container) {
        if (m_videoWidget) {
          m_videoWidget->setGeometry(0, 0, container->width(),
                                     container->height());
        }
        if (m_rythmoOverlay) {
          m_rythmoOverlay->setGeometry(0, 0, container->width(),
                                       container->height());
          m_rythmoOverlay->raise();
        }
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  // Resolve modifiers & key combo
  int key = event->key();
  if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
    QMainWindow::keyPressEvent(event);
    return;
  }

  int keyCombo = key;
  Qt::KeyboardModifiers modifiers = event->modifiers();
  if (modifiers & Qt::ShiftModifier)   keyCombo |= Qt::SHIFT;
  if (modifiers & Qt::ControlModifier) keyCombo |= Qt::CTRL;
  if (modifiers & Qt::AltModifier)     keyCombo |= Qt::ALT;
  if (modifiers & Qt::MetaModifier)    keyCombo |= Qt::META;

  QKeySequence pressedSeq(keyCombo);

  // Ignore auto-repeat for toggle/state actions (security rule)
  if (event->isAutoRepeat()) {
    if (pressedSeq == m_shortcutPlayPause ||
        pressedSeq == m_shortcutVolumeMute ||
        pressedSeq == SettingsManager::instance().shortcut("record_start") ||
        pressedSeq == SettingsManager::instance().shortcut("record_stop")) {
      event->accept();
      return;
    }
  }

  // Check focus context to prevent typing/editing interference
  QWidget *fw = focusWidget();
  bool inInput = fw && (fw->inherits("QLineEdit") ||
                        fw->inherits("QTextEdit") ||
                        fw->inherits("QAbstractSpinBox") ||
                        fw->inherits("RythmoWidget"));

  if (!inInput) {
    if (!m_shortcutPlayPause.isEmpty() && pressedSeq == m_shortcutPlayPause) {
      togglePlayback();
      event->accept();
      return;
    }

    if (!m_shortcutFrameBack.isEmpty() && pressedSeq == m_shortcutFrameBack) {
      m_playbackEngine->seek(qMax(0LL, m_playbackEngine->position() - frameStepMs()));
      event->accept();
      return;
    }

    if (!m_shortcutFrameForward.isEmpty() && pressedSeq == m_shortcutFrameForward) {
      m_playbackEngine->seek(qMin(m_playbackEngine->duration(), m_playbackEngine->position() + frameStepMs()));
      event->accept();
      return;
    }

    if (!m_shortcutSeekBack5s.isEmpty() && pressedSeq == m_shortcutSeekBack5s) {
      m_playbackEngine->seek(qMax(0LL, m_playbackEngine->position() - 5000));
      event->accept();
      return;
    }

    if (!m_shortcutSeekForward5s.isEmpty() && pressedSeq == m_shortcutSeekForward5s) {
      m_playbackEngine->seek(qMin(m_playbackEngine->duration(), m_playbackEngine->position() + 5000));
      event->accept();
      return;
    }

    if (!m_shortcutVolumeUp.isEmpty() && pressedSeq == m_shortcutVolumeUp) {
      m_volumeSlider->setValue(qMin(100, m_volumeSlider->value() + 5));
      event->accept();
      return;
    }

    if (!m_shortcutVolumeDown.isEmpty() && pressedSeq == m_shortcutVolumeDown) {
      m_volumeSlider->setValue(qMax(0, m_volumeSlider->value() - 5));
      event->accept();
      return;
    }

    if (!m_shortcutVolumeMute.isEmpty() && pressedSeq == m_shortcutVolumeMute) {
      m_volumeMuteButton->click();
      event->accept();
      return;
    }

    if (!m_shortcutTakeSplit.isEmpty() && pressedSeq == m_shortcutTakeSplit) {
      m_takeTimeline->splitSelectedAtPlayhead();
      event->accept();
      return;
    }

    if (!m_shortcutGoToStart.isEmpty() && pressedSeq == m_shortcutGoToStart) {
      m_playbackEngine->seek(0);
      event->accept();
      return;
    }
  }

  QMainWindow::keyPressEvent(event);
}

// =============================================================================
// Advanced Settings & Features Slots
// =============================================================================

void MainWindow::onOpenGlobalSettings() {
  GlobalSettingsDialog dialog(this, 0);
  if (dialog.exec() == QDialog::Accepted) {
    SettingsManager &sm = SettingsManager::instance();
    
    // Apply theme
    applyTheme();
    
    // Update auto-save timer
    m_autoSaveTimer->stop();
    if (sm.autoSaveEnabled()) {
      m_autoSaveTimer->start(sm.autoSaveInterval() * 60 * 1000);
    }
    
    
    // Update dynamic audio menu
    updateAudioMenu();
    
    // Apply default global microphone to empty input devices
    QString defaultMic = sm.defaultMicrophone();
    if (!defaultMic.isEmpty()) {
      for (int i = 0; i < m_trackCount; ++i) {
        if (m_trackPanels[i]->currentInputDevice().isEmpty()) {
          m_trackPanels[i]->setInputDevice(defaultMic);
        }
      }
    }

    // Apply shortcuts
    applyShortcuts();
    
    statusBar()->showMessage(tr("Settings updated"), 3000);
  }
}

QString MainWindow::autosaveFilePath() const {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
      .filePath("autosave_backup.dbi");
}

void MainWindow::discardAutosave() {
  if (m_ownsAutosave) {
    QFile::remove(autosaveFilePath());
  }
}

void MainWindow::checkForAutosaveRecovery() {
  QString autosaveFile = autosaveFilePath();
  if (!QFile::exists(autosaveFile)) {
    return;
  }

  QFileInfo fi(autosaveFile);
  QDateTime dt = fi.lastModified();
  QString dateStr = QLocale().toString(dt, QLocale::ShortFormat);

  QMessageBox msgBox(QMessageBox::Question,
                     tr("Recovery"),
                     tr("An autosave from %1 was found. DubInstante probably closed "
                        "unexpectedly.\n"
                        "Restore this work?")
                         .arg(dateStr),
                     QMessageBox::NoButton,
                     this);

  QPushButton *restoreBtn =
      msgBox.addButton(tr("Restore"), QMessageBox::AcceptRole);
  QPushButton *ignoreBtn =
      msgBox.addButton(tr("Ignore"), QMessageBox::DestructiveRole);
  msgBox.setDefaultButton(restoreBtn);

  bool timerWasActive = m_autoSaveTimer->isActive();
  if (timerWasActive) {
    m_autoSaveTimer->stop();
  }

  msgBox.exec();

  if (msgBox.clickedButton() == restoreBtn) {
    if (loadProjectFrom(autosaveFile)) {
      m_currentProjectPath.clear();
      setDirty(true);
      if (m_lastLoadLostTracksCount > 0) {
        statusBar()->showMessage(
            tr("Project restored. %n recording(s) not found.", nullptr,
               m_lastLoadLostTracksCount),
            8000);
      } else {
        statusBar()->showMessage(tr("Project restored"), 3000);
      }
    } else {
      // Illisible : écartée du chemin de démarrage pour ne pas reproposer le
      // même dialogue à chaque lancement, mais conservée pour inspection.
      QFile::remove(autosaveFile + ".corrupt");
      QFile::rename(autosaveFile, autosaveFile + ".corrupt");
    }
  } else if (msgBox.clickedButton() == ignoreBtn) {
    QFile::remove(autosaveFile);
  }
  // Sans bouton de rôle Reject, QMessageBox ignore Échap et la croix : le choix
  // est imposé, et seul un clic sur Ignorer détruit la sauvegarde.

  if (timerWasActive) {
    m_autoSaveTimer->start();
  }
}

void MainWindow::setDirty(bool dirty) {
  if (m_isDirty == dirty) {
    return;
  }
  m_isDirty = dirty;
  updateWindowTitle();
}

QString MainWindow::recordIdleText() const {
  //: Record button: records every armed track at once
  return "● " + tr("REC GLOBAL");
}

void MainWindow::updateWindowTitle() {
  QString title = m_currentProjectPath.isEmpty()
                      ? QStringLiteral("DubInstante - Studio")
                      : QStringLiteral("%1 — DubInstante")
                            .arg(QFileInfo(m_currentProjectPath).completeBaseName());
  if (m_isDirty) {
    title.prepend(QStringLiteral("* "));
  }
  setWindowTitle(title);
}

bool MainWindow::maybeSaveChanges() {
  if (!m_isDirty) {
    return true;
  }

  QMessageBox::StandardButton reply = QMessageBox::warning(
      this, tr("Unsaved changes"),
      tr("The project was modified. Save it before going on?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);

  if (reply == QMessageBox::Cancel) {
    return false;
  }
  if (reply == QMessageBox::Discard) {
    return true;
  }

  onQuickSaveProject(); // demande l'emplacement si le projet n'en a pas encore
  // ponytail: l'archive .zip est écrite en tâche de fond, m_isDirty ne retombe
  // qu'à la fin du QFutureWatcher. La fermeture est donc annulée et l'utilisateur
  // la relance une fois la barre de progression terminée. Passer à une fermeture
  // différée si ce détour devient gênant.
  return !m_isDirty;
}

void MainWindow::cleanupTempAudioFiles() {
  for (TrackPlayer *player : m_trackPlayers)
    player->releaseFiles();
  QDir(sessionDir()).removeRecursively();
}

void MainWindow::purgeStaleTempFiles() {
  QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
  const QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
  const QFileInfoList orphans = tempDir.entryInfoList(
      QStringList(QStringLiteral("dubinstante_*_track_*.wav")), QDir::Files);
  for (const QFileInfo &fi : orphans) {
    if (fi.lastModified() < cutoff) {
      QFile::remove(fi.absoluteFilePath());
    }
  }
  // Session dirs of crashed instances; the autosave recovery ran before this
  const QFileInfoList sessions = tempDir.entryInfoList(
      QStringList(QStringLiteral("dubinstante_session_*")),
      QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &fi : sessions) {
    if (fi.lastModified() < cutoff && fi.absoluteFilePath() != sessionDir())
      QDir(fi.absoluteFilePath()).removeRecursively();
  }

  // Une extraction interrompue par un crash laisse la vidéo entière derrière.
  // Même seuil que les prises : trop court supprimerait le dossier de travail
  // d'une autre instance, qui ne le réécrit pas une fois le projet ouvert.
  QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
  const QFileInfoList workDirs =
      cacheDir.entryInfoList(QStringList(QStringLiteral("dubinstante_prj_*")),
                             QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &fi : workDirs) {
    if (fi.lastModified() < cutoff)
      QDir(fi.absoluteFilePath()).removeRecursively();
  }
}

void MainWindow::onAutoSaveTriggered() {
  if (m_isRecording) {
    return; // Don't auto-save during active recording to prevent performance issues
  }

  // Rien à perdre : ni projet vierge ni projet déjà enregistré ne doivent laisser
  // un fichier derrière eux, sinon le prochain démarrage annonce un arrêt anormal
  // alors que le .dbi est à jour. Une instance secondaire écraserait la
  // sauvegarde de celle qui possède le verrou.
  if (!m_isDirty || !m_ownsAutosave) {
    return;
  }

  QString autosaveFile = autosaveFilePath();
  QDir dir = QFileInfo(autosaveFile).absoluteDir();
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // Keeps the absolute session paths: takes are recoverable without a copy pass
  SaveData saveData = collectSaveData();

  if (m_saveManager->save(autosaveFile, saveData)) {
    statusBar()->showMessage(tr("Autosave to cache done"), 2000);
  }
}

void MainWindow::onOutputDeviceTriggered(QAction *action) {
  if (!action) return;

  QString devDesc = action->data().toString();
  QList<QAudioDevice> availableDevices = QMediaDevices::audioOutputs();
  QAudioDevice targetDevice;

  for (const QAudioDevice &device : availableDevices) {
    if (device.description() == devDesc) {
      targetDevice = device;
      break;
    }
  }

  if (targetDevice.isNull()) {
    targetDevice = QMediaDevices::defaultAudioOutput();
  }

  m_playbackEngine->setAudioDevice(targetDevice);

  // Takes play on the same device
  for (TrackPlayer *player : m_trackPlayers) {
      player->setDevice(targetDevice);
  }

  // Update SettingsManager with currently active profile
  SettingsManager &sm = SettingsManager::instance();
  for (const QString &pref : sm.preferredOutputs()) {
    QStringList parts = pref.split("|");
    if (parts.size() >= 2 && parts[1] == devDesc) {
      sm.setActiveOutputProfile(pref);
      break;
    }
  }

  statusBar()->showMessage(tr("Audio output: %1").arg(action->text()), 3000);
}

void MainWindow::updateAudioMenu() {
  if (!m_audioMenu) return;

  // Clear old actions safely from the group
  for (QAction *action : m_outputDevicesGroup->actions()) {
    m_outputDevicesGroup->removeAction(action);
    action->deleteLater();
  }

  m_audioMenu->clear();

  SettingsManager &sm = SettingsManager::instance();
  QStringList preferred = sm.preferredOutputs();
  QString activeProfile = sm.activeOutputProfile();

  if (preferred.isEmpty()) {
    QAction *emptyAction = m_audioMenu->addAction(tr("No profile set up"));
    emptyAction->setEnabled(false);
  } else {
    for (const QString &pref : preferred) {
      QStringList parts = pref.split("|");
      if (parts.size() >= 2) {
        QString label = parts[0];
        QString devDesc = parts[1];

        QAction *action = new QAction(QString("%1 (%2)").arg(label, devDesc), m_audioMenu);
        action->setCheckable(true);
        action->setData(devDesc);

        m_audioMenu->addAction(action);
        m_outputDevicesGroup->addAction(action);

        if (pref == activeProfile) {
          action->setChecked(true);
        }
      }
    }
  }

  m_audioMenu->addSeparator();
  QAction *configAction = m_audioMenu->addAction(tr("Manage outputs..."));
  connect(configAction, &QAction::triggered, this, &MainWindow::onOpenGlobalSettings);
}

// =============================================================================
// Countdown Pre-Roll
// =============================================================================

void MainWindow::onCountdownTick() {
  m_countdownRemaining--;
  if (m_countdownRemaining > 0) {
    m_countdownLabel->setText(QString::number(m_countdownRemaining));
  } else {
    m_countdownTimer->stop();
    if (m_countdownLabel) {
      m_countdownLabel->hide();
    }
    m_recordButton->setEnabled(true);
    startRecordingProcess();
  }
}

void MainWindow::startRecordingProcess() {
  int armedCount = 0;
  for (int i = 0; i < m_trackCount; ++i) {
    if (m_trackPanels[i]->isArmed()) {
      armedCount++;
    }
  }

  if (armedCount == 0) {
    if (m_countdownLabel) {
      m_countdownLabel->hide();
    }
    m_recordButton->setChecked(false);
    m_recordButton->setText(recordIdleText());
    QMessageBox::warning(this, tr("Recording"),
                         tr("No track is armed. Arm at least one track before recording."));
    return;
  }

  for (int i = 0; i < m_trackCount; ++i) {
    if (m_trackPanels[i]->isArmed()) {
      QString dev = m_trackPanels[i]->currentInputDevice();
      if (dev.isEmpty()) {
        if (m_countdownLabel) {
          m_countdownLabel->hide();
        }
        m_recordButton->setChecked(false);
        m_recordButton->setText(recordIdleText());
        QMessageBox::warning(this, tr("Recording"),
                             tr("Track %1 is armed but no microphone is selected.")
                                 .arg(i + 1));
        return;
      }
    }
  }

  m_failedTakeTracks.clear();

  // Pre-roll: playback starts earlier so the actor hears the lead-in; the
  // armed tracks keep playing their comp up to the punch-in point.
  const qint64 punchInMs = m_playbackEngine->position();
  const qint64 startMs = qMax<qint64>(
      0, punchInMs - SettingsManager::instance().preRollSeconds() * 1000LL);
  if (startMs != punchInMs)
    m_playbackEngine->seek(startMs);
  m_lastRecordedStartMs = punchInMs;

  QDir().mkpath(sessionDir());
  m_recordingTakes.clear();
  for (int i = 0; i < m_trackCount; ++i) {
    if (m_trackPanels[i]->isArmed()) {
      // A new file per take: earlier takes stay untouched
      const int takeId = m_nextTakeId++;
      const Take take{takeId, sessionTakePath(i, takeId), startMs, punchInMs - startMs, 0};
      m_recordingTakes.insert(i, take);
      m_audioRecorders[i]->startRecording(QUrl::fromLocalFile(take.file));
      m_trackPlayers[i]->setSilentFrom(punchInMs);
      m_trackPanels[i]->setRecordingState("recording");
    } else if (!m_takeTracks[i].isEmpty()) {
      m_trackPanels[i]->setRecordingState("playing");
    }
  }

  // Enter fullscreen if action is checked
  if (m_actionFullscreen->isChecked()) {
    enterFullscreenRecording();
  }

  // Lock rythmo editing during recording
  m_rythmoOverlay->setEditable(false);

  m_playbackEngine->play();
  m_recordingTimer.start();
  m_recordDurationLabel->setText("00:00");
  m_recordDurationLabel->setVisible(true);
  m_recordDurationTimer->start(100);

  m_isRecording = true;
  m_recordButton->setEnabled(true);
  m_recordButton->setChecked(true);
  m_recordButton->setText(tr("STOP"));
  m_exportProgressBar->setVisible(false);
  updateEditActions();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // Avant toute chose : finaliser les WAV en cours. La finalisation est
  // asynchrone, le temps du dialogue lui suffit. Un compte à rebours est annulé
  // aussi : les dialogues ci-dessous font tourner la boucle d'événements, il
  // lancerait l'enregistrement dessous.
  // A save requested during the take would pop up over the dialog below once
  // the WAV is finalized: maybeSaveChanges() normally asks the question itself.
  m_pendingSave = PendingSave::None;
  if (m_isRecording || m_countdownTimer->isActive()) {
    toggleRecording();
  }

  if (!maybeSaveChanges()) {
    event->ignore();
    return;
  }

  if (m_exportService->isExporting()) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Export in progress"),
        tr("An export is in progress. Quitting now will stop it and delete the "
           "incomplete file.\n"
           "Quit anyway?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      event->ignore();
      return;
    }
    // L'arrêt de ffmpeg et la suppression du fichier sont faits par le
    // destructeur d'ExportService.
  }

  discardAutosave();
  cleanupTempAudioFiles();
  m_archiveWorkDir.reset();
  QMainWindow::closeEvent(event);
}
