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

// GUI includes
#include "ClickableSlider.h"
#include "RythmoOverlay.h"
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
#include <QVideoSink>

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
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

#include <QFutureWatcher>
#include <QProgressDialog>
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
      m_saveManager(new SaveManager(this))
      // Initialize state
      ,
      m_trackCount(0), m_previousVolume(100), m_isRecording(false),
      m_isFullscreenRecording(false), m_lastRecordedDurationMs(0),
      m_lastRecordedStartMs(0), m_recordingStartTimeMs(0),
      m_isDirty(false), m_lastLoadLostTracksCount(0),
      m_autoSaveTimer(new QTimer(this)),
      m_countdownTimer(new QTimer(this)),
      m_countdownRemaining(0),
      m_countdownLabel(nullptr),
      m_postRecordBar(nullptr),
      m_postRecordLabel(nullptr),
      m_outputDevicesGroup(new QActionGroup(this)) {
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
  setMinimumSize(800, 500);
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

// Every role Fusion draws with is set explicitly, including the Disabled group:
// a role left unset falls back to the OS theme palette, which is the opposite
// scheme half the time (light frames under the dark theme). The Disabled
// overrides come last, setColor(role, c) having written all three groups.
QPalette buildPalette(bool dark) {
  QPalette p;
  if (dark) {
    p.setColor(QPalette::Window, QColor("#0d0d12"));
    p.setColor(QPalette::WindowText, QColor("#f3f3f6"));
    p.setColor(QPalette::Base, QColor("#161622"));
    p.setColor(QPalette::AlternateBase, QColor("#212130"));
    p.setColor(QPalette::ToolTipBase, QColor("#212130"));
    p.setColor(QPalette::ToolTipText, QColor("#f3f3f6"));
    p.setColor(QPalette::Text, QColor("#f3f3f6"));
    p.setColor(QPalette::PlaceholderText, QColor(Brand::TextMuted));
    p.setColor(QPalette::Button, QColor("#212130"));
    p.setColor(QPalette::ButtonText, QColor("#f3f3f6"));
    p.setColor(QPalette::BrightText, QColor("#ffffff"));
    p.setColor(QPalette::Link, QColor(Brand::AccentLight));
    p.setColor(QPalette::Highlight, QColor(Brand::AccentLight));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Light, QColor(Brand::SurfaceAlt));
    p.setColor(QPalette::Midlight, QColor("#2a2a3c"));
    p.setColor(QPalette::Mid, QColor("#23232f"));
    p.setColor(QPalette::Dark, QColor("#12121a"));
    p.setColor(QPalette::Shadow, QColor("#000000"));
    const QColor disabled("#5c5c6f");
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  } else {
    p.setColor(QPalette::Window, QColor("#f3f4f6"));
    p.setColor(QPalette::WindowText, QColor("#1f2937"));
    p.setColor(QPalette::Base, QColor("#ffffff"));
    p.setColor(QPalette::AlternateBase, QColor("#f8fafc"));
    p.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
    p.setColor(QPalette::ToolTipText, QColor("#111827"));
    p.setColor(QPalette::Text, QColor("#111827"));
    p.setColor(QPalette::PlaceholderText, QColor("#94a3b8"));
    p.setColor(QPalette::Button, QColor("#f8fafc"));
    p.setColor(QPalette::ButtonText, QColor("#334155"));
    p.setColor(QPalette::BrightText, QColor("#ffffff"));
    p.setColor(QPalette::Link, QColor("#3b82f6"));
    p.setColor(QPalette::Highlight, QColor("#3b82f6"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Light, QColor("#ffffff"));
    p.setColor(QPalette::Midlight, QColor("#f3f4f6"));
    p.setColor(QPalette::Mid, QColor("#cbd5e1"));
    p.setColor(QPalette::Dark, QColor("#94a3b8"));
    p.setColor(QPalette::Shadow, QColor("#64748b"));
    const QColor disabled("#9aa3b2");
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

void MainWindow::releasePreviewSource(int trackIndex) {
    if (trackIndex >= m_previewPlayers.size()) return;
    QMediaPlayer *player = m_previewPlayers[trackIndex];
    player->stop();
    player->setSource(QUrl()); // explicitly release file handle
}

void MainWindow::refreshPreviewSources() {
    for (int i = 0; i < m_previewPlayers.size(); ++i) {
        if (!m_hasRecording.value(i, false)) continue;
        QString path = m_tempAudioPaths.value(i);
        if (path.isEmpty() || !QFile::exists(path)) continue;
        QUrl source = QUrl::fromLocalFile(path);
        if (m_previewPlayers[i]->source() != source) {
            m_previewPlayers[i]->setSource(source);
        }
    }
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

  m_videoWidget = new VideoWidget(m_videoFrame);
  m_videoWidget->show();

  m_rythmoOverlay = new RythmoOverlay(m_videoFrame);
  m_rythmoOverlay->show();

  QVBoxLayout *playerContainerLayout = new QVBoxLayout();
  playerContainerLayout->setContentsMargins(0, 0, 0, 0);
  playerContainerLayout->setSpacing(0);
  playerContainerLayout->addWidget(m_videoFrame, 1);

  mainLayout->addLayout(playerContainerLayout, 1);

  // Watch for resize events
  m_videoFrame->installEventFilter(this);

  // Fullscreen container (hidden until recording starts)
  m_fullscreenContainer =
      new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
  m_fullscreenContainer->setObjectName("fullscreenContainer");
  m_fullscreenContainer->setStyleSheet("background-color: black;");
  m_fullscreenContainer->hide();

  // =========================================================================
  // Position Slider
  // =========================================================================

  m_positionSlider = new ClickableSlider(Qt::Horizontal, this);
  m_positionSlider->setObjectName("positionSlider");
  m_positionSlider->setRange(0, 0);
  mainLayout->addWidget(m_positionSlider);

  // =========================================================================
  // Control Bar
  // =========================================================================

  QWidget *controlBarHost = new QWidget(this);
  QHBoxLayout *controlBarHostLayout = new QHBoxLayout(controlBarHost);
  controlBarHostLayout->setContentsMargins(12, 10, 12, 0);
  controlBarHostLayout->setSpacing(0);

  QWidget *controlBar = new QWidget(controlBarHost);
  controlBar->setObjectName("controlBar");
  controlBar->setMinimumHeight(72);
  controlBar->setAttribute(Qt::WA_StyledBackground, true);
  controlBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *controlShadow = new QGraphicsDropShadowEffect(controlBar);
  controlShadow->setBlurRadius(20);
  controlShadow->setOffset(0, 4);
  controlShadow->setColor(QColor(17, 24, 39, 28));
  controlBar->setGraphicsEffect(controlShadow);
  controlBarHostLayout->addWidget(controlBar);

  QHBoxLayout *controlBarLayout = new QHBoxLayout(controlBar);
  controlBarLayout->setContentsMargins(18, 12, 18, 12);
  controlBarLayout->setSpacing(12);

  m_stepBackButton = new QPushButton(QIcon(":/resources/icons/arrow_left.svg"), "", controlBar);
  m_stepBackButton->setObjectName("btnStepBack");
  m_stepBackButton->setProperty("cssClass", "iconButton");
  m_stepBackButton->setToolTip("Reculer (1 frame)");
  m_stepBackButton->setMinimumSize(32, 32);

  m_playPauseButton = new QPushButton(QIcon(":/resources/icons/play.svg"), " PLAY", controlBar);
  m_playPauseButton->setObjectName("btnPlayPause");
  m_playPauseButton->setProperty("class", "btn");

  m_stopButton = new QPushButton("⏹ STOP", controlBar);
  m_stopButton->setObjectName("btnStop");
  m_stopButton->setProperty("class", "btn");

  m_stepForwardButton = new QPushButton(QIcon(":/resources/icons/arrow_right.svg"), "", controlBar);
  m_stepForwardButton->setObjectName("btnStepForward");
  m_stepForwardButton->setProperty("cssClass", "iconButton");
  m_stepForwardButton->setToolTip("Avancer (1 frame)");
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

  QLabel *speedLabel = new QLabel("Vitesse Défilement", controlBar);
  speedLabel->setProperty("cssClass", "control-label");
  group2Layout->addWidget(speedLabel);

  m_speedDownButton = new QPushButton("−", controlBar);
  m_speedDownButton->setObjectName("speedDownButton");
  m_speedDownButton->setProperty("cssClass", "stepButton");
  m_speedDownButton->setToolTip(tr("Ralentir (-10%)"));
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
  m_speedUpButton->setToolTip(tr("Accélérer (+10%)"));
  m_speedUpButton->setMinimumSize(28, 28);
  group2Layout->addWidget(m_speedUpButton);


  m_recordButton = new QPushButton("● REC GLOBAL", controlBar);
  m_recordButton->setObjectName("recordButton");
  m_recordButton->setCheckable(true);
  m_recordButton->setMinimumHeight(34);
  m_recordButton->setMinimumWidth(132);
  m_recordButton->setCursor(Qt::PointingHandCursor);
  group2Layout->addWidget(m_recordButton);

  m_recordDurationLabel = new QLabel("00:00", controlBar);
  m_recordDurationLabel->setObjectName("recordDurationLabel");
  m_recordDurationLabel->setStyleSheet("color: #ff4d66; font-weight: bold; margin-left: 8px;");
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

  QLabel *masterLabel = new QLabel("Master Vol", controlBar);
  masterLabel->setProperty("cssClass", "control-label");
  group3Layout->addWidget(masterLabel);

  m_volumeMuteButton =
      new QPushButton(QIcon(":/resources/icons/volume.svg"), "", controlBar);
  m_volumeMuteButton->setObjectName("volumeMuteButton");
  m_volumeMuteButton->setProperty("cssClass", "iconButton");
  m_volumeMuteButton->setCheckable(true);
  m_volumeMuteButton->setToolTip(tr("Activer/Désactiver le son"));
  m_volumeMuteButton->setMinimumSize(30, 30);
  group3Layout->addWidget(m_volumeMuteButton);

  m_volumeDownButton = new QPushButton("−", controlBar);
  m_volumeDownButton->setObjectName("volumeDownButton");
  m_volumeDownButton->setProperty("cssClass", "stepButton");
  m_volumeDownButton->setToolTip(tr("Baisser le volume (-5%)"));
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
  m_volumeUpButton->setToolTip(tr("Monter le volume (+5%)"));
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

  m_exportCancelBtn = new QPushButton(tr("Annuler"), controlBar);
  m_exportCancelBtn->setVisible(false);
  group3Layout->addWidget(m_exportCancelBtn);

  controlBarLayout->addLayout(group3Layout);

  mainLayout->addWidget(controlBarHost);

  // =========================================================================
  // Post-Record Notification Bar
  // =========================================================================

  m_postRecordBar = new QWidget(centralWidget);
  m_postRecordBar->setObjectName("postRecordBar");
  m_postRecordBar->setVisible(false);

  QHBoxLayout *prLayout = new QHBoxLayout(m_postRecordBar);
  prLayout->setContentsMargins(12, 6, 12, 6);

  m_postRecordLabel = new QLabel(tr("✅ Enregistrement terminé !"), m_postRecordBar);
  m_postRecordLabel->setProperty("cssClass", "settingsLabel");
  prLayout->addWidget(m_postRecordLabel);
  prLayout->addStretch();

  QPushButton *listenBtn = new QPushButton(tr("▶ Écouter"), m_postRecordBar);
  listenBtn->setProperty("cssClass", "presetButton");
  connect(listenBtn, &QPushButton::clicked, this, [this]() {
      m_playbackEngine->seek(m_recordingStartTimeMs);
      m_playbackEngine->play();
      hidePostRecordBar();
  });
  prLayout->addWidget(listenBtn);

  QPushButton *exportBtn = new QPushButton(tr("📤 Exporter"), m_postRecordBar);
  exportBtn->setProperty("cssClass", "presetButton");
  connect(exportBtn, &QPushButton::clicked, this, [this]() {
      hidePostRecordBar();
      showExportDialog();
  });
  prLayout->addWidget(exportBtn);

  QPushButton *closeBtn = new QPushButton(tr("✕"), m_postRecordBar);
  closeBtn->setProperty("cssClass", "iconButton");
  closeBtn->setFixedSize(28, 28);
  connect(closeBtn, &QPushButton::clicked, this, &MainWindow::hidePostRecordBar);
  prLayout->addWidget(closeBtn);

  mainLayout->addWidget(m_postRecordBar);

  // =========================================================================
  // Mixer Zone
  // =========================================================================

  QWidget *mixerHost = new QWidget(this);
  QHBoxLayout *mixerHostLayout = new QHBoxLayout(mixerHost);
  mixerHostLayout->setContentsMargins(12, 10, 12, 12);
  mixerHostLayout->setSpacing(0);

  QWidget *mixerZone = new QWidget(mixerHost);
  mixerZone->setObjectName("mixerZone");
  mixerZone->setFixedHeight(240);
  mixerZone->setAttribute(Qt::WA_StyledBackground, true);
  m_tracksLayout = new QHBoxLayout(mixerZone);
  m_tracksLayout->setContentsMargins(18, 18, 18, 18);
  m_tracksLayout->setSpacing(14);

  mixerHostLayout->addWidget(mixerZone);

  mainLayout->addWidget(mixerHost);

  // Initial sync
  m_rythmoOverlay->setSpeed(m_speedSpinBox->value());
  m_rythmoManager->setSpeed(m_speedSpinBox->value());
}

void MainWindow::createMenus() {
  // Use QMainWindow's built-in menuBar() to ensure the native macOS menu bar
  // is used. Creating a `new QMenuBar(this)` can bypass native integration.
  QMenuBar *mb = menuBar();

  // === Files Menu ===
  QMenu *filesMenu = mb->addMenu(tr("Files"));

  m_actionOpenMp4 = new QAction(tr("Open MP4"), this);
  connect(m_actionOpenMp4, &QAction::triggered, this, &MainWindow::onOpenFile);
  filesMenu->addAction(m_actionOpenMp4);

  m_actionLoadProject = new QAction(tr("Open save file"), this);
  connect(m_actionLoadProject, &QAction::triggered, this,
          &MainWindow::onLoadProject);
  filesMenu->addAction(m_actionLoadProject);

  m_actionSaveProject = new QAction(tr("Save .dbi / .zip"), this);
  connect(m_actionSaveProject, &QAction::triggered, this,
          &MainWindow::onSaveProject);
  filesMenu->addAction(m_actionSaveProject);

  m_actionManualExport = new QAction(tr("Exporter le doublage..."), this);
  connect(m_actionManualExport, &QAction::triggered, this,
          &MainWindow::showExportDialog);
  filesMenu->addAction(m_actionManualExport);

  // === Application Menu ===
  QMenu *appMenu = mb->addMenu(tr("Application"));

  m_actionFullscreen = new QAction(tr("Fullscreen mode"), this);
  m_actionFullscreen->setCheckable(true);
  appMenu->addAction(m_actionFullscreen);



  m_actionGlobalSettings = new QAction(tr("Paramètre global"), this);
  appMenu->addAction(m_actionGlobalSettings);

  // === Bande Rythmo Menu ===
  QMenu *rythmoMenu = mb->addMenu(tr("Bande Rythmo"));

  // Track count selector using plain QActions (QWidgetAction with embedded
  // widgets doesn't work on macOS native menu bars).
  QAction *actionRemoveTrack = new QAction(tr("Retirer une bande (−)"), this);
  connect(actionRemoveTrack, &QAction::triggered, this, [this]() {
    if (!m_isRecording) {
      setTrackCount(m_trackCount - 1);
    }
  });
  rythmoMenu->addAction(actionRemoveTrack);

  QAction *actionAddTrack = new QAction(tr("Ajouter une bande (+)"), this);
  connect(actionAddTrack, &QAction::triggered, this, [this]() {
    if (!m_isRecording) {
      setTrackCount(m_trackCount + 1);
    }
  });
  rythmoMenu->addAction(actionAddTrack);

  rythmoMenu->addSeparator();

  m_actionPersonalizeRythmo = new QAction(tr("Personnaliser"), this);
  rythmoMenu->addAction(m_actionPersonalizeRythmo);

  // === Audio Menu ===
  m_audioMenu = mb->addMenu(tr("Audio"));
  updateAudioMenu();

}

void MainWindow::setupConnections() {
  // =========================================================================
  // Playback Controls
  // =========================================================================

  connect(m_playPauseButton, &QPushButton::clicked, this, [this]() {
    if (m_playbackEngine->playbackState() == QMediaPlayer::PlayingState) {
      m_playbackEngine->pause();
    } else {
      m_playbackEngine->play();
    }
  });

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

  // Sync preview playback
  connect(m_playbackEngine, &PlaybackEngine::positionChanged,
          this, &MainWindow::handlePreviewSync);
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged,
          this, &MainWindow::handlePreviewStateChange);

  // PlaybackEngine -> RythmoOverlay
  connect(m_playbackEngine, &PlaybackEngine::positionChanged, m_rythmoOverlay,
          &RythmoOverlay::sync);
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged, this,
          [this](QMediaPlayer::PlaybackState state) {
            m_rythmoOverlay->setPlaying(state == QMediaPlayer::PlayingState);
          });

  // =========================================================================
  // Position Slider
  // =========================================================================

  connect(m_positionSlider, &QSlider::sliderMoved, m_playbackEngine,
          &PlaybackEngine::seek);

  // Frame stepping configuration
  connect(m_playbackEngine, &PlaybackEngine::metaDataChanged, this, [this]() {
    const int step = static_cast<int>(frameStepMs());
    m_positionSlider->setSingleStep(step);
    m_positionSlider->setPageStep(10 * step);
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

  QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);

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
              if (!m_isRecording) {
                refreshPreviewSources();
              }
            });

    // Create TrackWidget
    TrackWidget *panel = new TrackWidget(idx + 1, 
        QString("Piste %1").arg(idx + 1), Brand::Accent, this);
    m_trackPanels.append(panel);
    m_tracksLayout->addWidget(panel);

    // Initialize tracking metadata
    m_hasRecording.append(false);
    m_trackRecordStartMs.append(0);
    m_trackRecordDurationMs.append(0);
    m_trackSyncThrottle.append(QElapsedTimer());

    // Create Preview Player & Output
    auto *audioOutput = new QAudioOutput(this);
    audioOutput->setDevice(m_playbackEngine->audioDevice());
    audioOutput->setVolume(0.8f); // matches TrackWidget default slider

    auto *player = new QMediaPlayer(this);
    player->setAudioOutput(audioOutput);
    player->setVideoSink(new QVideoSink(player)); // Prevent spawning new window for audio

    m_previewPlayers.append(player);
    m_previewOutputs.append(audioOutput);

    connect(panel, &TrackWidget::volumeChanged, this, [this, idx](int vol) {
        if (idx < m_previewOutputs.size()) {
            m_previewOutputs[idx]->setVolume(static_cast<float>(vol) / 100.0f);
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
                    // "Aucune entrée" selected → stop monitoring
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

    // Setup temp audio path (unique par instance : deux applications lancées
    // en parallèle ne doivent pas écrire dans le même WAV)
    m_tempAudioPaths.append(
        tempDir + QString("/dubinstante_%1_track_%2.wav")
                      .arg(QCoreApplication::applicationPid())
                      .arg(idx + 1));

    // Initialize RythmoManager text for this track
    m_rythmoManager->setText(idx, "");

    m_trackCount++;
  }

  // Remove tracks if needed
  while (m_trackCount > count) {
    int idx = m_trackCount - 1;

    // Remove TrackWidget
    TrackWidget *panel = m_trackPanels.takeLast();
    m_tracksLayout->removeWidget(panel);
    panel->deleteLater();

    // Remove AudioRecorder
    AudioRecorder *recorder = m_audioRecorders.takeLast();
    recorder->stopMonitoring();
    recorder->deleteLater();

    // Remove temp path
    m_tempAudioPaths.removeLast();

    // Remove tracking metadata
    m_hasRecording.removeLast();
    m_trackRecordStartMs.removeLast();
    m_trackRecordDurationMs.removeLast();
    m_trackSyncThrottle.removeLast();

    // Clean up preview player
    QMediaPlayer *previewPlayer = m_previewPlayers.takeLast();
    previewPlayer->stop();
    previewPlayer->setSource(QUrl());
    previewPlayer->deleteLater();

    QAudioOutput *previewOutput = m_previewOutputs.takeLast();
    previewOutput->deleteLater();

    m_trackCount--;
  }

  // Update RythmoOverlay
  m_rythmoOverlay->setTrackCount(count);

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
            m_rythmoManager->setText(index, text);
            setDirty(true);
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
  QString fileName = QFileDialog::getOpenFileName(this, tr("Ouvrir"), "",
                                                  tr("Vidéos MP4 (*.mp4)"));

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
  SaveData data;
  data.videoUrl = m_currentVideoPath;
  data.videoVolume = m_playbackEngine->volume();
  data.trackCount = m_trackCount;
  data.scrollSpeed = m_speedSpinBox->value();

  for (int i = 0; i < m_trackCount; ++i) {
    TrackAudioSaveData audioData;
    audioData.audioInput = m_trackPanels[i]->currentInputDevice();
    audioData.audioGain = m_trackPanels[i]->currentVolume() / 100.0f;
    audioData.hasRecording = m_hasRecording.value(i, false);
    audioData.recordStartMs = m_trackRecordStartMs.value(i, 0);
    audioData.recordDurationMs = m_trackRecordDurationMs.value(i, 0);
    data.audioTracks.append(audioData);
  }

  for (int i = 0; i < m_trackCount; ++i) {
    TrackSaveData trackData;
    trackData.text = m_rythmoManager->text(i);
    trackData.style = m_rythmoManager->trackStyle(i);
    data.tracks.append(trackData);
  }

  return data;
}

void MainWindow::onSaveProject() {
  if (deferSaveDuringTake(PendingSave::SaveAs))
    return;

  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(
      this, tr("Sauvegarder"),
      tr("Voulez-vous inclure la vidéo dans l'archive ?\n(Cela créera un "
         "fichier .zip)"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if (reply == QMessageBox::Cancel)
    return;

  bool saveWithVideo = (reply == QMessageBox::Yes);
  QString filter = saveWithVideo ? tr("DubInstante Archive (*.zip)")
                                 : tr("DubInstante Project (*.dbi)");
  QString suffix = saveWithVideo ? ".zip" : ".dbi";

  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Sauvegarder le projet"), "", filter);

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
      tr("Le projet sera enregistré à la fin de la prise."), 5000);
  return true;
}

void MainWindow::saveProjectTo(const QString &fileName, bool saveWithVideo) {
  SaveData data = collectSaveData();

  // Setup audio sub-directory for this project
  QFileInfo fi(fileName);
  QDir dir = fi.absoluteDir();
  QString baseName = fi.completeBaseName();
  QString audioDirName = baseName + "_audio";
  QString audioDirPath = dir.absoluteFilePath(audioDirName);
  QDir audioDir(audioDirPath);

  // Check if we have any audio to save
  bool hasAnyAudio = false;
  for (int i = 0; i < m_trackCount; ++i) {
      if (m_hasRecording.value(i, false)) {
          hasAnyAudio = true;
          break;
      }
  }

  if (!saveWithVideo && hasAnyAudio && !audioDir.exists()) {
      dir.mkdir(audioDirName);
  }

  // Attach project-relative audio paths and copy takes next to the .dbi
  for (int i = 0; i < data.audioTracks.size(); ++i) {
    TrackAudioSaveData &audioData = data.audioTracks[i];
    if (!audioData.hasRecording)
      continue;

    QString tempPath = m_tempAudioPaths.value(i);
    QString destFilename = QString("track_%1.wav").arg(i + 1);

    // Always populate the relative path for serialization
    audioData.audioFilePath = audioDirName + "/" + destFilename;

    // Only physically copy files here if NOT saving as zip
    if (!saveWithVideo) {
        QString destPath = audioDir.absoluteFilePath(destFilename);
        if (QFile::exists(tempPath)) {
            if (QFile::exists(destPath)) QFile::remove(destPath);
            // Un échec doit faire échouer la sauvegarde : le projet resterait
            // sinon marqué enregistré et closeEvent détruirait le WAV
            // temporaire, seule copie de la prise.
            if (!QFile::copy(tempPath, destPath)) {
                QMessageBox::critical(
                    this, tr("Erreur"),
                    tr("Impossible de copier l'enregistrement de la piste %1 "
                       "vers :\n%2\n\nVérifiez l'espace disque ou les "
                       "permissions. Le projet n'a pas été sauvegardé.")
                        .arg(i + 1)
                        .arg(QDir::toNativeSeparators(destPath)));
                return;
            }
        }
    }
  }

  if (saveWithVideo) {
    // Check zip availability BEFORE launching thread (for specific error
    // messages)
    QString zipError;
    if (!SaveManager::isZipAvailable(&zipError)) {
      QMessageBox::critical(this, tr("Erreur de sauvegarde"), zipError);
      return;
    }

    // Show progress dialog
    QProgressDialog *progressDialog = new QProgressDialog(this);
    progressDialog->setLabelText(
        tr("Création de l'archive ZIP en cours...\nCela peut prendre quelques "
           "minutes selon la taille de la vidéo."));
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
                statusBar()->showMessage(tr("Projet sauvegardé"), 3000);
              } else {
                QMessageBox::critical(
                    this, tr("Erreur"),
                    errorMessage->isEmpty()
                        ? tr("Impossible de créer l'archive ZIP.\nVérifiez "
                             "l'espace disque ou les permissions.")
                        : *errorMessage);
              }
            });

    // Copy: the worker thread must not read members owned by the GUI thread
    const QStringList audioPaths = m_tempAudioPaths;
    QFuture<bool> future = QtConcurrent::run([this, fileName, data, audioPaths, errorMessage]() {
      return m_saveManager->saveWithMedia(fileName, data, audioPaths, errorMessage.get());
    });
    watcher->setFuture(future);

  } else {
    if (m_saveManager->save(fileName, data)) {
      discardAutosave();
      m_currentProjectPath = fileName;
      setDirty(false);
      updateWindowTitle();
      statusBar()->showMessage(tr("Projet sauvegardé"), 3000);
    } else {
      QMessageBox::critical(this, tr("Erreur"),
                            tr("Impossible de sauvegarder le projet."));
    }
  }
}

