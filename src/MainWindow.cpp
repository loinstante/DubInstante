#include "MainWindow.h"
#include "TrackPanel.h"
#include <QAudioDevice>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>
#include <QVariant>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_playerController(new PlayerController(this)),
      m_previousVolume(100),
      m_rythmoOverlay(new RythmoOverlay(
          this)), // Parented initially to this, reparented in setupUi
      m_recorderManager(new AudioRecorderManager(this)),
      m_recorderManager2(new AudioRecorderManager(this)), // Track 2 Recorder
      m_exporter(new Exporter(this)) {
  // Load styling using Qt resource system
  QFile styleFile(":/resources/style.qss");
  if (styleFile.open(QFile::ReadOnly)) {
    QString styleSheet = QLatin1String(styleFile.readAll());
    setStyleSheet(styleSheet);
  }

  setupUi();
  // setupTrack2() removed - logic now in TrackPanel via setupUi
  setupConnections();

  // Link the controller to the widget's sink
  m_playerController->setVideoSink(m_videoWidget->videoSink());

  m_tempAudioPath =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      "/temp_dub.wav";

  m_tempAudioPath2 =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      "/temp_dub_2.wav";

  // 0. General Window Settings
  setWindowTitle("DubInstante - Studio");
  resize(900, 600); // Even smaller default
  setMinimumSize(800, 500);
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(5, 5, 5, 5); // Tight Pro margins
  mainLayout->setSpacing(5);                  // Tight Pro spacing

  // 1. Header Removed - Open Button moved to toolbar
  // ...

  // 2. Video Area with Manual Overlay Positioning
  QFrame *videoFrame = new QFrame(this);
  videoFrame->setObjectName("videoFrame");
  videoFrame->setFrameStyle(QFrame::NoFrame);
  videoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // VideoWidget as direct child, no layout (manual positioning)
  m_videoWidget = new VideoWidget(videoFrame);
  m_videoWidget->show();

  // Wrapper Layout for the video frame
  QVBoxLayout *playerContainerLayout = new QVBoxLayout();
  playerContainerLayout->setContentsMargins(0, 0, 0, 0);
  playerContainerLayout->setSpacing(0);

  playerContainerLayout->addWidget(videoFrame, 1);

  // Rythmo Overlay (OOP)
  // Reparent to videoFrame so it sits on top of VideoWidget
  m_rythmoOverlay->setParent(videoFrame);
  m_rythmoOverlay->show();

  // Note: Geometry managed by eventFilter resizing

  mainLayout->addLayout(playerContainerLayout, 1);

  // Watch for resize to reposition both VideoWidget and RythmoWidget
  videoFrame->installEventFilter(this);

  // 3. Position Slider
  m_positionSlider = new ClickableSlider(Qt::Horizontal, this);
  m_positionSlider->setRange(0, 0);
  mainLayout->addWidget(m_positionSlider);

  // 4. Playback Controls Row
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
  m_volumeSpinBox->setFixedWidth(70);            // Compact, no wasted space
  m_volumeSpinBox->setAlignment(Qt::AlignRight); // Align next to buttons
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

  m_recorderManager = new AudioRecorderManager(this);
  m_recorderManager2 = new AudioRecorderManager(this);

  m_speedSpinBox = new QSpinBox(this);
  m_speedSpinBox->setRange(1, 400);
  m_speedSpinBox->setValue(100);
  m_speedSpinBox->setSuffix("%");
  m_speedSpinBox->setFixedWidth(70);
  m_speedSpinBox->setAlignment(Qt::AlignRight);
  m_speedSpinBox->setSingleStep(10);

  m_exportProgressBar = new QProgressBar(this);
  m_exportProgressBar->setVisible(false);

  // 5. Bottom Controls Layout
  QHBoxLayout *bottomControlsLayout = new QHBoxLayout();
  // =========================================================
  // TRACK 1 CONTROLS
  // =========================================================
  m_track1Panel = new TrackPanel("Piste 1", m_recorderManager, this);

  // =========================================================
  // TRACK 2 CONTROLS (Hidden by default)
  // =========================================================
  m_track2Container = new QWidget(this);
  QHBoxLayout *track2ContainerLayout = new QHBoxLayout(m_track2Container);
  track2ContainerLayout->setContentsMargins(0, 0, 0, 0);
  track2ContainerLayout->setSpacing(0);

  m_track2Panel =
      new TrackPanel("Piste 2", m_recorderManager2, m_track2Container);
  track2ContainerLayout->addWidget(m_track2Panel);

  m_track2Container->setVisible(false); // Hidden initially

  // =========================================================
  // MAIN BOTTOM LAYOUT
  // =========================================================
  // Left: Tracks. Right: Global Settings (Speed, Export)
  QVBoxLayout *tracksLayout = new QVBoxLayout();
  tracksLayout->setSpacing(5);
  tracksLayout->addWidget(m_track1Panel);
  tracksLayout->addWidget(m_track2Container);

  // Checkbox near Start/Stop or near tracks? Near tracks.
  m_enableTrack2Check = new QCheckBox("Activer Piste 2", this);
  tracksLayout->addWidget(m_enableTrack2Check);

  bottomControlsLayout->addLayout(tracksLayout);
  bottomControlsLayout->addStretch();

  // Speed Controls
  QVBoxLayout *speedLayout = new QVBoxLayout();
  speedLayout->setSpacing(2);
  speedLayout->addWidget(new QLabel("Vitesse Défilement:", this));

  QHBoxLayout *speedControlRow = new QHBoxLayout();
  // Simplified row: Just label (above) and spinbox
  speedControlRow->addWidget(m_speedSpinBox);
  speedLayout->addLayout(speedControlRow);

  m_textColorCheck = new QCheckBox("Texte Blanc", this);
  speedLayout->addWidget(m_textColorCheck);

  bottomControlsLayout->addLayout(speedLayout);

  bottomControlsLayout->addSpacing(20);
  bottomControlsLayout->addWidget(m_exportProgressBar);

  mainLayout->addLayout(bottomControlsLayout);

  // Initial Sync
  m_rythmoOverlay->setSpeed(m_speedSpinBox->value());
  m_rythmoOverlay->track2()->setText("TRACK 2: Ready for dubbing...");

  connect(m_enableTrack2Check, &QCheckBox::toggled, this, [this](bool checked) {
    m_track2Container->setVisible(checked);
    m_rythmoOverlay->setTrack2Visible(checked);
  });
}

