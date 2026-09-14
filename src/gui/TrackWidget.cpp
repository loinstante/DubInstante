#include "TrackWidget.h"
#include "AudioMeterWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QStyle>

TrackWidget::TrackWidget(int trackIndex, const QString& title, const QString& badgeColor, QWidget *parent)
    : QFrame(parent), m_trackIndex(trackIndex)
{
    setObjectName(QString("track_%1").arg(trackIndex));
    setProperty("cssClass", "track");
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(160);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(17, 24, 39, 18));
    setGraphicsEffect(shadow);

    setupUi(title, badgeColor);
    setupConnections();
}

void TrackWidget::setupUi(const QString& title, const QString& badgeColor)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Header ---
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 6);
    
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setProperty("cssClass", "track-header-title");

    m_optionsButton = new QPushButton("⚙️", this);
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
    m_recordArmButton->setToolTip(tr("Armer/Désarmer l'enregistrement"));
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
    
    QLabel *inLabel = new QLabel("IN:", this);
    inLabel->setProperty("cssClass", "track-control-label");
    inLabel->setFixedWidth(30);

    m_inputCombo = new QComboBox(this);
    m_inputCombo->setProperty("cssClass", "track-control-select");
    m_inputCombo->addItem("Aucune entrée");

    inLayout->addWidget(inLabel);
    inLayout->addWidget(m_inputCombo, 1);
    
    mainLayout->addLayout(inLayout);

    // --- VOL Control ---
    QHBoxLayout *volLayout = new QHBoxLayout();
    volLayout->setSpacing(10);

    QLabel *volLabel = new QLabel("VOL:", this);
    volLabel->setProperty("cssClass", "track-control-label");
    volLabel->setFixedWidth(30);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setProperty("cssClass", "track-control-slider");

    volLayout->addWidget(volLabel);
    volLayout->addWidget(m_volumeSlider, 1);

    mainLayout->addLayout(volLayout);

    // --- Spacer to push VU and Badge to bottom ---
    mainLayout->addStretch();

    // --- VU Meter ---
    m_vuMeter = new AudioMeterWidget(this);
    mainLayout->addWidget(m_vuMeter);

    m_recordingStateLabel = new QLabel("", this);
    m_recordingStateLabel->setAlignment(Qt::AlignCenter);
    m_recordingStateLabel->setObjectName("recordingStateLabel");
    m_recordingStateLabel->setVisible(false);
    mainLayout->addWidget(m_recordingStateLabel);

    // --- Color Badge ---
    QHBoxLayout *badgeLayout = new QHBoxLayout();
    badgeLayout->setContentsMargins(0, 5, 0, 0);
    badgeLayout->setSpacing(8);

    QFrame *colorBox = new QFrame(this);
    colorBox->setFixedSize(14, 14);
    colorBox->setStyleSheet(QString("background-color: %1; border-radius: 3px;").arg(badgeColor));

    badgeLayout->addWidget(colorBox);
    badgeLayout->addStretch();

    mainLayout->addLayout(badgeLayout);
}

void TrackWidget::setupConnections()
{
    connect(m_volumeSlider, &QSlider::valueChanged, this, &TrackWidget::onVolumeSliderChanged);
    connect(m_optionsButton, &QPushButton::clicked, this, &TrackWidget::optionsClicked);
    connect(m_inputCombo, &QComboBox::currentTextChanged, this, &TrackWidget::inputSelected);
    
    // Emit device index when selection changes (index 0 = "Aucune entrée", real devices start at 1)
    connect(m_inputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                // deviceIndex = index - 1 (to skip "Aucune entrée" at pos 0)
                emit inputDeviceIndexChanged(index - 1);
            });

    connect(m_recordArmButton, &QPushButton::toggled, this, &TrackWidget::recordArmChanged);
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
    int index = m_inputCombo->findText(device);
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
    m_inputCombo->addItem("Aucune entrée");
    for (const QAudioDevice &dev : devices) {
        m_inputCombo->addItem(dev.description());
    }
    m_inputCombo->blockSignals(false);
}

void TrackWidget::setRecordingState(const QString &state)
{
    if (state == "recording") {
        m_recordingStateLabel->setText("● REC");
        m_recordingStateLabel->setProperty("state", "recording");
        m_recordingStateLabel->style()->unpolish(m_recordingStateLabel);
        m_recordingStateLabel->style()->polish(m_recordingStateLabel);
        m_recordingStateLabel->setVisible(true);
    } else if (state == "playing") {
        m_recordingStateLabel->setText("▶ PLAYBACK");
        m_recordingStateLabel->setProperty("state", "playing");
        m_recordingStateLabel->style()->unpolish(m_recordingStateLabel);
        m_recordingStateLabel->style()->polish(m_recordingStateLabel);
        m_recordingStateLabel->setVisible(true);
    } else {
        m_recordingStateLabel->setVisible(false);
    }
}
