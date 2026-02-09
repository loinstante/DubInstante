/**
 * @file MainWindow.cpp
 * @brief Implementation of the MainWindow class.
 */

#include "MainWindow.h"

// Core includes
#include "AudioRecorder.h"
#include "ExportService.h"
#include "PlaybackEngine.h"
#include "RythmoManager.h"

// GUI includes
#include "ClickableSlider.h"
#include "RythmoOverlay.h"
#include "TrackPanel.h"
#include "VideoWidget.h"

// Utils includes
#include "TimeFormatter.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      // Initialize Core services
      ,
      m_playbackEngine(new PlaybackEngine(this)),
      m_rythmoManager(new RythmoManager(this)),
      m_audioRecorder1(new AudioRecorder(this)),
      m_audioRecorder2(new AudioRecorder(this)),
      m_exportService(new ExportService(this))
      // Initialize state
      ,
      m_previousVolume(100), m_isRecording(false), m_lastRecordedDurationMs(0),
      m_recordingStartTimeMs(0) {
  loadStylesheet();
  setupUi();
  setupConnections();

  // Connect video sink
  m_playbackEngine->setVideoSink(m_videoWidget->videoSink());

  // Setup temporary file paths
  QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  m_tempAudioPath1 = tempDir + "/temp_dub.wav";
  m_tempAudioPath2 = tempDir + "/temp_dub_2.wav";

  // Window configuration
  setWindowTitle("DubInstante - Studio");
  resize(900, 600);
  setMinimumSize(800, 500);
}

// =============================================================================
// UI Setup
// =============================================================================

