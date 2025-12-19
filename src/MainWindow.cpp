#include "MainWindow.h"
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_playerController(new PlayerController(this)),
      m_previousVolume(100) {
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

  setWindowTitle("DUBSync - Lecteur Pro");
  resize(1280, 720);
}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(40, 40, 40, 40);
  mainLayout->setSpacing(20);

  // Header: Open Button
  m_openButton = new QPushButton("Ouvrir vidéo MP4", this);
  m_openButton->setObjectName("openButton");
  m_openButton->setCursor(Qt::PointingHandCursor);
  m_openButton->setToolTip("Ouvrir un fichier vidéo (Ctrl+O)");
  m_openButton->setAccessibleName("Ouvrir vidéo");
  m_openButton->setShortcut(QKeySequence("Ctrl+O"));
  m_openButton->setMaximumWidth(200);

  mainLayout->addWidget(m_openButton, 0, Qt::AlignLeft);

  // Video Widget
  m_videoWidget = new VideoWidget(this);
  m_videoWidget->setMinimumSize(640, 360);
  m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mainLayout->addWidget(m_videoWidget, 1);

  // Progress Bar
  m_positionSlider = new QSlider(Qt::Horizontal, this);
  m_positionSlider->setRange(0, 0);
  m_positionSlider->setToolTip("Barre de navigation temporelle");
  m_positionSlider->setAccessibleName("Position vidéo");
  mainLayout->addWidget(m_positionSlider);

  // Controls Row
  QHBoxLayout *controlsLayout = new QHBoxLayout();
  controlsLayout->setContentsMargins(0, 0, 0, 0);

  m_playPauseButton = new QPushButton(this);
  m_playPauseButton->setProperty("class", "controlButton");
  m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  m_playPauseButton->setToolTip("Lecture / Pause (Espace)");
  m_playPauseButton->setAccessibleName("Lecture et Pause");
  m_playPauseButton->setShortcut(QKeySequence("Space"));

  m_stopButton = new QPushButton(this);
  m_stopButton->setProperty("class", "controlButton");
  m_stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
  m_stopButton->setToolTip("Arrêter la vidéo");
  m_stopButton->setAccessibleName("Arrêt");

  m_timeLabel = new QLabel("00:00 / 00:00", this);
  m_timeLabel->setObjectName("timeLabel");

  // Volume Section
  m_volumeButton = new QPushButton(this);
  m_volumeButton->setProperty("class", "controlButton");
  m_volumeButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
  m_volumeButton->setToolTip("Couper le son (M)");
  m_volumeButton->setAccessibleName("Couper/Activer le son");
  m_volumeButton->setShortcut(QKeySequence("M"));
  m_volumeButton->setFixedSize(40, 40);

  m_volumeSlider = new QSlider(Qt::Horizontal, this);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  m_volumeSlider->setFixedWidth(100);
  m_volumeSlider->setToolTip("Ajuster le volume");
  m_volumeSlider->setAccessibleName("Volume");

  controlsLayout->addWidget(m_playPauseButton);
  controlsLayout->addWidget(m_stopButton);
  controlsLayout->addSpacing(15);
  controlsLayout->addWidget(m_timeLabel);
  controlsLayout->addStretch();
  controlsLayout->addWidget(m_volumeButton);
  controlsLayout->addWidget(m_volumeSlider);

  mainLayout->addLayout(controlsLayout);
}

void MainWindow::setupConnections() {
  connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);

  connect(m_playPauseButton, &QPushButton::clicked, this, [this]() {
    if (m_playerController->playbackState() == QMediaPlayer::PlayingState)
      m_playerController->pause();
    else
      m_playerController->play();
  });

  connect(m_stopButton, &QPushButton::clicked, m_playerController,
          &PlayerController::stop);

  connect(m_playerController, &PlayerController::positionChanged, this,
          &MainWindow::updatePosition);
  connect(m_playerController, &PlayerController::durationChanged, this,
          &MainWindow::updateDuration);
  connect(m_playerController, &PlayerController::playbackStateChanged, this,
          &MainWindow::updatePlayPauseButton);

  connect(m_positionSlider, &QSlider::sliderMoved, m_playerController,
          &PlayerController::seek);

  // Volume connections with memory
  connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
    m_playerController->setVolume(static_cast<float>(value) / 100.0f);
    // Remember non-zero volume levels
    if (value > 0) {
      m_previousVolume = value;
    }
  });

  connect(m_volumeButton, &QPushButton::clicked, this, [this]() {
    if (m_volumeSlider->value() > 0) {
      // Mute: remember current volume
      m_previousVolume = m_volumeSlider->value();
      m_volumeSlider->setValue(0);
    } else {
      // Unmute: restore previous volume
      m_volumeSlider->setValue(m_previousVolume);
    }
  });

  // Auto-load first frame: use proper preloading instead of timer hack
  connect(m_playerController, &PlayerController::mediaStatusChanged, this,
          [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::LoadedMedia) {
              // Pause immediately to show first frame without playback
              m_playerController->pause();
              // Update duration after media is loaded
              updateDuration(m_playerController->duration());
            }
          });

  // Error handling with user feedback
  connect(m_playerController, &PlayerController::errorOccurred, this,
          &MainWindow::handleError);
}

void MainWindow::onOpenFile() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Ouvrir une vidéo", "", "Vidéos MP4 (*.mp4)");
  if (!fileName.isEmpty()) {
    m_playerController->openFile(QUrl::fromLocalFile(fileName));
  }
}

void MainWindow::updatePosition(qint64 position) {
  if (!m_positionSlider->isSliderDown()) {
    m_positionSlider->setValue(static_cast<int>(position));
  }

  qint64 totalDuration = m_playerController->duration();
  m_timeLabel->setText(formatTime(position) + " / " + formatTime(totalDuration));
}

void MainWindow::updateDuration(qint64 duration) {
  m_positionSlider->setRange(0, static_cast<int>(duration));
}

void MainWindow::updatePlayPauseButton(QMediaPlayer::PlaybackState state) {
  if (state == QMediaPlayer::PlayingState) {
    m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
  } else {
    m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  }
}

void MainWindow::handleError(const QString &errorMessage) {
  QMessageBox::critical(this, "Erreur de lecture",
                        "Une erreur s'est produite lors de la lecture:\n\n" +
                            errorMessage);
}

QString MainWindow::formatTime(qint64 milliseconds) const {
  // Dynamic formatting: use hh:mm:ss for videos >= 1 hour, mm:ss otherwise
  QTime currentTime(0, 0);
  currentTime = currentTime.addMSecs(static_cast<int>(milliseconds));

  if (milliseconds >= 3600000) { // >= 1 hour
    return currentTime.toString("hh:mm:ss");
  } else {
    return currentTime.toString("mm:ss");
  }
}