void MainWindow::setupConnections() {
  connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);

  connect(m_playPauseButton, &QPushButton::clicked, this, [this]() {
    if (m_playerController->playbackState() == QMediaPlayer::PlayingState)
      m_playerController->pause();
    else
      m_playerController->play();
  });

  connect(m_stopButton, &QPushButton::clicked, this, [this]() {
    m_playerController->stop();
    if (m_isRecording)
      toggleRecording();
  });

  connect(m_playerController, &PlayerController::positionChanged, this,
          &MainWindow::updatePosition);
  connect(m_playerController, &PlayerController::durationChanged, this,
          &MainWindow::updateDuration);
  connect(m_playerController, &PlayerController::playbackStateChanged, this,
          &MainWindow::updatePlayPauseButton);
  connect(m_playerController, &PlayerController::playbackStateChanged, this,
          [this](QMediaPlayer::PlaybackState state) {
            m_rythmoOverlay->setPlaying(state == QMediaPlayer::PlayingState);
          });

  connect(m_playerController, &PlayerController::positionChanged, this,
          [this](qint64 pos) { m_rythmoOverlay->sync(pos); });

  // Scrubbing from either track
  connect(m_rythmoOverlay->track1(), &RythmoWidget::scrubRequested,
          m_playerController, &PlayerController::seek);
  connect(m_rythmoOverlay->track2(), &RythmoWidget::scrubRequested,
          m_playerController, &PlayerController::seek);

  // Escape key -> Play
  connect(m_rythmoOverlay->track1(), &RythmoWidget::playRequested,
          m_playerController, &PlayerController::play);
  connect(m_rythmoOverlay->track2(), &RythmoWidget::playRequested,
          m_playerController, &PlayerController::play);

  connect(m_positionSlider, &QSlider::sliderMoved, m_playerController,
          &PlayerController::seek);

  connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
    if (m_playerController) {
      m_playerController->setVolume(static_cast<float>(value) / 100.0f);
    }
    if (m_volumeSpinBox && m_volumeSpinBox->value() != value) {
      m_volumeSpinBox->blockSignals(true);
      m_volumeSpinBox->setValue(value);
      m_volumeSpinBox->blockSignals(false);
    }
    if (value > 0)
      m_previousVolume = value;
  });

  connect(m_volumeButton, &QPushButton::clicked, this, [this]() {
    if (m_volumeSlider->value() > 0) {
      m_previousVolume = m_volumeSlider->value();
      m_volumeSlider->setValue(0);
    } else {
      m_volumeSlider->setValue(m_previousVolume);
    }
  });

  // Volume SpinBox -> Player Volume & Slider
  connect(m_volumeSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
    if (m_playerController) {
      m_playerController->setVolume(static_cast<float>(value) / 100.0f);
    }
    if (m_volumeSlider && m_volumeSlider->value() != value) {
      m_volumeSlider->blockSignals(true);
      m_volumeSlider->setValue(value);
      m_volumeSlider->blockSignals(false);
    }
  });

  // Speed spinbox -> RythmoOverlay (Updates both tracks)
  connect(m_speedSpinBox, &QSpinBox::valueChanged, m_rythmoOverlay,
          &RythmoOverlay::setSpeed);

  connect(m_textColorCheck, &QCheckBox::toggled, this, [this](bool checked) {
    QColor color = checked ? QColor(Qt::white) : QColor(34, 34, 34);
    m_rythmoOverlay->setTextColor(color);
  });

  connect(m_recordButton, &QPushButton::clicked, this,
          &MainWindow::toggleRecording);

  connect(m_playerController, &PlayerController::volumeChanged, this,
          [this](float volume) {
            int val = static_cast<int>(volume * 100);
            if (m_volumeSlider && m_volumeSlider->value() != val) {
              m_volumeSlider->blockSignals(true);
              m_volumeSlider->setValue(val);
              m_volumeSlider->blockSignals(false);
            }
            if (m_volumeSpinBox && m_volumeSpinBox->value() != val) {
              m_volumeSpinBox->blockSignals(true);
              m_volumeSpinBox->setValue(val);
              m_volumeSpinBox->blockSignals(false);
            }
          });

  connect(m_playerController, &PlayerController::metaDataChanged, this,
          [this]() {
            qreal fps = m_playerController->videoFrameRate();
            if (fps > 0) {
              int frameDurationMs = static_cast<int>(1000.0 / fps);
              m_positionSlider->setSingleStep(frameDurationMs);
              m_positionSlider->setPageStep(frameDurationMs *
                                            10); // 10 frames per page step
            }
          });

  connect(m_exporter, &Exporter::progressChanged, this,
          &MainWindow::updateExportProgress);
  connect(m_exporter, &Exporter::finished, this, &MainWindow::onExportFinished);

  connect(m_playerController, &PlayerController::errorOccurred, this,
          &MainWindow::handleError);
  connect(m_recorderManager, &AudioRecorderManager::errorOccurred, this,
          &MainWindow::handleError);
}

