/**
 * @file TrackPanel.cpp
 * @brief Implementation of the TrackPanel class.
 */

#include "TrackPanel.h"
#include "AudioRecorder.h"
#include "ClickableSlider.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

TrackPanel::TrackPanel(const QString &title, AudioRecorder *recorder,
                       QWidget *parent)
    : QWidget(parent), m_title(title), m_recorder(recorder),
      m_inputDeviceCombo(new QComboBox(this)),
      m_volumeSlider(new ClickableSlider(Qt::Horizontal, this)),
      m_gainSpinBox(new QSpinBox(this)) {
  setupUi(title);
  setupConnections();
  populateDeviceList();
}

// =============================================================================
// Configuration
// =============================================================================

void TrackPanel::setDevice(const QAudioDevice &device) {
  if (m_recorder) {
    m_recorder->setDevice(device);
  }
}

void TrackPanel::setVolume(float volume) {
  if (m_recorder) {
    m_recorder->setVolume(volume);
  }

  int sliderValue = static_cast<int>(volume * 100);
  if (m_volumeSlider->value() != sliderValue) {
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(sliderValue);
    m_volumeSlider->blockSignals(false);
  }
}

float TrackPanel::gain() const {
  return static_cast<float>(m_volumeSlider->value()) / 100.0f;
}

QAudioDevice TrackPanel::selectedDevice() const {
  int index = m_inputDeviceCombo->currentIndex();
  if (index >= 0) {
    return m_inputDeviceCombo->itemData(index).value<QAudioDevice>();
  }
  return QAudioDevice();
}

AudioRecorder *TrackPanel::recorder() const { return m_recorder; }

// =============================================================================
// Recording Control
// =============================================================================

void TrackPanel::startRecording(const QUrl &outputUrl) {
  if (m_recorder) {
    m_recorder->startRecording(outputUrl);
  }
}

void TrackPanel::stopRecording() {
  if (m_recorder) {
    m_recorder->stopRecording();
  }
}

// =============================================================================
// Private Methods
// =============================================================================

void TrackPanel::setupUi(const QString &title) {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(5);

  // Title label
  QLabel *titleLabel = new QLabel(title, this);
  titleLabel->setStyleSheet("font-weight: bold;");
  mainLayout->addWidget(titleLabel);

  // Device selection
  QHBoxLayout *deviceLayout = new QHBoxLayout();
  deviceLayout->setSpacing(5);

  QLabel *deviceLabel = new QLabel("Entrée:", this);
  deviceLayout->addWidget(deviceLabel);

  m_inputDeviceCombo->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Preferred);
  deviceLayout->addWidget(m_inputDeviceCombo);

  mainLayout->addLayout(deviceLayout);

  // Volume control
  QHBoxLayout *volumeLayout = new QHBoxLayout();
  volumeLayout->setSpacing(5);

  QLabel *volumeLabel = new QLabel("Gain:", this);
  volumeLayout->addWidget(volumeLabel);

  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  volumeLayout->addWidget(m_volumeSlider);

  m_gainSpinBox->setRange(0, 100);
  m_gainSpinBox->setValue(100);
  m_gainSpinBox->setSuffix("%");
  m_gainSpinBox->setFixedWidth(60);
  volumeLayout->addWidget(m_gainSpinBox);

  mainLayout->addLayout(volumeLayout);
}

void TrackPanel::setupConnections() {
  // Device combo -> Recorder
  connect(m_inputDeviceCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            if (index >= 0 && m_recorder) {
              QVariant data = m_inputDeviceCombo->itemData(index);
              QAudioDevice device = data.value<QAudioDevice>();
              m_recorder->setDevice(device);
            }
          });

  // Volume slider -> Recorder + SpinBox
  connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
    float volume = static_cast<float>(value) / 100.0f;

    if (m_recorder) {
      m_recorder->setVolume(volume);
    }

    if (m_gainSpinBox->value() != value) {
      m_gainSpinBox->blockSignals(true);
      m_gainSpinBox->setValue(value);
      m_gainSpinBox->blockSignals(false);
    }

    emit volumeChanged(volume);
  });

  // SpinBox -> Slider (bidirectional)
  connect(m_gainSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            if (m_volumeSlider->value() != value) {
              m_volumeSlider->blockSignals(true);
              m_volumeSlider->setValue(value);
              m_volumeSlider->blockSignals(false);
            }
          });
}

void TrackPanel::populateDeviceList() {
  m_inputDeviceCombo->clear();

  if (m_recorder) {
    QList<QAudioDevice> devices = m_recorder->availableDevices();

    for (const QAudioDevice &device : devices) {
      m_inputDeviceCombo->addItem(device.description(),
                                  QVariant::fromValue(device));
    }

    // Select first device if available
    if (!devices.isEmpty()) {
      m_recorder->setDevice(devices.first());
    }
  }
}