void MainWindow::onLoadProject() {
  if (!maybeSaveChanges())
    return;

  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Charger un projet"), "",
      tr("Projets DubInstante (*.dbi *.zip);;Projet (*.dbi);;Archive (*.zip)"));

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
        this, tr("Erreur"),
        tr("Impossible de créer le dossier temporaire d'extraction."));
    return QString();
  }

  QString error;

  // Off the GUI thread: a video can weigh several GB. The nested loop keeps
  // this function sequential while the window stays responsive.
  QProgressDialog progress(this);
  progress.setLabelText(
      tr("Extraction de l'archive en cours...\nCela peut prendre quelques "
         "minutes selon la taille de la vidéo."));
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
    QMessageBox::critical(this, tr("Erreur"), error);
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
  QMessageBox::critical(this, tr("Erreur"),
                        tr("Cette archive contient plusieurs projets et aucun "
                           "ne porte le nom de l'archive (%1).").arg(expected));
  return QString();
}

bool MainWindow::loadProjectFrom(const QString &path, bool strictRelative) {
  SaveData data;
  if (!m_saveManager->load(path, data)) {
    QMessageBox::critical(
        this, tr("Erreur"),
        tr("Le fichier est corrompu ou d'une version incompatible."));
    return false;
  }

  // La plage « Dernier enregistrement » n'est pas sérialisée : celle de la
  // session précédente ne correspond pas au projet chargé.
  m_lastRecordedDurationMs = 0;
  m_lastRecordedStartMs = 0;

  // Apply loaded data
  m_speedSpinBox->setValue(data.scrollSpeed);

  // Set track count
  int loadedTrackCount = qBound(1, data.trackCount, MAX_TRACKS);
  setTrackCount(loadedTrackCount);

  // Restore rythmo tracks
  for (int i = 0; i < qMin(data.tracks.size(), m_trackCount); ++i) {
    m_rythmoManager->setText(i, data.tracks[i].text);
    m_rythmoManager->setTrackStyle(i, data.tracks[i].style);
    RythmoWidget *w = m_rythmoOverlay->track(i);
    if (w)
      w->setText(data.tracks[i].text);
  }

  // Paths come from a file that may have been written by someone else
  QFileInfo fi(path);
  QDir dir = fi.absoluteDir();
  int refusedPaths = 0;

  // Restore video and volume. Cleared first: the video of the previous project
  // may live in a work dir this load is about to delete.
  m_currentVideoPath.clear();
  if (!data.videoUrl.isEmpty()) {
    QString storedVideo = data.videoUrl;
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
    // produite par saveWithMedia(). Même filtre que openVideoDialog().
    if (!localPath.endsWith(".mp4", Qt::CaseInsensitive))
      localPath.clear();

    if (localPath.isEmpty()) {
      ++refusedPaths;
    } else if (!QFile::exists(localPath)) {
      QMessageBox::warning(
          this, tr("Relink"),
          tr("La vidéo est introuvable. Veuillez la localiser."));
      openVideoDialog(); // Simple relink via open file dialog
    } else {
      m_playbackEngine->openFile(QUrl::fromLocalFile(localPath));
      m_currentVideoPath = localPath;
    }
  }

  m_playbackEngine->setVolume(data.videoVolume);

  // Restore audio device selection and gain
  m_lastLoadLostTracksCount = 0;
  for (int i = 0; i < qMin(data.audioTracks.size(), m_trackCount); ++i) {
    m_trackPanels[i]->setInputDevice(data.audioTracks[i].audioInput);
    m_trackPanels[i]->setVolume(static_cast<int>(data.audioTracks[i].audioGain * 100));

    // Restore recording metadata
    m_hasRecording[i] = data.audioTracks[i].hasRecording;
    m_trackRecordStartMs[i] = data.audioTracks[i].recordStartMs;
    m_trackRecordDurationMs[i] = data.audioTracks[i].recordDurationMs;

    // Restore WAV file
    if (m_hasRecording[i] && !data.audioTracks[i].audioFilePath.isEmpty()) {
        const QString savedWavPath = SaveManager::resolveProjectPath(
            dir.absolutePath(), data.audioTracks[i].audioFilePath,
            strictRelative);
        if (savedWavPath.isEmpty()) {
            m_hasRecording[i] = false;
            ++refusedPaths;
        } else if (QFile::exists(savedWavPath)) {
            QString tempPath = m_tempAudioPaths.value(i);
            if (savedWavPath != tempPath) {
                if (QFile::exists(tempPath)) QFile::remove(tempPath);
                QFile::copy(savedWavPath, tempPath);
            }
        } else {
            m_hasRecording[i] = false; // file is missing
            m_lastLoadLostTracksCount++;
        }
    }
  }

  // Load recordings into preview players
  refreshPreviewSources();

  if (refusedPaths > 0) {
    QMessageBox::warning(
        this, tr("Projet"),
        tr("Ce projet référence des fichiers situés hors de son dossier. "
           "Ils ont été ignorés par sécurité."));
  }

  statusBar()->showMessage(tr("Projet chargé"), 3000);
  return true;
}