void MainWindow::loadStylesheet() {
  QFile styleFile(":/resources/style.qss");
  if (styleFile.open(QFile::ReadOnly)) {
    QString styleSheet = QLatin1String(styleFile.readAll());
    setStyleSheet(styleSheet);
  }
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(5);

  // =========================================================================
  // Video Area with Overlay
  // =========================================================================

  QFrame *videoFrame = new QFrame(this);
  videoFrame->setObjectName("videoFrame");
  videoFrame->setFrameStyle(QFrame::NoFrame);
  videoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_videoWidget = new VideoWidget(videoFrame);
  m_videoWidget->show();

  m_rythmoOverlay = new RythmoOverlay(videoFrame);
  m_rythmoOverlay->show();

  QVBoxLayout *playerContainerLayout = new QVBoxLayout();
  playerContainerLayout->setContentsMargins(0, 0, 0, 0);
  playerContainerLayout->setSpacing(0);
  playerContainerLayout->addWidget(videoFrame, 1);

  mainLayout->addLayout(playerContainerLayout, 1);

  // Watch for resize events
  videoFrame->installEventFilter(this);

  // =========================================================================
  // Position Slider
  // =========================================================================

  m_positionSlider = new ClickableSlider(Qt::Horizontal, this);
  m_positionSlider->setRange(0, 0);
  mainLayout->addWidget(m_positionSlider);

  // =========================================================================
  // Playback Controls
  // =========================================================================

  QHBoxLayout *controlsLayout = new QHBoxLayout();
  controlsLayout->setSpacing(10);

  m_openButton =
      new QPushButton(QIcon(":/resources/icons/folder_open.svg"), "", this);
  m_openButton->setFixedSize(24, 24);
  m_openButton->setFlat(true);
  controlsLayout->addWidget(m_openButton);

  m_playPauseButton =
      new QPushButton(QIcon(":/resources/icons/play.svg"), "", this);
  m_playPauseButton->setFixedSize(36, 36);
  m_playPauseButton->setIconSize(QSize(24, 24));
  controlsLayout->addWidget(m_playPauseButton);

  m_stopButton = new QPushButton(QIcon(":/resources/icons/stop.svg"), "", this);
  m_stopButton->setFixedSize(36, 36);
  m_stopButton->setIconSize(QSize(24, 24));
  controlsLayout->addWidget(m_stopButton);

  m_timeLabel = new QLabel("00:00 / 00:00", this);
  m_timeLabel->setStyleSheet(
      "color: #666; font-family: monospace; font-weight: bold;");
  controlsLayout->addWidget(m_timeLabel);

  controlsLayout->addStretch();

  // Volume controls
  m_volumeButton =
      new QPushButton(QIcon(":/resources/icons/arrow_up.svg"), "", this);
  m_volumeButton->setFixedSize(24, 24);
  m_volumeButton->setFlat(true);
  controlsLayout->addWidget(m_volumeButton);

  m_volumeSlider = new ClickableSlider(Qt::Horizontal, this);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  m_volumeSlider->setFixedWidth(100);
  controlsLayout->addWidget(m_volumeSlider);

  m_volumeSpinBox = new QSpinBox(this);
  m_volumeSpinBox->setRange(0, 100);
  m_volumeSpinBox->setValue(100);
  m_volumeSpinBox->setFixedWidth(90);
  m_volumeSpinBox->setAlignment(Qt::AlignRight);
  m_volumeSpinBox->setSuffix("%");
  controlsLayout->addWidget(m_volumeSpinBox);

  m_recordButton =
      new QPushButton(QIcon(":/resources/icons/record.svg"), "REC", this);
  m_recordButton->setObjectName("recordButton");
  m_recordButton->setCheckable(true);
  m_recordButton->setFixedSize(90, 36);
  m_recordButton->setIconSize(QSize(16, 16));
  m_recordButton->setCursor(Qt::PointingHandCursor);
  controlsLayout->addWidget(m_recordButton);

  mainLayout->addLayout(controlsLayout);

  // =========================================================================
  // Bottom Controls (Tracks + Settings)
  // =========================================================================

  QHBoxLayout *bottomControlsLayout = new QHBoxLayout();

  // Tracks column
  QVBoxLayout *tracksLayout = new QVBoxLayout();
  tracksLayout->setSpacing(5);

  m_track1Panel = new TrackPanel("Piste 1", m_audioRecorder1, this);
  tracksLayout->addWidget(m_track1Panel);

  m_track2Container = new QWidget(this);
  QHBoxLayout *track2ContainerLayout = new QHBoxLayout(m_track2Container);
  track2ContainerLayout->setContentsMargins(0, 0, 0, 0);
  m_track2Panel =
      new TrackPanel("Piste 2", m_audioRecorder2, m_track2Container);
  track2ContainerLayout->addWidget(m_track2Panel);
  m_track2Container->setVisible(false);
  tracksLayout->addWidget(m_track2Container);

  m_enableTrack2Check = new QCheckBox("Activer Piste 2", this);
  tracksLayout->addWidget(m_enableTrack2Check);

  bottomControlsLayout->addLayout(tracksLayout);
  bottomControlsLayout->addStretch();

  // Speed controls column
  QVBoxLayout *speedLayout = new QVBoxLayout();
  speedLayout->setSpacing(2);
  speedLayout->addWidget(new QLabel("Vitesse Défilement:", this));

  m_speedSpinBox = new QSpinBox(this);
  m_speedSpinBox->setRange(1, 400);
  m_speedSpinBox->setValue(100);
  m_speedSpinBox->setSuffix("%");
  m_speedSpinBox->setFixedWidth(90);
  m_speedSpinBox->setAlignment(Qt::AlignRight);
  m_speedSpinBox->setSingleStep(10);
  speedLayout->addWidget(m_speedSpinBox);

  m_textColorCheck = new QCheckBox("Texte Blanc", this);
  speedLayout->addWidget(m_textColorCheck);

  bottomControlsLayout->addLayout(speedLayout);

  bottomControlsLayout->addSpacing(20);

  m_exportProgressBar = new QProgressBar(this);
  m_exportProgressBar->setVisible(false);
  bottomControlsLayout->addWidget(m_exportProgressBar);

  mainLayout->addLayout(bottomControlsLayout);

  // Initial sync
  m_rythmoOverlay->setSpeed(m_speedSpinBox->value());
  m_rythmoManager->setSpeed(m_speedSpinBox->value());
  m_rythmoManager->setText(0, ""); // Initialize track 1
  m_rythmoManager->setText(
      1, "TRACK 2: Ready for dubbing..."); // Initialize track 2
}