void MainWindow::toggleRecording() {
  if (!m_isRecording) {
    QString currentVideo = property("currentVideoPath").toString();
    if (currentVideo.isEmpty()) {
      QMessageBox::warning(this, tr("Dubbing"),
                           tr("Chargez une vidéo avant d'enregistrer."));
      m_recordButton->setChecked(false);
      return;
    }

    m_playerController->seek(0);
    // Capture start time for seeking
    m_recordingStartTimeMs = m_playerController->position();

    m_track1Panel->startRecording(QUrl::fromLocalFile(m_tempAudioPath));

    // Track 2 Recording
    if (m_enableTrack2Check->isChecked()) {
      m_track2Panel->startRecording(QUrl::fromLocalFile(m_tempAudioPath2));
    }

    m_playerController->play();

    m_recordingTimer.start();

    m_isRecording = true;
    m_recordButton->setText("STOP"); // Icon is sufficient or change icon?
    // Let's keep it simple: Pressed state = Recording.
    m_exportProgressBar->setVisible(false);
    m_openButton->setEnabled(false);
    m_enableTrack2Check->setEnabled(false);

  } else {
    m_playerController->pause();
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

    QString currentVideo = property("currentVideoPath").toString();
    QString outputFile = QFileDialog::getSaveFileName(
        this, tr("Sauvegarder le doublage"),
        QDir::homePath() + "/dub_result.mp4", tr("Video (*.mp4)"));

    if (!outputFile.isEmpty()) {
      m_exportProgressBar->setVisible(true);
      m_exportProgressBar->setValue(0);
      m_exporter->setTotalDuration(m_lastRecordedDurationMs);
      float currentVolume = m_playerController->volume();

      QString secondTrackPath = "";
      if (m_enableTrack2Check->isChecked()) {
        secondTrackPath = m_tempAudioPath2;
      }

      m_exporter->merge(currentVideo, m_tempAudioPath, outputFile,
                        m_lastRecordedDurationMs, m_recordingStartTimeMs,
                        currentVolume, secondTrackPath);
    }
  }
}