// =============================================================================
// Slots - Playback Updates
// =============================================================================

void MainWindow::onPositionChanged(qint64 position) {
  if (!m_positionSlider->isSliderDown()) {
    m_positionSlider->setValue(static_cast<int>(position));
  }

  m_timeLabel->setText(TimeFormatter::format(position) + " / " +
                       TimeFormatter::format(m_playbackEngine->duration()));
}

void MainWindow::onDurationChanged(qint64 duration) {
  m_positionSlider->setRange(0, static_cast<int>(duration));
}

void MainWindow::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
  if (state == QMediaPlayer::PlayingState) {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/pause.svg"));
    m_playPauseButton->setText(" PAUSE");
  } else {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/play.svg"));
    m_playPauseButton->setText(" PLAY");
  }
}

void MainWindow::handlePreviewSync(qint64 masterPosition) {
    for (int i = 0; i < m_previewPlayers.size(); ++i) {
        QMediaPlayer *player = m_previewPlayers[i];
        if (!m_hasRecording.value(i, false)) continue;
        if (m_isRecording && m_trackPanels[i]->isArmed()) continue;

        qint64 startMs = m_trackRecordStartMs.value(i, 0);
        qint64 durMs = m_trackRecordDurationMs.value(i, 0);
        qint64 targetPos = masterPosition - startMs;

        // The track is active only inside its own [start, start+duration] window.
        // Outside it (before the punch-in point or after the take has ended) the
        // preview must stay paused — restarting a finished player would replay it
        // from the beginning.
        bool active = targetPos >= 0 && (durMs <= 0 || targetPos < durMs);

        if (active && m_playbackEngine->playbackState() == QMediaPlayer::PlayingState) {
            if (player->playbackState() != QMediaPlayer::PlayingState) {
                // Crossing into the track's window: start it.
                player->blockSignals(true);
                player->setPosition(targetPos);
                player->play();
                player->blockSignals(false);
            } else {
                qint64 drift = qAbs(player->position() - targetPos);
                QElapsedTimer &throttle = m_trackSyncThrottle[i];
                if (drift > 150 && (!throttle.isValid() || throttle.elapsed() >= 1000)) {
                    player->blockSignals(true);
                    player->setPosition(targetPos);
                    player->blockSignals(false);
                    throttle.restart();
                }
            }
        } else if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->blockSignals(true);
            player->pause();
            player->blockSignals(false);
        }
    }
}

