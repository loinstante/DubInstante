#include "TrackWidget.h"
#include "AudioMeterWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QStyle>

TrackWidget::TrackWidget(int trackIndex, const QString& title, QWidget *parent)
    : QFrame(parent), m_trackIndex(trackIndex)
{
    setObjectName(QString("track_%1").arg(trackIndex));
    setProperty("cssClass", "track");
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(160);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(20, 20, 20, 18));
    setGraphicsEffect(shadow);

    setupUi(title);
    setupConnections();
}

void TrackWidget::setupUi(const QString& title)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // --- Header ---
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 2);
    
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setProperty("cssClass", "track-header-title");

    m_optionsButton = new QPushButton(QStringLiteral(u"\u2699\uFE0E"), this); // text glyph: the emoji one ignores the QSS colour
    m_optionsButton->setFixedSize(28, 28);
    m_optionsButton->setFlat(true);
    m_optionsButton->setProperty("cssClass", "track-header-btn");
    
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_optionsButton);

    m_recordArmButton = new QPushButton("R", this);
    m_recordArmButton->setObjectName("recordArmButton");
    m_recordArmButton->setCheckable(true);
    m_recordArmButton->setChecked(true);
    m_recordArmButton->setFixedSize(28, 28);
    m_recordArmButton->setToolTip(tr("Arm/Disarm recording"));
    headerLayout->addWidget(m_recordArmButton);

    QFrame *headerLine = new QFrame(this);
    headerLine->setFrameShape(QFrame::NoFrame);
    headerLine->setFixedHeight(1);
    headerLine->setProperty("cssClass", "track-header-line");

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(headerLine);

    // --- IN Control ---
    QHBoxLayout *inLayout = new QHBoxLayout();
    inLayout->setSpacing(10);
    
    QLabel *inLabel = new QLabel(tr("IN:"), this);
    inLabel->setProperty("cssClass", "track-control-label");

    m_inputCombo = new QComboBox(this);
    m_inputCombo->setProperty("cssClass", "track-control-select");
    m_inputCombo->addItem(tr("No input"));

    inLayout->addWidget(inLabel);
    inLayout->addWidget(m_inputCombo, 1);
    
    mainLayout->addLayout(inLayout);

    // --- VOL Control ---
    QHBoxLayout *volLayout = new QHBoxLayout();
    volLayout->setSpacing(10);

    QLabel *volLabel = new QLabel(tr("VOL:"), this);
    volLabel->setProperty("cssClass", "track-control-label");
    // Same width so both controls line up, whatever the language's labels
    const int labelWidth = qMax(inLabel->sizeHint().width(), volLabel->sizeHint().width());
    inLabel->setMinimumWidth(labelWidth);
    volLabel->setMinimumWidth(labelWidth);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setProperty("cssClass", "track-control-slider");

    volLayout->addWidget(volLabel);
    volLayout->addWidget(m_volumeSlider, 1);

    mainLayout->addLayout(volLayout);

    // --- Spacer to push the VU meter to the bottom ---
    mainLayout->addStretch();

    // --- VU Meter ---
    m_vuMeter = new AudioMeterWidget(this);
    mainLayout->addWidget(m_vuMeter);

    m_recordingStateLabel = new QLabel("", this);
    m_recordingStateLabel->setAlignment(Qt::AlignCenter);
    m_recordingStateLabel->setObjectName("recordingStateLabel");
    // Hidden at rest, but its row stays reserved so the mixer height never changes
    m_recordingStateLabel->setFixedHeight(14);
    QSizePolicy stateLabelPolicy = m_recordingStateLabel->sizePolicy();
    stateLabelPolicy.setRetainSizeWhenHidden(true);
    m_recordingStateLabel->setSizePolicy(stateLabelPolicy);
    m_recordingStateLabel->setVisible(false);
    mainLayout->addWidget(m_recordingStateLabel);
}

void TrackWidget::setupConnections()
{
    connect(m_volumeSlider, &QSlider::valueChanged, this, &TrackWidget::onVolumeSliderChanged);
    connect(m_optionsButton, &QPushButton::clicked, this, &TrackWidget::optionsClicked);
    
    // Emit device index when selection changes (index 0 = no input, real devices start at 1)
    connect(m_inputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                // deviceIndex = index - 1 (to skip "no input" at pos 0)
                emit inputDeviceIndexChanged(index - 1);
            });
}

void TrackWidget::onVolumeSliderChanged(int value)
{
    emit volumeChanged(value);
}

void TrackWidget::setVuLevel(int percentage)
{
    if (m_vuMeter) {
        m_vuMeter->setLevel(percentage / 100.0f);
    }
}

void TrackWidget::setInputDevice(const QString& device)
{
    // Projects before 1.0 saved the French "no input" label itself
    int index = device.isEmpty() || device == QLatin1String("Aucune entrée")
                    ? 0
                    : m_inputCombo->findText(device);
    if (index >= 0) {
        m_inputCombo->setCurrentIndex(index);
    }
}

void TrackWidget::setVolume(int volume)
{
    int clamped = qBound(0, volume, 100);
    m_volumeSlider->setValue(clamped);
}

void TrackWidget::populateInputDevices(const QList<QAudioDevice> &devices)
{
    m_inputCombo->blockSignals(true);
    m_inputCombo->clear();
    m_inputCombo->addItem(tr("No input"));
    for (const QAudioDevice &dev : devices) {
        m_inputCombo->addItem(dev.description());
    }
    m_inputCombo->blockSignals(false);
}

void TrackWidget::setRecordingState(const QString &state)
{
    if (state == "recording") {
        m_recordingStateLabel->setText("● " + tr("REC"));
        m_recordingStateLabel->setProperty("state", "recording");
        m_recordingStateLabel->style()->unpolish(m_recordingStateLabel);
        m_recordingStateLabel->style()->polish(m_recordingStateLabel);
        m_recordingStateLabel->setVisible(true);
    } else if (state == "playing") {
        m_recordingStateLabel->setText("▶ " + tr("PLAYBACK"));
        m_recordingStateLabel->setProperty("state", "playing");
        m_recordingStateLabel->style()->unpolish(m_recordingStateLabel);
        m_recordingStateLabel->style()->polish(m_recordingStateLabel);
        m_recordingStateLabel->setVisible(true);
    } else {
        m_recordingStateLabel->setVisible(false);
    }
}