void MainWindow::setupConnections() {
  // =========================================================================
  // File Operations
  // =========================================================================

  connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);

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

  // PlaybackEngine -> RythmoManager -> RythmoOverlay
  connect(m_playbackEngine, &PlaybackEngine::positionChanged, m_rythmoManager,
          &RythmoManager::sync);
  connect(m_playbackEngine, &PlaybackEngine::positionChanged, m_rythmoOverlay,
          &RythmoOverlay::sync);
  connect(m_playbackEngine, &PlaybackEngine::playbackStateChanged, this,
          [this](QMediaPlayer::PlaybackState state) {
            m_rythmoOverlay->setPlaying(state == QMediaPlayer::PlayingState);
          });

  // =========================================================================
  // RythmoOverlay Interactions -> PlaybackEngine
  // =========================================================================

  connect(m_rythmoOverlay->track1(), &RythmoWidget::seekRequested,
          m_playbackEngine, &PlaybackEngine::seek);
  connect(m_rythmoOverlay->track2(), &RythmoWidget::seekRequested,
          m_playbackEngine, &PlaybackEngine::seek);
  connect(m_rythmoOverlay->track1(), &RythmoWidget::playRequested,
          m_playbackEngine, &PlaybackEngine::play);
  connect(m_rythmoOverlay->track2(), &RythmoWidget::playRequested,
          m_playbackEngine, &PlaybackEngine::play);

  // Text editing: RythmoWidget -> RythmoManager
  connect(m_rythmoOverlay->track1(), &RythmoWidget::characterTyped, this,
          [this](const QString &character) {
            m_rythmoManager->insertCharacter(0, character);
            m_rythmoOverlay->track1()->setText(m_rythmoManager->text(0));
          });
  connect(m_rythmoOverlay->track2(), &RythmoWidget::characterTyped, this,
          [this](const QString &character) {
            m_rythmoManager->insertCharacter(1, character);
            m_rythmoOverlay->track2()->setText(m_rythmoManager->text(1));
          });

  connect(m_rythmoOverlay->track1(), &RythmoWidget::backspacePressed, this,
          [this]() {
            m_rythmoManager->deleteCharacter(0, true);
            m_rythmoOverlay->track1()->setText(m_rythmoManager->text(0));
          });
  connect(m_rythmoOverlay->track2(), &RythmoWidget::backspacePressed, this,
          [this]() {
            m_rythmoManager->deleteCharacter(1, true);
            m_rythmoOverlay->track2()->setText(m_rythmoManager->text(1));
          });

  connect(m_rythmoOverlay->track1(), &RythmoWidget::deletePressed, this,
          [this]() {
            m_rythmoManager->deleteCharacter(0, false);
            m_rythmoOverlay->track1()->setText(m_rythmoManager->text(0));
          });
  connect(m_rythmoOverlay->track2(), &RythmoWidget::deletePressed, this,
          [this]() {
            m_rythmoManager->deleteCharacter(1, false);
            m_rythmoOverlay->track2()->setText(m_rythmoManager->text(1));
          });

  // Navigation (frame stepping via RythmoWidget arrow keys)
  qreal fps = m_playbackEngine->videoFrameRate();
  int frameStep = (fps > 0) ? static_cast<int>(1000.0 / fps) : 40;
  connect(m_rythmoOverlay->track1(), &RythmoWidget::navigationRequested, this,
          [this, frameStep](bool forward) {
            qint64 delta = forward ? frameStep : -frameStep;
            m_playbackEngine->seek(m_playbackEngine->position() + delta);
          });
  connect(m_rythmoOverlay->track2(), &RythmoWidget::navigationRequested, this,
          [this, frameStep](bool forward) {
            qint64 delta = forward ? frameStep : -frameStep;
            m_playbackEngine->seek(m_playbackEngine->position() + delta);
          });

  // =========================================================================
  // Position Slider
  // =========================================================================

  connect(m_positionSlider, &QSlider::sliderMoved, m_playbackEngine,
          &PlaybackEngine::seek);

  // Frame stepping configuration
  connect(m_playbackEngine, &PlaybackEngine::metaDataChanged, this, [this]() {
    qreal fps = m_playbackEngine->videoFrameRate();
    if (fps > 0) {
      int frameDurationMs = static_cast<int>(1000.0 / fps);
      m_positionSlider->setSingleStep(frameDurationMs);
      m_positionSlider->setPageStep(frameDurationMs * 10);
    }
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
    if (value > 0) {
      m_previousVolume = value;
    }
  });

  connect(m_volumeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            m_playbackEngine->setVolume(static_cast<float>(value) / 100.0f);
            if (m_volumeSlider->value() != value) {
              m_volumeSlider->blockSignals(true);
              m_volumeSlider->setValue(value);
              m_volumeSlider->blockSignals(false);
            }
          });

  connect(m_volumeButton, &QPushButton::clicked, this, [this]() {
    if (m_volumeSlider->value() > 0) {
      m_previousVolume = m_volumeSlider->value();
      m_volumeSlider->setValue(0);
    } else {
      m_volumeSlider->setValue(m_previousVolume);
    }
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

  connect(m_textColorCheck, &QCheckBox::toggled, this, [this](bool checked) {
    QColor color = checked ? QColor(Qt::white) : QColor(34, 34, 34);
    m_rythmoOverlay->setTextColor(color);
  });

  // =========================================================================
  // Track 2 Toggle
  // =========================================================================

  connect(m_enableTrack2Check, &QCheckBox::toggled, this, [this](bool checked) {
    m_track2Container->setVisible(checked);
    m_rythmoOverlay->setTrack2Visible(checked);
  });

  // =========================================================================
  // Recording
  // =========================================================================

  connect(m_recordButton, &QPushButton::clicked, this,
          &MainWindow::toggleRecording);
  connect(m_audioRecorder1, &AudioRecorder::errorOccurred, this,
          &MainWindow::onError);
  connect(m_audioRecorder2, &AudioRecorder::errorOccurred, this,
          &MainWindow::onError);

  // =========================================================================
  // Export
  // =========================================================================

  connect(m_exportService, &ExportService::progressChanged, this,
          &MainWindow::onExportProgress);
  connect(m_exportService, &ExportService::exportFinished, this,
          &MainWindow::onExportFinished);
}

// =============================================================================
// Slots - File Operations
// =============================================================================

void MainWindow::onOpenFile() {
  QString fileName = QFileDialog::getOpenFileName(this, tr("Ouvrir"), "",
                                                  tr("Vidéos MP4 (*.mp4)"));

  if (!fileName.isEmpty()) {
    m_playbackEngine->openFile(QUrl::fromLocalFile(fileName));
    setProperty("currentVideoPath", fileName);
  }
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
  } else {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/play.svg"));
  }
}

