#include "MainWindow.h"
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
      m_previousVolume(100), m_rythmoWidget(new RythmoWidget(this)),
      m_recorderManager(new AudioRecorderManager(this)),
      m_exporter(new Exporter(this)) {
  // Load styling using Qt resource system
  QFile styleFile(":/resources/style.qss");
  if (styleFile.open(QFile::ReadOnly)) {
    QString styleSheet = QLatin1String(styleFile.readAll());
    setStyleSheet(styleSheet);
  }

  setupUi();
  setupConnections();

  // Link the controller to the widget's sink
  m_playerController->setVideoSink(m_videoWidget->videoSink());

  m_tempAudioPath =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      "/temp_dub.wav";

  // 0. General Window Settings
  setWindowTitle("DUBSync - Studio");
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

  // RythmoWidget as overlay on top of VideoWidget
  m_rythmoWidget->setParent(videoFrame);
  m_rythmoWidget->setFixedHeight(70);
  m_rythmoWidget->raise(); // Bring to front
  m_rythmoWidget->show();

  mainLayout->addLayout(playerContainerLayout, 1);

  // Watch for resize to reposition both VideoWidget and RythmoWidget
  videoFrame->installEventFilter(this);

  // 3. Position Slider
  m_positionSlider = new ClickableSlider(Qt::Horizontal, this);
  m_positionSlider->setRange(0, 0);
  mainLayout->addWidget(m_positionSlider);

  // 4. Compact Control Bar
  QString iconButtonStyle =
      "QPushButton { border: 1px solid #ccc; background: #f5f5f5; "
      "border-radius: 3px; min-width: 24px; max-width: 24px; min-height: 24px; "
      "max-height: 24px; color: #333; padding: 0px; }"
      "QPushButton:hover { background: #e5e5e5; border-color: #bbb; }"
      "QPushButton:pressed { background: #ddd; border-color: #0078d7; }";

  m_openButton = new QPushButton(this);
  m_openButton->setIcon(QIcon(":/resources/icons/folder_open.svg"));
  m_openButton->setToolTip("Ouvrir une vidéo");
  m_openButton->setFixedSize(32, 32);
  m_openButton->setStyleSheet(iconButtonStyle);
  m_openButton->setObjectName("openButtonToolbar");

  if (!m_exporter->isFFmpegAvailable()) {
    QMessageBox::critical(this, "Erreur",
                          "FFmpeg n'est pas installé ou introuvable "
                          "!\nL'exportation ne fonctionnera pas.\nVeuillez "
                          "installer FFmpeg (sudo apt install ffmpeg).");
    m_openButton->setEnabled(false);
  }

  QHBoxLayout *controlsLayout = new QHBoxLayout();

  m_playPauseButton = new QPushButton(this);
  m_playPauseButton->setIcon(QIcon(":/resources/icons/play.svg"));
  m_playPauseButton->setIconSize(QSize(20, 20));
  m_playPauseButton->setFixedSize(32, 32);
  m_playPauseButton->setStyleSheet(iconButtonStyle);

  m_stopButton = new QPushButton(this);
  m_stopButton->setIcon(QIcon(":/resources/icons/stop.svg"));
  m_stopButton->setIconSize(QSize(20, 20));
  m_stopButton->setFixedSize(32, 32);
  m_stopButton->setStyleSheet(iconButtonStyle);

  m_timeLabel = new QLabel("00:00 / 00:00", this);
  m_timeLabel->setObjectName("timeLabel");
  m_timeLabel->setStyleSheet(
      "font-size: 11px; font-weight: bold; margin: 0 5px;");

  m_recordButton = new QPushButton("REC", this);
  m_recordButton->setObjectName("recordButton");
  m_recordButton->setIcon(QIcon(":/resources/icons/record.svg"));
  m_recordButton->setIconSize(QSize(16, 16));
  m_recordButton->setCheckable(true);
  m_recordButton->setFixedSize(90, 32); // Pill size
  // Updated style later in QSS
  m_recordButton->setStyleSheet("");

  m_volumeButton = new QPushButton(this);
  m_volumeButton->setIcon(QIcon(":/resources/icons/volume.svg"));
  m_volumeButton->setIconSize(QSize(20, 20));
  m_volumeButton->setFixedSize(32, 32);
  m_volumeButton->setStyleSheet(iconButtonStyle);
  m_volumeSlider = new ClickableSlider(Qt::Horizontal, this);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  m_volumeSlider->setFixedWidth(80);
  m_volumeSlider->setObjectName("volumeSlider");

  m_volumeSpinBox = new QSpinBox(this);
  m_volumeSpinBox->setRange(0, 100);
  m_volumeSpinBox->setValue(100);
  m_volumeSpinBox->setFixedWidth(50);
  m_volumeSpinBox->setSuffix("%");

  controlsLayout->addWidget(m_openButton); // Leftmost
  controlsLayout->addSpacing(10);
  controlsLayout->addWidget(m_playPauseButton);
  controlsLayout->addWidget(m_stopButton);
  controlsLayout->addSpacing(10);
  controlsLayout->addWidget(m_timeLabel);
  controlsLayout->addStretch();
  controlsLayout->addWidget(m_recordButton);
  controlsLayout->addSpacing(20);
  controlsLayout->addWidget(m_volumeButton);
  controlsLayout->addWidget(m_volumeSlider);
  controlsLayout->addWidget(m_volumeSpinBox);

  mainLayout->addLayout(controlsLayout);

  // 5. Bottom Controls Layout (Audio/Speed/Export)
  QHBoxLayout *bottomControlsLayout = new QHBoxLayout();
  bottomControlsLayout->setSpacing(15);
  bottomControlsLayout->setContentsMargins(10, 5, 10, 5);

  m_inputDeviceCombo = new QComboBox(this);
  m_inputDeviceCombo->setMinimumWidth(150);
  m_inputDeviceCombo->addItem("Aucun (NONE)", QVariant()); // Add NONE option
  auto devices = m_recorderManager->availableDevices();
  for (const auto &dev : devices) {
    m_inputDeviceCombo->addItem(dev.description(), QVariant::fromValue(dev));
  }

  m_micVolumeSlider = new ClickableSlider(Qt::Horizontal, this);
  m_micVolumeSlider->setRange(0, 100);
  m_micVolumeSlider->setValue(100);
  m_micVolumeSlider->setFixedWidth(100);

  m_micGainSpinBox = new QSpinBox(this);
  m_micGainSpinBox->setRange(0, 100);
  m_micGainSpinBox->setValue(100);
  m_micGainSpinBox->setFixedWidth(50);
  m_micGainSpinBox->setSuffix("%");

  m_exportProgressBar = new QProgressBar(this);
  m_exportProgressBar->setRange(0, 100);
  m_exportProgressBar->setValue(0);
  m_exportProgressBar->setVisible(false);
  m_exportProgressBar->setFixedHeight(15);
  m_exportProgressBar->setTextVisible(false);

  m_speedSpinBox = new QSpinBox(this);
  m_speedSpinBox->setRange(50, 500); // 50 to 500 px/sec
  m_speedSpinBox->setSingleStep(10);
  m_speedSpinBox->setValue(m_rythmoWidget->speed());
  m_speedSpinBox->setFixedWidth(100);
  m_speedSpinBox->setSuffix(" px/s");

  // Mic Group (Tight)
  QHBoxLayout *micGroup = new QHBoxLayout();
  micGroup->setSpacing(2); // Very tight spacing
  micGroup->addWidget(new QLabel("Mic:", this));
  micGroup->addWidget(m_inputDeviceCombo);

  // Gain Group (Tight)
  QHBoxLayout *gainGroup = new QHBoxLayout();
  gainGroup->setSpacing(2); // Very tight spacing
  gainGroup->addWidget(new QLabel("Gain:", this));
  gainGroup->addWidget(m_micVolumeSlider);
  gainGroup->addWidget(m_micGainSpinBox);

  bottomControlsLayout->addLayout(micGroup);
  bottomControlsLayout->addSpacing(15);
  bottomControlsLayout->addLayout(gainGroup);
  bottomControlsLayout->addStretch();
  bottomControlsLayout->addWidget(new QLabel("Vitesse:", this));

  QPushButton *speedDownBtn = new QPushButton(this);
  speedDownBtn->setIcon(QIcon(":/resources/icons/arrow_left.svg"));
  speedDownBtn->setFixedWidth(32);
  speedDownBtn->setFixedHeight(32);
  speedDownBtn->setStyleSheet(iconButtonStyle);
  connect(speedDownBtn, &QPushButton::clicked, this,
          [this]() { m_speedSpinBox->setValue(m_speedSpinBox->value() - 10); });
  bottomControlsLayout->addWidget(speedDownBtn);

  bottomControlsLayout->addWidget(m_speedSpinBox);

  QPushButton *speedUpBtn = new QPushButton(this);
  speedUpBtn->setIcon(QIcon(":/resources/icons/arrow_right.svg"));
  speedUpBtn->setFixedWidth(32);
  speedUpBtn->setFixedHeight(32);
  speedUpBtn->setStyleSheet(iconButtonStyle);
  connect(speedUpBtn, &QPushButton::clicked, this,
          [this]() { m_speedSpinBox->setValue(m_speedSpinBox->value() + 10); });
  bottomControlsLayout->addWidget(speedUpBtn);

  bottomControlsLayout->addSpacing(20);
  bottomControlsLayout->addWidget(m_exportProgressBar);

  mainLayout->addLayout(bottomControlsLayout);
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
            m_rythmoWidget->setPlaying(state == QMediaPlayer::PlayingState);
          });

  connect(m_playerController, &PlayerController::positionChanged, this,
          [this](qint64 pos) { m_rythmoWidget->sync(pos); });

  connect(m_rythmoWidget, &RythmoWidget::scrubRequested, m_playerController,
          &PlayerController::seek);

  // Escape key -> Play
  connect(m_rythmoWidget, &RythmoWidget::playRequested, m_playerController,
          &PlayerController::play);

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

  connect(m_inputDeviceCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            QVariant data = m_inputDeviceCombo->itemData(index);
            if (data.isValid()) {
              auto device = data.value<QAudioDevice>();
              m_recorderManager->setDevice(device);
            } else {
              // NONE selected
              m_recorderManager->setDevice(QAudioDevice());
            }
          });

  connect(m_micVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
    m_recorderManager->setVolume(value / 100.0f);
    if (m_micGainSpinBox->value() != value) {
      m_micGainSpinBox->blockSignals(true);
      m_micGainSpinBox->setValue(value);
      m_micGainSpinBox->blockSignals(false);
    }
  });

  connect(m_micGainSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
    if (m_micVolumeSlider->value() != value) {
      m_micVolumeSlider->setValue(value);
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

  // Speed spinbox -> RythmoWidget
  connect(m_speedSpinBox, &QSpinBox::valueChanged, m_rythmoWidget,
          &RythmoWidget::setSpeed);

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
      QMessageBox::warning(this, "Dubbing",
                           "Chargez une vidéo avant d'enregistrer.");
      m_recordButton->setChecked(false);
      return;
    }

    m_playerController->seek(0);
    // Capture start time for seeking
    m_recordingStartTimeMs = m_playerController->position();

    m_recorderManager->startRecording(QUrl::fromLocalFile(m_tempAudioPath));
    m_playerController->play();

    m_recordingTimer.start();

    m_isRecording = true;
    m_recordButton->setText("STOP"); // Icon is sufficient or change icon?
    // Let's keep it simple: Pressed state = Recording.
    m_exportProgressBar->setVisible(false);
    m_openButton->setEnabled(false);

  } else {
    m_playerController->pause();
    m_recorderManager->stopRecording();
    m_lastRecordedDurationMs = m_recordingTimer.elapsed();

    m_isRecording = false;
    m_recordButton->setChecked(false);
    m_recordButton->setText("REC");
    m_openButton->setEnabled(true);

    QString currentVideo = property("currentVideoPath").toString();
    QString outputFile = QFileDialog::getSaveFileName(
        this, "Sauvegarder le doublage", QDir::homePath() + "/dub_result.mp4",
        "Video (*.mp4)");

    if (!outputFile.isEmpty()) {
      m_exportProgressBar->setVisible(true);
      m_exportProgressBar->setValue(0);
      m_exporter->setTotalDuration(m_lastRecordedDurationMs);
      float currentVolume = m_playerController->volume();
      m_exporter->merge(currentVideo, m_tempAudioPath, outputFile,
                        m_lastRecordedDurationMs, m_recordingStartTimeMs,
                        currentVolume);
    }
  }
}

void MainWindow::updateExportProgress(int percentage) {
  m_exportProgressBar->setValue(percentage);
}

void MainWindow::onExportFinished(bool success, const QString &message) {
  m_exportProgressBar->setVisible(false);
  if (success) {
    QMessageBox::information(this, "Export", message);
  } else {
    QMessageBox::critical(this, "Export", message);
  }
}

void MainWindow::onOpenFile() {
  QString fileName =
      QFileDialog::getOpenFileName(this, "Ouvrir", "", "Vidéos MP4 (*.mp4)");
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
  QMessageBox::critical(this, "Erreur", errorMessage);
}

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
    if (frame && m_videoWidget) {
      // Position VideoWidget to fill the entire frame
      m_videoWidget->setGeometry(0, 0, frame->width(), frame->height());

      // Get the actual video content rectangle (respects aspect ratio)
      QRect videoRect = m_videoWidget->videoRect();
      // Position rythmo at the bottom of the actual video, not the container
      int ry = videoRect.bottom() - m_rythmoWidget->height();
      int rx = videoRect.left();
      int rw = videoRect.width();
      m_rythmoWidget->setGeometry(rx, ry, rw, m_rythmoWidget->height());
    }
  }
  return QMainWindow::eventFilter(watched, event);
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