void MainWindow::handlePreviewStateChange(QMediaPlayer::PlaybackState state) {
    for (int i = 0; i < m_previewPlayers.size(); ++i) {
        QMediaPlayer *player = m_previewPlayers[i];
        if (!m_hasRecording.value(i, false)) continue;

        // Skip tracks that are being recorded right now
        if (m_isRecording && m_trackPanels[i]->isArmed()) continue;

        player->blockSignals(true);
        switch (state) {
            case QMediaPlayer::PlayingState: {
                if (player->source().isEmpty()) {
                    refreshPreviewSources();
                }
                qint64 startMs = m_trackRecordStartMs.value(i, 0);
                qint64 durMs = m_trackRecordDurationMs.value(i, 0);
                qint64 targetPos = m_playbackEngine->position() - startMs;
                if (targetPos >= 0 && (durMs <= 0 || targetPos < durMs)) {
                    player->setPosition(targetPos);
                    player->play();
                } else {
                    player->pause();
                }
                break;
            }
            case QMediaPlayer::PausedState:
                player->pause();
                break;
            case QMediaPlayer::StoppedState:
                player->stop();
                break;
        }
        player->blockSignals(false);
    }
}

// =============================================================================
// Slots - Recording
// =============================================================================

void MainWindow::toggleRecording() {
  if (m_countdownTimer->isActive()) {
    // Cancel countdown
    m_countdownTimer->stop();
    if (m_countdownLabel) {
      m_countdownLabel->hide();
    }
    m_recordButton->setEnabled(true);
    m_recordButton->setChecked(false);
    m_recordButton->setText("● REC GLOBAL");
    return;
  }

  if (!m_isRecording) {
    QString currentVideo = m_currentVideoPath;
    if (currentVideo.isEmpty()) {
      QMessageBox::warning(this, tr("Dubbing"),
                           tr("Chargez une vidéo avant d'enregistrer."));
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
      
      m_recordButton->setText("ANNULER");
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
    for (int i = 0; i < m_trackCount; ++i) {
      if (m_trackPanels[i]->isArmed()) {
        m_trackRecordDurationMs[i] = m_recordingTimer.elapsed();
        m_pendingTakeChecks.append(i);
      }
    }

    // Stop recording on ARMED tracks only
    for (int i = 0; i < m_trackCount; ++i) {
      if (m_trackPanels[i]->isArmed()) {
        m_audioRecorders[i]->stopRecording();
      }
      // Stop unarmed preview playback
      if (i < m_previewPlayers.size()) {
        m_previewPlayers[i]->blockSignals(true);
        m_previewPlayers[i]->pause();
        m_previewPlayers[i]->blockSignals(false);
      }
      // Clear visual state
      m_trackPanels[i]->setRecordingState("");
    }

    // Exit fullscreen if active
    if (m_isFullscreenRecording) {
      exitFullscreenRecording();
    }

    // Unlock rythmo editing
    m_rythmoOverlay->setEditable(true);

    m_lastRecordedDurationMs = m_recordingTimer.elapsed();
    m_lastRecordedStartMs = m_recordingStartTimeMs;
    setDirty(true);

    m_isRecording = false;
    m_recordDurationTimer->stop();
    m_recordDurationLabel->setVisible(false);
    m_recordButton->setChecked(false);
    m_recordButton->setText("REC GLOBAL");
    m_actionOpenMp4->setEnabled(true);

    // A recorder that already stopped (failed to start, device lost mid-take)
    // will not emit StoppedState again: validate its take now.
    const QList<int> pending = m_pendingTakeChecks;
    for (int i : pending) {
      if (m_audioRecorders[i]->recorderState() == QMediaRecorder::StoppedState) {
        checkRecordedTake(i);
      }
    }
    // Other previews reload via recorderStateChanged once each WAV is finalized
    refreshPreviewSources();
  }
}

void MainWindow::checkRecordedTake(int trackIndex) {
  if (!m_pendingTakeChecks.removeOne(trackIndex) || trackIndex >= m_hasRecording.size()) {
    return;
  }

  // A WAV header alone is 44 bytes: a file this small holds no audio
  const QFileInfo take(m_tempAudioPaths.value(trackIndex));
  m_hasRecording[trackIndex] = take.exists() && take.size() > 1024;
  if (!m_hasRecording[trackIndex]) {
    m_failedTakeTracks.append(trackIndex);
  }
  if (!m_pendingTakeChecks.isEmpty()) {
    return;
  }

  // Show post-recording notification bar (replaces showExportDialog)
  QString message = tr("✅ Enregistrement terminé !");
  if (!m_failedTakeTracks.isEmpty()) {
    std::sort(m_failedTakeTracks.begin(), m_failedTakeTracks.end());
    QStringList trackNumbers;
    for (int track : m_failedTakeTracks) {
      trackNumbers << QString::number(track + 1);
    }
    message = m_failedTakeTracks.size() == 1
        ? tr("Enregistrement terminé, mais la piste %1 n'a produit aucun audio. "
             "Vérifiez le microphone sélectionné.").arg(trackNumbers.first())
        : tr("Enregistrement terminé, mais les pistes %1 n'ont produit aucun audio. "
             "Vérifiez le microphone sélectionné.").arg(trackNumbers.join(", "));
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

  applyShortcuts();
}

void MainWindow::applyShortcuts() {
  SettingsManager &sm = SettingsManager::instance();
  m_shRecordStart->setKey(sm.shortcut("record_start"));
  m_shRecordStop->setKey(sm.shortcut("record_stop"));
  m_shProjectSave->setKey(sm.shortcut("project_save"));
  m_shProjectSaveAs->setKey(sm.shortcut("project_save_as"));

  m_shortcutPlayPause = sm.shortcut("video_play_pause");
  m_shortcutFrameBack = sm.shortcut("video_frame_back");
  m_shortcutFrameForward = sm.shortcut("video_frame_forward");
  m_shortcutSeekBack5s = sm.shortcut("video_seek_back_5s");
  m_shortcutSeekForward5s = sm.shortcut("video_seek_forward_5s");
  m_shortcutVolumeUp = sm.shortcut("audio_volume_up");
  m_shortcutVolumeDown = sm.shortcut("audio_volume_down");
  m_shortcutVolumeMute = sm.shortcut("audio_volume_mute");
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

void MainWindow::showExportDialog() {
  QString currentVideo = m_currentVideoPath;
  if (currentVideo.isEmpty()) {
    QMessageBox::warning(this, tr("Export"), tr("Aucune vidéo chargée."));
    return;
  }

  QString ffmpegError;
  if (!ExportService::isFFmpegAvailable(&ffmpegError)) {
    QMessageBox::warning(this, tr("Export"), ffmpegError);
    return;
  }
  
  QVector<int> recordedTracks;
  QStringList missingTracks;
  for (int i = 0; i < m_trackCount; ++i) {
    if (!m_hasRecording.value(i, false)) {
      continue;
    }
    if (i < m_tempAudioPaths.size() && !m_tempAudioPaths[i].isEmpty() &&
        QFile::exists(m_tempAudioPaths[i])) {
      recordedTracks.append(i);
    } else {
      // Take marked as recorded but its WAV is gone (failed recording, failed copy on load)
      missingTracks.append(tr("Piste %1").arg(i + 1));
    }
  }
  if (recordedTracks.isEmpty()) {
    QMessageBox::warning(this, tr("Export"), tr("Aucun enregistrement audio trouvé à exporter."));
    return;
  }
  if (!missingTracks.isEmpty()) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Export"),
        tr("Le fichier audio est introuvable pour : %1.\n"
           "Exporter sans ces pistes ?").arg(missingTracks.join(", ")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      return;
    }
  }

  // Export-local renumbering: the first track with a take becomes the primary audio,
  // so an unarmed track 1 does not block the export. All lists follow recordedTracks order.
  QStringList extraAudios;
  QVector<qint64> trackOffsetsMs;
  QVector<float> currentTrackVolumes;
  QVector<bool> currentTrackMutes;
  QVector<int> trackNumbers;
  for (int k = 0; k < recordedTracks.size(); ++k) {
    const int track = recordedTracks[k];
    if (k > 0) {
      extraAudios.append(m_tempAudioPaths[track]);
    }
    trackOffsetsMs.append(m_trackRecordStartMs.value(track, 0));
    const float vol = track < m_trackPanels.size()
        ? m_trackPanels[track]->currentVolume() / 100.0f : 1.0f;
    currentTrackVolumes.append(vol);
    currentTrackMutes.append(vol < 0.01f);
    trackNumbers.append(track + 1);
  }

  float originalVol = m_playbackEngine->volume();
  bool originalMuted = m_volumeMuteButton->isChecked();

  ExportDialog dialog(
      currentVideo,
      m_tempAudioPaths[recordedTracks.first()],
      extraAudios,
      m_lastRecordedDurationMs,
      m_lastRecordedStartMs,
      trackOffsetsMs,
      originalMuted ? 0.0f : originalVol,
      currentTrackVolumes,
      currentTrackMutes,
      trackNumbers,
      this
  );

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
  QMessageBox::critical(this, tr("Erreur"), errorMessage);
}

// =============================================================================
// Event Handling
// =============================================================================

bool MainWindow::event(QEvent *event) {
  // With nothing to stop, the record_stop key (Escape by default) goes to the
  // focused widget instead of the application-wide shortcut: RythmoWidget uses
  // Escape to push the text.
  if (event->type() == QEvent::ShortcutOverride && !m_isRecording &&
      !m_countdownTimer->isActive()) {
    const QKeyCombination combo = static_cast<QKeyEvent *>(event)->keyCombination();
    if (QKeySequence(combo) == m_shRecordStop->key()) {
      event->accept();
      return true;
    }
  }
  return QMainWindow::event(event);
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
  // Escape in fullscreen recording: stop recording + exit fullscreen
  if (event->key() == Qt::Key_Escape && m_isFullscreenRecording &&
      m_isRecording) {
    toggleRecording();
    event->accept();
    return;
  }

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
      if (m_playbackEngine->playbackState() == QMediaPlayer::PlayingState) {
        m_playbackEngine->pause();
      } else {
        m_playbackEngine->play();
      }
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
        if (m_trackPanels[i]->currentInputDevice() == "Aucune entrée" || m_trackPanels[i]->currentInputDevice().isEmpty()) {
          m_trackPanels[i]->setInputDevice(defaultMic);
        }
      }
    }

    // Apply shortcuts
    applyShortcuts();
    
    statusBar()->showMessage(tr("Paramètres mis à jour"), 3000);
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
  QString dateStr = QLocale::system().toString(dt, QLocale::ShortFormat);

  QMessageBox msgBox(QMessageBox::Question,
                     tr("Récupération"),
                     tr("Une sauvegarde automatique du %1 a été trouvée. "
                        "DubInstante s'est probablement fermé de façon inattendue.\n"
                        "Voulez-vous restaurer ce travail ?")
                         .arg(dateStr),
                     QMessageBox::NoButton,
                     this);

  QPushButton *restoreBtn =
      msgBox.addButton(tr("Restaurer"), QMessageBox::AcceptRole);
  QPushButton *ignoreBtn =
      msgBox.addButton(tr("Ignorer"), QMessageBox::DestructiveRole);
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
            tr("Projet restauré. %1 enregistrement(s) introuvable(s).")
                .arg(m_lastLoadLostTracksCount),
            8000);
      } else {
        statusBar()->showMessage(tr("Projet restauré"), 3000);
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
      this, tr("Modifications non enregistrées"),
      tr("Le projet a été modifié. Voulez-vous enregistrer avant de continuer ?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);

  if (reply == QMessageBox::Cancel) {
    return false;
  }
  if (reply == QMessageBox::Discard) {
    return true;
  }

  onSaveProject();
  // ponytail: l'archive .zip est écrite en tâche de fond, m_isDirty ne retombe
  // qu'à la fin du QFutureWatcher. La fermeture est donc annulée et l'utilisateur
  // la relance une fois la barre de progression terminée. Passer à une fermeture
  // différée si ce détour devient gênant.
  return !m_isDirty;
}

void MainWindow::cleanupTempAudioFiles() {
  for (const QString &path : m_tempAudioPaths) {
    QFile::remove(path);
  }
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

  SaveData data = collectSaveData();

  // Autosave keeps the temp WAV paths: takes are recoverable without a copy pass
  for (int i = 0; i < data.audioTracks.size(); ++i) {
    if (data.audioTracks[i].hasRecording) {
      data.audioTracks[i].audioFilePath = m_tempAudioPaths.value(i);
    }
  }

  if (m_saveManager->save(autosaveFile, data)) {
    statusBar()->showMessage(tr("Sauvegarde automatique cache effectuée"), 2000);
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

  // Sync preview outputs to the same device
  for (QAudioOutput *out : m_previewOutputs) {
      out->setDevice(targetDevice);
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

  statusBar()->showMessage(tr("Sortie audio : %1").arg(action->text()), 3000);
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
    QAction *emptyAction = m_audioMenu->addAction(tr("Aucun profil configuré"));
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
  QAction *configAction = m_audioMenu->addAction(tr("Gérer les sorties..."));
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
    m_recordButton->setText("● REC GLOBAL");
    QMessageBox::warning(this, tr("Enregistrement"),
                         tr("Aucune piste n'est armée. Armez au moins une piste avant "
                            "de lancer l'enregistrement."));
    return;
  }

  for (int i = 0; i < m_trackCount; ++i) {
    if (m_trackPanels[i]->isArmed()) {
      QString dev = m_trackPanels[i]->currentInputDevice();
      if (dev.isEmpty() || dev == "Aucune entrée" || dev == tr("Aucune entrée")) {
        if (m_countdownLabel) {
          m_countdownLabel->hide();
        }
        m_recordButton->setChecked(false);
        m_recordButton->setText("● REC GLOBAL");
        QMessageBox::warning(this, tr("Enregistrement"),
                             tr("La piste %1 est armée mais aucun microphone n'est sélectionné.")
                                 .arg(i + 1));
        return;
      }
    }
  }

  // Results of a previous take still finalizing no longer apply
  m_pendingTakeChecks.clear();
  m_failedTakeTracks.clear();

  // Record from the current playhead position (punch-in support)
  m_recordingStartTimeMs = m_playbackEngine->position();

  for (int i = 0; i < m_trackCount; ++i) {
    if (m_trackPanels[i]->isArmed()) {
      // --- ARMED: Record this track ---
      // Step 1: Release any existing preview file handle
      releasePreviewSource(i);
      if (QFile::exists(m_tempAudioPaths[i])) {
        QFile::remove(m_tempAudioPaths[i]);
      }
      m_hasRecording[i] = false;

      // Step 2: Start recording
      m_audioRecorders[i]->startRecording(
          QUrl::fromLocalFile(m_tempAudioPaths[i]));

      // Step 3: Store per-track metadata
      m_trackRecordStartMs[i] = m_recordingStartTimeMs;

      // Step 4: Visual feedback
      m_trackPanels[i]->setRecordingState("recording");

    } else if (m_hasRecording.value(i, false)) {
      // --- UNARMED with existing recording: Play back old take in sync ---
      // The take lives on the master timeline at [start, start+duration]; only
      // start it now if the punch-in point already falls inside that window.
      // handlePreviewSync() will start it later otherwise.
      qint64 startMs = m_trackRecordStartMs.value(i, 0);
      qint64 durMs = m_trackRecordDurationMs.value(i, 0);
      qint64 targetPos = m_recordingStartTimeMs - startMs;

      m_previewPlayers[i]->setSource(
          QUrl::fromLocalFile(m_tempAudioPaths[i]));
      if (targetPos >= 0 && (durMs <= 0 || targetPos < durMs)) {
        m_previewPlayers[i]->setPosition(targetPos);
        m_previewPlayers[i]->play();
      }

      // Visual feedback
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
  m_recordButton->setText("STOP");
  m_exportProgressBar->setVisible(false);
  m_actionOpenMp4->setEnabled(false);
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
        this, tr("Export en cours"),
        tr("Un export est en cours. Quitter maintenant l'interrompra et "
           "supprimera le fichier incomplet.\nQuitter quand même ?"),
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