// =============================================================================
// Slots - Recording
// =============================================================================

void MainWindow::toggleRecording() {
  if (!m_isRecording) {
    QString currentVideo = property("currentVideoPath").toString();
    if (currentVideo.isEmpty()) {
      QMessageBox::warning(this, tr("Dubbing"),
                           tr("Chargez une vidéo avant d'enregistrer."));
      m_recordButton->setChecked(false);
      return;
    }

    m_playbackEngine->seek(0);
    m_recordingStartTimeMs = m_playbackEngine->position();

    m_track1Panel->startRecording(QUrl::fromLocalFile(m_tempAudioPath1));

    if (m_enableTrack2Check->isChecked()) {
      m_track2Panel->startRecording(QUrl::fromLocalFile(m_tempAudioPath2));
    }

    m_playbackEngine->play();
    m_recordingTimer.start();

    m_isRecording = true;
    m_recordButton->setText("STOP");
    m_exportProgressBar->setVisible(false);
    m_openButton->setEnabled(false);
    m_enableTrack2Check->setEnabled(false);

  } else {
    m_playbackEngine->pause();
    m_track1Panel->stopRecording();

    if (m_enableTrack2Check->isChecked()) {
      m_track2Panel->stopRecording();
    }

    m_lastRecordedDurationMs = m_recordingTimer.elapsed();

    m_isRecording = false;
    m_recordButton->setChecked(false);
    m_recordButton->setText("REC");
    m_openButton->setEnabled(true);
    m_enableTrack2Check->setEnabled(true);

    // Prompt for save location
    QString currentVideo = property("currentVideoPath").toString();
    QString outputFile = QFileDialog::getSaveFileName(
        this, tr("Sauvegarder le doublage"),
        QDir::homePath() + "/dub_result.mp4", tr("Video (*.mp4)"));

    if (!outputFile.isEmpty()) {
      m_exportProgressBar->setVisible(true);
      m_exportProgressBar->setValue(0);

      ExportConfig config;
      config.videoPath = currentVideo;
      config.audioPath = m_tempAudioPath1;
      config.outputPath = outputFile;
      config.durationMs = m_lastRecordedDurationMs;
      config.startTimeMs = m_recordingStartTimeMs;
      config.originalVolume = m_playbackEngine->volume();

      if (m_enableTrack2Check->isChecked()) {
        config.secondAudioPath = m_tempAudioPath2;
      }

      m_exportService->startExport(config);
    }
  }
}

