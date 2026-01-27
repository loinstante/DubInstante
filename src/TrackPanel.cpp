#include "TrackPanel.h"
#include "ClickableSlider.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

TrackPanel::TrackPanel(const QString &title,
                       AudioRecorderManager *recorderManager, QWidget *parent)
    : QWidget(parent), m_title(title), m_recorderManager(recorderManager) {
  setupUi(title);
  setupConnections();
}

void TrackPanel::setupUi(const QString &title) {
  QFrame *frame = new QFrame(this);
  frame->setFrameShape(QFrame::NoFrame);

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 2, 0, 2);
  layout->setSpacing(10);

  // Title
  layout->addWidget(new QLabel("<b>" + title + ":</b>", this));

  // Mic Selection
  QHBoxLayout *micGroup = new QHBoxLayout();
  micGroup->setSpacing(2);
  micGroup->addWidget(new QLabel(tr("Mic:"), this));

  m_inputDeviceCombo = new QComboBox(this);
  m_inputDeviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_inputDeviceCombo->setMinimumWidth(100);

  // Populate devices
  m_inputDeviceCombo->addItem(tr("Aucun (NONE)"), QVariant());
  auto devices = m_recorderManager->availableDevices();
  for (const auto &device : devices) {
    m_inputDeviceCombo->addItem(device.description(),
                                QVariant::fromValue(device));
  }

  micGroup->addWidget(m_inputDeviceCombo);
  layout->addLayout(micGroup);

  // Gain Control
  QHBoxLayout *gainGroup = new QHBoxLayout();
  gainGroup->setSpacing(5);
  gainGroup->addWidget(new QLabel(tr("Gain:"), this));

  m_volumeSlider = new ClickableSlider(Qt::Horizontal, this);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  m_volumeSlider->setFixedWidth(100);
  gainGroup->addWidget(m_volumeSlider);

  m_gainSpinBox = new QSpinBox(this);
  m_gainSpinBox->setRange(0, 100);
  m_gainSpinBox->setValue(100);
  m_gainSpinBox->setFixedWidth(70);
  m_gainSpinBox->setAlignment(Qt::AlignRight);
  m_gainSpinBox->setSuffix("%");
  gainGroup->addWidget(m_gainSpinBox);

  layout->addLayout(gainGroup);
}

void TrackPanel::setupConnections() {
  connect(m_inputDeviceCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            QVariant data = m_inputDeviceCombo->itemData(index);
            if (data.isValid()) {
              auto device = data.value<QAudioDevice>();
              m_recorderManager->setDevice(device);
            } else {
              m_recorderManager->setDevice(QAudioDevice());
            }
          });

  connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
    float vol = value / 100.0f;
    m_recorderManager->setVolume(vol);
    emit volumeChanged(vol);

    if (m_gainSpinBox->value() != value) {
      m_gainSpinBox->blockSignals(true);
      m_gainSpinBox->setValue(value);
      m_gainSpinBox->blockSignals(false);
    }
  });

  connect(m_gainSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
    if (m_volumeSlider->value() != value) {
      m_volumeSlider->setValue(value);
    }
  });
}

AudioRecorderManager *TrackPanel::recorderManager() const {
  return m_recorderManager;
}

void TrackPanel::startRecording(const QUrl &outputUrl) {
  m_recorderManager->startRecording(outputUrl);
}

void TrackPanel::stopRecording() { m_recorderManager->stopRecording(); }

void TrackPanel::setDevice(const QAudioDevice &device) {
  // Optional: Find device in combo box and select it
}

void TrackPanel::setVolume(float volume) {
  int val = static_cast<int>(volume * 100);
  m_volumeSlider->setValue(val);
}