void MainWindow::updateExportProgress(int percentage) {
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

void MainWindow::onOpenFile() {
  QString fileName = QFileDialog::getOpenFileName(this, tr("Ouvrir"), "",
                                                  tr("Vidéos MP4 (*.mp4)"));
  if (!fileName.isEmpty()) {
    m_playerController->openFile(QUrl::fromLocalFile(fileName));
    setProperty("currentVideoPath", fileName);
  }
}

void MainWindow::updatePosition(qint64 position) {
  if (!m_positionSlider->isSliderDown()) {
    m_positionSlider->setValue(static_cast<int>(position));
  }
  m_timeLabel->setText(formatTime(position) + " / " +
                       formatTime(m_playerController->duration()));
}

void MainWindow::updateDuration(qint64 duration) {
  m_positionSlider->setRange(0, static_cast<int>(duration));
}

void MainWindow::updatePlayPauseButton(QMediaPlayer::PlaybackState state) {
  if (state == QMediaPlayer::PlayingState) {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/pause.svg"));
  } else {
    m_playPauseButton->setIcon(QIcon(":/resources/icons/play.svg"));
  }
}

void MainWindow::handleError(const QString &errorMessage) {
  QMessageBox::critical(this, tr("Erreur"), errorMessage);
}

// Helper removed - Layout handles geometry

QString MainWindow::formatTime(qint64 milliseconds) const {
  QTime currentTime(0, 0);
  currentTime = currentTime.addMSecs(static_cast<int>(milliseconds));
  return (milliseconds >= 3600000) ? currentTime.toString("hh:mm:ss")
                                   : currentTime.toString("mm:ss");
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched->objectName() == "videoFrame" &&
      event->type() == QEvent::Resize) {
    QFrame *frame = qobject_cast<QFrame *>(watched);
    if (frame) {
      // Position VideoWidget to fill the entire frame
      if (m_videoWidget)
        m_videoWidget->setGeometry(0, 0, frame->width(), frame->height());

      // Position OverlayWidget to fill the entire frame
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
  // If text field is focused... oh wait, it's gone.
  // RythmoWidget handles its own keys if it has focus.
  // We still want Global Play/Pause via Space?
  if (event->key() == Qt::Key_Space) {
    if (m_playerController->playbackState() == QMediaPlayer::PlayingState)
      m_playerController->pause();
    else
      m_playerController->play();
    event->accept();
    return;
  }

  // Otherwise, handle Frame-by-Frame navigation
  qreal fps = m_playerController->videoFrameRate();
  int frameStep = (fps > 0) ? static_cast<int>(1000.0 / fps) : 40;

  if (event->key() == Qt::Key_Left) {
    m_playerController->seek(m_playerController->position() - frameStep);
    event->accept();
  } else if (event->key() == Qt::Key_Right) {
    m_playerController->seek(m_playerController->position() + frameStep);
    event->accept();
  } else {
    QMainWindow::keyPressEvent(event);
  }
}