// =============================================================================
// Slots - Export
// =============================================================================

void MainWindow::onExportProgress(int percentage) {
  m_exportProgressBar->setValue(percentage);
}

void MainWindow::onExportFinished(bool success, const QString &message) {
  m_exportProgressBar->setVisible(false);

  if (success) {
    QMessageBox::information(this, tr("Export"), message);
  } else {
    QMessageBox::critical(this, tr("Export"), message);
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched->objectName() == "videoFrame" &&
      event->type() == QEvent::Resize) {
    QFrame *frame = qobject_cast<QFrame *>(watched);
    if (frame) {
      if (m_videoWidget) {
        m_videoWidget->setGeometry(0, 0, frame->width(), frame->height());
      }
      if (m_rythmoOverlay) {
        m_rythmoOverlay->setGeometry(0, 0, frame->width(), frame->height());
        m_rythmoOverlay->raise();
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  // Global Play/Pause via Space
  if (event->key() == Qt::Key_Space) {
    if (m_playbackEngine->playbackState() == QMediaPlayer::PlayingState) {
      m_playbackEngine->pause();
    } else {
      m_playbackEngine->play();
    }
    event->accept();
    return;
  }

  // Frame-by-frame navigation
  qreal fps = m_playbackEngine->videoFrameRate();
  int frameStep = (fps > 0) ? static_cast<int>(1000.0 / fps) : 40;

  if (event->key() == Qt::Key_Left) {
    // Only intercept if we are not in an input widget
    if (!focusWidget() || !focusWidget()->inherits("QAbstractSpinBox")) {
      m_playbackEngine->seek(m_playbackEngine->position() - frameStep);
      event->accept();
      return;
    }
  } else if (event->key() == Qt::Key_Right) {
    // Only intercept if we are not in an input widget
    if (!focusWidget() || !focusWidget()->inherits("QAbstractSpinBox")) {
      m_playbackEngine->seek(m_playbackEngine->position() + frameStep);
      event->accept();
      return;
    }
  }
}
else {
  QMainWindow::keyPressEvent(event);
}
}
