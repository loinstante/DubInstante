#include "ExportDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QScrollArea>
#include <QIcon>
#include <QProcess>
#include <QRegularExpression>

// French puts a space before "%": the translation decides
static QString percentText(int value) { return ExportDialog::tr("%1%").arg(value); }

ExportDialog::ExportDialog(const QString &sourceVideo,
                          const QList<ExportTrack> &tracks,
                          qint64 lastRecordedDurationMs,
                          qint64 lastRecordedStartMs,
                          float currentOriginalVolume,
                          QWidget *parent)
    : QDialog(parent)
    , m_sourceVideoPath(sourceVideo)
    , m_tracks(tracks)
    , m_lastRecordedDurationMs(lastRecordedDurationMs)
    , m_lastRecordedStartMs(lastRecordedStartMs)
    , m_defaultOriginalVolume(currentOriginalVolume)
    , m_videoOriginalWidth(0)
    , m_videoOriginalHeight(0)
    , m_videoAspectRatio(1.777f)
    , m_updatingCustomRes(false)
{
    setupUi();
    populateFields();
    validateSettings();
}

ExportDialog::~ExportDialog()
{
    // A still-pending ffprobe is a child destroyed after the widgets: ~QProcess can then
    // deliver finished(), whose handler would write to the already-deleted widgets.
    for (QProcess *probe : findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly)) {
        probe->disconnect(this);
    }
}

void ExportDialog::setupUi()
{
    setObjectName("exportSettingsDialog");
    setWindowTitle(tr("Export Settings"));
    setMinimumSize(860, 600);
    resize(860, 620);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    // 1. Header (Title & Subtitle)
    QLabel *titleLabel = new QLabel(tr("Export Project"), this);
    titleLabel->setObjectName("settingsDialogTitle");
    mainLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(tr("Set the export options and the final mix of your dub."), this);
    subtitleLabel->setObjectName("settingsDialogSubtitle");
    mainLayout->addWidget(subtitleLabel);

    // 2. Destination output textbox (always shown at the top)
    QFrame *destCard = new QFrame(this);
    destCard->setObjectName("settingsCard");
    QVBoxLayout *destLayout = new QVBoxLayout(destCard);
    destLayout->setContentsMargins(14, 10, 14, 10);
    destLayout->setSpacing(6);

    QLabel *destTitle = new QLabel(tr("Destination File"), destCard);
    destTitle->setProperty("cssClass", "fineLabel");
    destLayout->addWidget(destTitle);

    QHBoxLayout *destInputsLayout = new QHBoxLayout();
    destInputsLayout->setSpacing(8);

    m_outputPathEdit = new QLineEdit(destCard);
    m_outputPathEdit->setPlaceholderText(tr("Output path..."));
    destInputsLayout->addWidget(m_outputPathEdit);

    m_browseButton = new QPushButton(destCard);
    m_browseButton->setIcon(QIcon(":/resources/icons/folder_open.svg"));
    m_browseButton->setToolTip(tr("Browse..."));
    m_browseButton->setFixedSize(32, 32);
    m_browseButton->setProperty("cssClass", "iconButton");
    destInputsLayout->addWidget(m_browseButton);

    destLayout->addLayout(destInputsLayout);
    mainLayout->addWidget(destCard);

    // 3. Two-Column Body Layout
    QHBoxLayout *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(16);

    // =========================================================================
    // LEFT COLUMN: Encoding Parameters
    // =========================================================================
    QVBoxLayout *leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(10);

    // 3A. BASIC MODE WIDGET (Single card)
    m_basicWidget = new QFrame(this);
    m_basicWidget->setObjectName("settingsCard");
    QVBoxLayout *basicCardLayout = new QVBoxLayout(m_basicWidget);
    basicCardLayout->setContentsMargins(14, 14, 14, 14);
    basicCardLayout->setSpacing(10);

    QLabel *basicCardTitle = new QLabel(tr("Encoding Settings"), m_basicWidget);
    basicCardTitle->setProperty("cssClass", "settingsLabel");
    basicCardLayout->addWidget(basicCardTitle);

    QWidget *basicFormWidget = new QWidget(m_basicWidget);
    QFormLayout *basicLayout = new QFormLayout(basicFormWidget);
    basicLayout->setContentsMargins(0, 0, 0, 0);
    basicLayout->setVerticalSpacing(12);
    basicLayout->setHorizontalSpacing(14);

    m_formatCombo = new QComboBox(basicFormWidget);
    m_formatCombo->addItem("MP4", "mp4");
    m_formatCombo->addItem("MKV", "mkv");
    m_formatCombo->addItem("MOV", "mov");
    m_formatCombo->addItem("AVI", "avi");
    
    m_resolutionCombo = new QComboBox(basicFormWidget);
    m_resolutionCombo->addItem(tr("Original (no scaling)"), "");
    m_resolutionCombo->addItem(tr("Full HD (1080p)"), "1920:-2");
    m_resolutionCombo->addItem(tr("HD (720p)"), "1280:-2");

    m_videoQualityCombo = new QComboBox(basicFormWidget);
    m_videoQualityCombo->addItem(tr("Fast (standard quality - CRF 26)"), "superfast");
    m_videoQualityCombo->addItem(tr("Standard (recommended - CRF 21)"), "medium");
    m_videoQualityCombo->addItem(tr("High (slow processing - CRF 16)"), "slow");
    m_videoQualityCombo->setCurrentIndex(1);

    m_audioQualityCombo = new QComboBox(basicFormWidget);
    m_audioQualityCombo->addItem(tr("Economy (128 kbps)"), 128);
    m_audioQualityCombo->addItem(tr("Standard (192 kbps)"), 192);
    m_audioQualityCombo->addItem(tr("Superior (256 kbps)"), 256);
    m_audioQualityCombo->addItem(tr("Studio (320 kbps)"), 320);
    m_audioQualityCombo->setCurrentIndex(1);

    QLabel *fmtLabel = new QLabel(tr("Container Format"), basicFormWidget);
    fmtLabel->setProperty("cssClass", "fineLabel");
    basicLayout->addRow(fmtLabel, m_formatCombo);

    QLabel *resLabel = new QLabel(tr("Resolution"), basicFormWidget);
    resLabel->setProperty("cssClass", "fineLabel");
    basicLayout->addRow(resLabel, m_resolutionCombo);

    QLabel *vqLabel = new QLabel(tr("Video quality"), basicFormWidget);
    vqLabel->setProperty("cssClass", "fineLabel");
    basicLayout->addRow(vqLabel, m_videoQualityCombo);

    QLabel *aqLabel = new QLabel(tr("Audio quality"), basicFormWidget);
    aqLabel->setProperty("cssClass", "fineLabel");
    basicLayout->addRow(aqLabel, m_audioQualityCombo);

    basicCardLayout->addWidget(basicFormWidget);
    leftColumn->addWidget(m_basicWidget);

    // 3B. EXPERT MODE WIDGET (QTabWidget)
    m_expertWidget = new QTabWidget(this);
    m_expertWidget->setObjectName("expertTabWidget");
    m_expertWidget->setVisible(false);

    // ==========================================
    // TAB 1: Encoders & Containers
    // ==========================================
    QWidget *tabCodecs = new QWidget(m_expertWidget);
    QFormLayout *layoutCodecs = new QFormLayout(tabCodecs);
    layoutCodecs->setContentsMargins(14, 16, 14, 14);
    layoutCodecs->setVerticalSpacing(12);
    layoutCodecs->setHorizontalSpacing(14);

    m_expFormatCombo = new QComboBox(tabCodecs);
    m_expFormatCombo->addItem("MP4", "mp4");
    m_expFormatCombo->addItem("MKV", "mkv");
    m_expFormatCombo->addItem("MOV", "mov");
    m_expFormatCombo->addItem("AVI", "avi");

    m_expVideoCodecCombo = new QComboBox(tabCodecs);
    m_expVideoCodecCombo->addItem("H.264 (libx264)", "libx264");
    m_expVideoCodecCombo->addItem("HEVC / H.265 (libx265)", "libx265");
    m_expVideoCodecCombo->addItem("VP9 (libvpx-vp9)", "libvpx-vp9");
    m_expVideoCodecCombo->addItem("ProRes (prores)", "prores");
    m_expVideoCodecCombo->addItem(tr("No encoding (copy)"), "copy");

    m_expAudioCodecCombo = new QComboBox(tabCodecs);
    m_expAudioCodecCombo->addItem(tr("AAC (standard)"), "aac");
    m_expAudioCodecCombo->addItem("MP3 (Lame)", "libmp3lame");
    m_expAudioCodecCombo->addItem("AC-3 (Dolby Digital)", "ac3");
    m_expAudioCodecCombo->addItem(tr("PCM 16-bit uncompressed"), "pcm_s16le");
    m_expAudioCodecCombo->addItem(tr("PCM 24-bit uncompressed"), "pcm_s24le");
    m_expAudioCodecCombo->addItem(tr("No encoding (copy)"), "copy");

    QLabel *expFmtLbl = new QLabel(tr("Container Format"), tabCodecs); expFmtLbl->setProperty("cssClass", "fineLabel");
    layoutCodecs->addRow(expFmtLbl, m_expFormatCombo);
    QLabel *expVcLbl = new QLabel(tr("Video Codec"), tabCodecs); expVcLbl->setProperty("cssClass", "fineLabel");
    layoutCodecs->addRow(expVcLbl, m_expVideoCodecCombo);
    QLabel *expAcLbl = new QLabel(tr("Audio Codec"), tabCodecs); expAcLbl->setProperty("cssClass", "fineLabel");
    layoutCodecs->addRow(expAcLbl, m_expAudioCodecCombo);

    m_expertWidget->addTab(tabCodecs, tr("Streams & Codecs").replace('&', "&&"));

    // ==========================================
    // TAB 2: Image & Quality
    // ==========================================
    QWidget *tabVideo = new QWidget(m_expertWidget);
    QFormLayout *layoutVideo = new QFormLayout(tabVideo);
    layoutVideo->setContentsMargins(14, 16, 14, 14);
    layoutVideo->setVerticalSpacing(12);
    layoutVideo->setHorizontalSpacing(14);

    m_expResolutionCombo = new QComboBox(tabVideo);
    m_expResolutionCombo->addItem(tr("Original (no scaling)"), "");
    m_expResolutionCombo->addItem("4K Ultra HD (3840x2160)", "3840:2160");
    m_expResolutionCombo->addItem("2K Quad HD (2560x1440)", "2560:1440");
    m_expResolutionCombo->addItem("Full HD 1080p (1920x1080)", "1920:1080");
    m_expResolutionCombo->addItem("HD 720p (1280x720)", "1280:720");
    m_expResolutionCombo->addItem("SD PAL 576p (768x576)", "768:576");
    m_expResolutionCombo->addItem("SD NTSC 480p (640x480)", "640:480");
    m_expResolutionCombo->addItem(tr("Custom..."), "custom");

    // Custom resolution widget
    m_expCustomResContainer = new QWidget(tabVideo);
    m_expCustomResContainer->setVisible(false);
    QHBoxLayout *customResLayout = new QHBoxLayout(m_expCustomResContainer);
    customResLayout->setContentsMargins(0, 0, 0, 0);
    customResLayout->setSpacing(6);

    m_customWidthSpin = new QSpinBox(m_expCustomResContainer);
    m_customWidthSpin->setRange(16, 8192);
    m_customWidthSpin->setSingleStep(2);
    m_customWidthSpin->setValue(1920);
    customResLayout->addWidget(m_customWidthSpin);

    QLabel *xLabel = new QLabel("x", m_expCustomResContainer);
    xLabel->setAlignment(Qt::AlignCenter);
    customResLayout->addWidget(xLabel);

    m_customHeightSpin = new QSpinBox(m_expCustomResContainer);
    m_customHeightSpin->setRange(16, 8192);
    m_customHeightSpin->setSingleStep(2);
    m_customHeightSpin->setValue(1080);
    customResLayout->addWidget(m_customHeightSpin);

    m_aspectLockCheck = new QCheckBox(tr("Link ratio"), m_expCustomResContainer);
    m_aspectLockCheck->setChecked(true);
    customResLayout->addWidget(m_aspectLockCheck);

    m_expRateControlCombo = new QComboBox(tabVideo);
    m_expRateControlCombo->addItem(tr("Constant quality (CRF)"), "crf");
    m_expRateControlCombo->addItem(tr("Target bitrate (VBR)"), "bitrate");

    // CRF container
    m_expCrfContainer = new QWidget(tabVideo);
    QHBoxLayout *crfLayout = new QHBoxLayout(m_expCrfContainer);
    crfLayout->setContentsMargins(0, 0, 0, 0);
    crfLayout->setSpacing(8);

    m_expCrfSlider = new QSlider(Qt::Horizontal, m_expCrfContainer);
    m_expCrfSlider->setRange(0, 51);
    m_expCrfSlider->setValue(21);
    crfLayout->addWidget(m_expCrfSlider);

    m_expCrfPercentLabel = new QLabel("21", m_expCrfContainer);
    m_expCrfPercentLabel->setFixedWidth(28);
    m_expCrfPercentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    crfLayout->addWidget(m_expCrfPercentLabel);

    // Bitrate target container
    m_expBitrateContainer = new QWidget(tabVideo);
    m_expBitrateContainer->setVisible(false);
    QHBoxLayout *bitrateLayout = new QHBoxLayout(m_expBitrateContainer);
    bitrateLayout->setContentsMargins(0, 0, 0, 0);
    m_expBitrateSpin = new QSpinBox(m_expBitrateContainer);
    m_expBitrateSpin->setRange(1, 150);
    m_expBitrateSpin->setValue(15);
    m_expBitrateSpin->setSuffix(" Mbps");
    bitrateLayout->addWidget(m_expBitrateSpin);

    m_expPresetCombo = new QComboBox(tabVideo);
    m_expPresetCombo->addItem("Ultrafast", "ultrafast");
    m_expPresetCombo->addItem("Superfast", "superfast");
    m_expPresetCombo->addItem("Veryfast", "veryfast");
    m_expPresetCombo->addItem("Faster", "faster");
    m_expPresetCombo->addItem("Fast", "fast");
    m_expPresetCombo->addItem("Medium", "medium");
    m_expPresetCombo->addItem("Slow", "slow");
    m_expPresetCombo->addItem("Slower", "slower");
    m_expPresetCombo->addItem("Veryslow", "veryslow");
    m_expPresetCombo->addItem("Placebo", "placebo");
    m_expPresetCombo->setCurrentIndex(5);

    QLabel *expResLbl = new QLabel(tr("Resolution"), tabVideo); expResLbl->setProperty("cssClass", "fineLabel");
    layoutVideo->addRow(expResLbl, m_expResolutionCombo);
    layoutVideo->addRow("", m_expCustomResContainer);
    
    QLabel *expRcLbl = new QLabel(tr("Bitrate Mode"), tabVideo); expRcLbl->setProperty("cssClass", "fineLabel");
    layoutVideo->addRow(expRcLbl, m_expRateControlCombo);
    layoutVideo->addRow(tr("Quality (CRF)"), m_expCrfContainer);
    layoutVideo->addRow(tr("Target bitrate"), m_expBitrateContainer);

    QLabel *expPresetLbl = new QLabel(tr("Speed Preset"), tabVideo); expPresetLbl->setProperty("cssClass", "fineLabel");
    layoutVideo->addRow(expPresetLbl, m_expPresetCombo);

    m_expertWidget->addTab(tabVideo, tr("Picture & Quality").replace('&', "&&"));

    // ==========================================
    // TAB 3: Audio & Advanced
    // ==========================================
    QWidget *tabAudio = new QWidget(m_expertWidget);
    QFormLayout *layoutAudio = new QFormLayout(tabAudio);
    layoutAudio->setContentsMargins(14, 16, 14, 14);
    layoutAudio->setVerticalSpacing(12);
    layoutAudio->setHorizontalSpacing(14);

    m_expAudioBitrateSpin = new QSpinBox(tabAudio);
    m_expAudioBitrateSpin->setRange(32, 512);
    m_expAudioBitrateSpin->setSuffix(" kbps");
    m_expAudioBitrateSpin->setValue(192);

    m_expSampleRateCombo = new QComboBox(tabAudio);
    m_expSampleRateCombo->addItem(tr("Original"), 0);
    m_expSampleRateCombo->addItem("44.1 kHz", 44100);
    m_expSampleRateCombo->addItem("48.0 kHz", 48000);
    m_expSampleRateCombo->addItem("96.0 kHz", 96000);

    m_expCustomFlagsEdit = new QLineEdit(tabAudio);
    m_expCustomFlagsEdit->setPlaceholderText(tr("Additional flags (e.g. -preset slow)..."));

    QLabel *expAbLbl = new QLabel(tr("Audio Bitrate"), tabAudio); expAbLbl->setProperty("cssClass", "fineLabel");
    layoutAudio->addRow(expAbLbl, m_expAudioBitrateSpin);
    
    QLabel *expSrLbl = new QLabel(tr("Sample Rate"), tabAudio); expSrLbl->setProperty("cssClass", "fineLabel");
    layoutAudio->addRow(expSrLbl, m_expSampleRateCombo);
    
    QLabel *expFlagsLbl = new QLabel(tr("FFmpeg Flags"), tabAudio); expFlagsLbl->setProperty("cssClass", "fineLabel");
    layoutAudio->addRow(expFlagsLbl, m_expCustomFlagsEdit);

    m_expertWidget->addTab(tabAudio, tr("Audio & Flags").replace('&', "&&"));
    
    leftColumn->addWidget(m_expertWidget);

    // Range card (Timeline range)
    QFrame *rangeCard = new QFrame(this);
    rangeCard->setObjectName("settingsCard");
    QFormLayout *rangeForm = new QFormLayout(rangeCard);
    rangeForm->setContentsMargins(14, 10, 14, 10);
    rangeForm->setHorizontalSpacing(12);

    QLabel *rngTitle = new QLabel(tr("Time range"), rangeCard);
    rngTitle->setProperty("cssClass", "fineLabel");
    
    m_rangeCombo = new QComboBox(rangeCard);
    m_rangeCombo->addItem(tr("Whole project (full video)"), "all");
    if (m_lastRecordedDurationMs > 0) {
        m_rangeCombo->addItem(tr("Last recording only"), "last");
        m_rangeCombo->setCurrentIndex(1);
    } else {
        m_rangeCombo->setCurrentIndex(0);
    }
    rangeForm->addRow(rngTitle, m_rangeCombo);
    leftColumn->addWidget(rangeCard);

    // Warning display (at the bottom of the left column)
    m_warningLabel = new QLabel(this);
    m_warningLabel->setObjectName("warningLabel");
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setProperty("cssClass", "warning-text");
    m_warningLabel->setVisible(false);
    leftColumn->addWidget(m_warningLabel);

    bodyLayout->addLayout(leftColumn, 1);

    // =========================================================================
    // RIGHT COLUMN: Audio Mixer
    // =========================================================================
    QGroupBox *mixGroup = new QGroupBox(tr("Volume Mix"), this);
    mixGroup->setMinimumWidth(370);
    QVBoxLayout *mixGroupLayout = new QVBoxLayout(mixGroup);
    mixGroupLayout->setContentsMargins(10, 18, 10, 10);

    QScrollArea *scrollArea = new QScrollArea(mixGroup);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameStyle(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("background: transparent;");

    QWidget *scrollWidget = new QWidget(scrollArea);
    scrollWidget->setStyleSheet("background: transparent;");
    m_audioTracksLayout = new QVBoxLayout(scrollWidget);
    m_audioTracksLayout->setContentsMargins(4, 4, 4, 4);
    m_audioTracksLayout->setSpacing(12);

    // Original Video row
    QWidget *origRow = new QWidget(scrollWidget);
    QHBoxLayout *origRowLayout = new QHBoxLayout(origRow);
    origRowLayout->setContentsMargins(0, 0, 0, 0);
    origRowLayout->setSpacing(8);

    QLabel *origTitle = new QLabel(tr("Original audio:"), origRow);
    origTitle->setMinimumWidth(100);
    origTitle->setProperty("cssClass", "fineLabel");
    origRowLayout->addWidget(origTitle);

    m_originalVolumeSlider = new QSlider(Qt::Horizontal, origRow);
    m_originalVolumeSlider->setRange(0, 200);
    m_originalVolumeSlider->setValue(qBound(0, static_cast<int>(m_defaultOriginalVolume * 100), 200));
    origRowLayout->addWidget(m_originalVolumeSlider);

    m_originalVolPercentLabel = new QLabel(percentText(m_originalVolumeSlider->value()), origRow);
    m_originalVolPercentLabel->setMinimumWidth(40);
    m_originalVolPercentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    origRowLayout->addWidget(m_originalVolPercentLabel);

    m_originalMuteBtn = new QPushButton(origRow);
    m_originalMuteBtn->setIcon(QIcon(":/resources/icons/volume.svg"));
    m_originalMuteBtn->setCheckable(true);
    m_originalMuteBtn->setFixedSize(28, 28);
    m_originalMuteBtn->setChecked(m_defaultOriginalVolume < 0.01f);
    m_originalMuteBtn->setProperty("cssClass", "iconButton");
    origRowLayout->addWidget(m_originalMuteBtn);

    m_audioTracksLayout->addWidget(origRow);

    // Mic tracks
    for (int i = 0; i < m_tracks.size(); ++i) {
        addTrackRow(scrollWidget, i);
    }

    m_audioTracksLayout->addStretch();
    scrollWidget->setLayout(m_audioTracksLayout);
    scrollArea->setWidget(scrollWidget);
    mixGroupLayout->addWidget(scrollArea);
    
    bodyLayout->addWidget(mixGroup, 1);
    mainLayout->addLayout(bodyLayout);

    // 4. Bottom Switch and Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 4, 0, 0);

    m_advancedCheck = new QCheckBox(tr("Enable Expert Mode (sound & video engineer)"), this);
    m_advancedCheck->setObjectName("advancedCheck");
    bottomLayout->addWidget(m_advancedCheck);

    bottomLayout->addStretch();

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setMinimumSize(90, 32);
    m_btnCancel->setProperty("cssClass", "presetButton");
    bottomLayout->addWidget(m_btnCancel);

    m_btnExport = new QPushButton(tr("Export"), this);
    m_btnExport->setObjectName("settingsExportButton");
    m_btnExport->setMinimumSize(100, 32);
    m_btnExport->setDefault(true);
    bottomLayout->addWidget(m_btnExport);

    mainLayout->addLayout(bottomLayout);

    // Basic connections
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::onFormatChanged);
    connect(m_browseButton, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    connect(m_advancedCheck, &QCheckBox::toggled, this, &ExportDialog::onAdvancedToggled);
    connect(m_rangeCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::validateSettings);
    
    // Expert connections
    connect(m_expFormatCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::onFormatChanged);
    connect(m_expVideoCodecCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::onCodecChanged);
    connect(m_expAudioCodecCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::onCodecChanged);
    connect(m_expResolutionCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::onResolutionSelectionChanged);
    
    connect(m_customWidthSpin, &QSpinBox::valueChanged, this, &ExportDialog::onCustomResolutionWidthChanged);
    connect(m_customHeightSpin, &QSpinBox::valueChanged, this, &ExportDialog::onCustomResolutionHeightChanged);
    
    connect(m_expRateControlCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::onRateControlSelectionChanged);
    connect(m_expCrfSlider, &QSlider::valueChanged, this, [this](int val) {
        m_expCrfPercentLabel->setText(QString::number(val));
        validateSettings();
    });
    connect(m_expBitrateSpin, &QSpinBox::valueChanged, this, &ExportDialog::validateSettings);
    connect(m_expAudioBitrateSpin, &QSpinBox::valueChanged, this, &ExportDialog::validateSettings);
    connect(m_expPresetCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::validateSettings);
    connect(m_expSampleRateCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::validateSettings);
    connect(m_expCustomFlagsEdit, &QLineEdit::textChanged, this, &ExportDialog::validateSettings);

    // Volume connections
    connect(m_originalVolumeSlider, &QSlider::valueChanged, this, &ExportDialog::onVolumeSliderChanged);
    connect(m_originalMuteBtn, &QPushButton::toggled, this, &ExportDialog::onMuteToggled);

    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnExport, &QPushButton::clicked, this, &QDialog::accept);
}

void ExportDialog::addTrackRow(QWidget *parent, int index)
{
    const ExportTrack &track = m_tracks[index];
    const QString title = tr("Track %1").arg(track.number);
    QWidget *row = new QWidget(parent);
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    QLabel *lbl = new QLabel(tr("%1:").arg(title), row);
    lbl->setMinimumWidth(100);
    lbl->setProperty("cssClass", "fineLabel");
    rowLayout->addWidget(lbl);

    const float defaultVolume = track.volume;
    const bool defaultMute = track.muted;

    QSlider *slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 200);
    slider->setValue(qBound(0, static_cast<int>(defaultVolume * 100), 200));
    slider->setProperty("trackIndex", index);
    rowLayout->addWidget(slider);

    QLabel *pct = new QLabel(percentText(slider->value()), row);
    pct->setMinimumWidth(40);
    pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(pct);

    QPushButton *mute = new QPushButton(row);
    mute->setIcon(QIcon(":/resources/icons/volume.svg"));
    mute->setCheckable(true);
    mute->setFixedSize(28, 28);
    mute->setChecked(defaultMute);
    mute->setProperty("trackIndex", index);
    mute->setProperty("cssClass", "iconButton");
    rowLayout->addWidget(mute);

    m_trackSliders.append(slider);
    m_trackPercentLabels.append(pct);
    m_trackMuteBtns.append(mute);

    m_audioTracksLayout->addWidget(row);

    connect(slider, &QSlider::valueChanged, this, &ExportDialog::onVolumeSliderChanged);
    connect(mute, &QPushButton::toggled, this, &ExportDialog::onMuteToggled);
}

void ExportDialog::populateFields()
{
    // 1. Defaults right away; the async ffprobe below refines them on arrival
    // (a blocking probe froze the dialog up to 1.5 s on slow/network storage)
    m_videoOriginalWidth = 1920;
    m_videoOriginalHeight = 1080;
    m_videoAspectRatio = 1.777f;
    m_customWidthSpin->setValue(m_videoOriginalWidth);
    m_customHeightSpin->setValue(m_videoOriginalHeight);

    if (!m_sourceVideoPath.isEmpty() && QFile::exists(m_sourceVideoPath)) {
        auto *probe = new QProcess(this);
        connect(probe, &QProcess::errorOccurred, probe, &QObject::deleteLater);
        connect(probe, &QProcess::finished, this,
                [this, probe](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString output = QString::fromUtf8(probe->readAllStandardOutput());
            probe->deleteLater();
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                return; // keep defaults, assume the source has audio
            }

            bool hasAudioStream = false;
            int width = 0;
            int height = 0;
            const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                QStringList parts = line.trimmed().split(',');
                if (parts.value(0) == "video" && parts.size() >= 3 && width <= 0) {
                    width = parts[1].toInt();
                    height = parts[2].toInt();
                } else if (parts.value(0) == "audio") {
                    hasAudioStream = true;
                }
            }

            if (width > 0 && height > 0) {
                m_videoOriginalWidth = width;
                m_videoOriginalHeight = height;
                m_videoAspectRatio = static_cast<float>(width) / height;
                m_customWidthSpin->setValue(width);
                m_customHeightSpin->setValue(height);
            }

            // Video without audio: mixing [0:a] would make FFmpeg fail
            if (!hasAudioStream) {
                m_defaultOriginalVolume = 0.0f;
                m_originalVolumeSlider->setValue(0);
                m_originalVolumeSlider->setEnabled(false);
                m_originalMuteBtn->setChecked(true);
                m_originalMuteBtn->setEnabled(false);
            }
        });
        probe->start(ExportService::toolPath("ffprobe"),
                     QStringList() << "-v" << "error"
                                   << "-show_entries" << "stream=codec_type,width,height"
                                   << "-of" << "csv=p=0" << m_sourceVideoPath);
    }

    // 2. Set default output path
    if (!m_sourceVideoPath.isEmpty()) {
        QFileInfo videoInfo(m_sourceVideoPath);
        QString defaultDir = videoInfo.absolutePath();
        QString baseName = videoInfo.completeBaseName();
        QString formatExt = m_formatCombo->currentData().toString();
        m_outputPathEdit->setText(QDir(defaultDir).filePath(baseName + "_double." + formatExt));
    } else {
        m_outputPathEdit->setText(QDir::homePath() + "/dub_result.mp4");
    }
}

void ExportDialog::onFormatChanged(int index)
{
    QComboBox *snd = qobject_cast<QComboBox *>(sender());
    if (snd == m_formatCombo) {
        m_expFormatCombo->blockSignals(true);
        m_expFormatCombo->setCurrentIndex(index);
        m_expFormatCombo->blockSignals(false);
    } else if (snd == m_expFormatCombo) {
        m_formatCombo->blockSignals(true);
        m_formatCombo->setCurrentIndex(index);
        m_formatCombo->blockSignals(false);
    }

    QString formatExt = m_formatCombo->currentData().toString();
    QString currentPath = m_outputPathEdit->text().trimmed();

    if (!currentPath.isEmpty()) {
        QFileInfo fileInfo(currentPath);
        QString absoluteDir = fileInfo.absolutePath();
        QString baseName = fileInfo.completeBaseName();
        m_outputPathEdit->setText(QDir(absoluteDir).filePath(baseName + "." + formatExt));
    }

    onCodecChanged();
}

void ExportDialog::onBrowseClicked()
{
    QString formatExt = m_formatCombo->currentData().toString();
    QString filter = QString("%1 (*.%2)").arg(m_formatCombo->currentText()).arg(formatExt);
    
    QString currentPath = m_outputPathEdit->text().trimmed();
    QString startDir = currentPath.isEmpty() ? QDir::homePath() : QFileInfo(currentPath).absolutePath();

    QString selectedFile = QFileDialog::getSaveFileName(
        this, tr("Choose the export file"), startDir, filter);

    if (!selectedFile.isEmpty()) {
        if (!selectedFile.endsWith("." + formatExt, Qt::CaseInsensitive)) {
            selectedFile += "." + formatExt;
        }
        m_outputPathEdit->setText(selectedFile);
    }
}

void ExportDialog::onAdvancedToggled(bool checked)
{
    m_basicWidget->setVisible(!checked);
    m_expertWidget->setVisible(checked);
    validateSettings();
}

void ExportDialog::onResolutionSelectionChanged(int index)
{
    Q_UNUSED(index);
    QString resolution = m_expResolutionCombo->currentData().toString();
    m_expCustomResContainer->setVisible(resolution == "custom");
    validateSettings();
}

void ExportDialog::onRateControlSelectionChanged(int index)
{
    Q_UNUSED(index);
    QString rateControl = m_expRateControlCombo->currentData().toString();
    
    m_expCrfContainer->setVisible(rateControl == "crf");
    m_expBitrateContainer->setVisible(rateControl == "bitrate");

    // Retrieve layout label widgets in parent tab QFormLayout
    QFormLayout *layout = qobject_cast<QFormLayout *>(m_expCrfContainer->parentWidget()->layout());
    if (layout) {
        QWidget *crfLabel = layout->labelForField(m_expCrfContainer);
        QWidget *bitrateLabel = layout->labelForField(m_expBitrateContainer);
        if (crfLabel) crfLabel->setVisible(rateControl == "crf");
        if (bitrateLabel) bitrateLabel->setVisible(rateControl == "bitrate");
    }
    
    validateSettings();
}

void ExportDialog::onCustomResolutionWidthChanged(int width)
{
    if (m_updatingCustomRes) return;
    if (m_aspectLockCheck->isChecked()) {
        m_updatingCustomRes = true;
        int calculatedHeight = qRound(width / m_videoAspectRatio);
        calculatedHeight = ((calculatedHeight + 1) / 2) * 2;
        m_customHeightSpin->setValue(qBound(16, calculatedHeight, 8192));
        m_updatingCustomRes = false;
    }
    validateSettings();
}

void ExportDialog::onCustomResolutionHeightChanged(int height)
{
    if (m_updatingCustomRes) return;
    if (m_aspectLockCheck->isChecked()) {
        m_updatingCustomRes = true;
        int calculatedWidth = qRound(height * m_videoAspectRatio);
        calculatedWidth = ((calculatedWidth + 1) / 2) * 2;
        m_customWidthSpin->setValue(qBound(16, calculatedWidth, 8192));
        m_updatingCustomRes = false;
    }
    validateSettings();
}

void ExportDialog::onCodecChanged()
{
    validateSettings();
}

void ExportDialog::validateSettings()
{
    QString warning = "";

    if (m_advancedCheck->isChecked()) {
        QString format = m_expFormatCombo->currentData().toString();
        QString vCodec = m_expVideoCodecCombo->currentData().toString();
        QString aCodec = m_expAudioCodecCombo->currentData().toString();

        if (format == "mp4") {
            if (vCodec == "prores") {
                warning += tr("⚠️ ProRes is not standard in an MP4 container. Use MOV or MKV.\n");
            }
            if (aCodec.startsWith("pcm")) {
                warning += tr("⚠️ Uncompressed audio (PCM) is not standard in an MP4 container. Use MOV or MKV.\n");
            }
        } else if (format == "avi") {
            if (aCodec == "aac") {
                warning += tr("⚠️ The AAC audio codec is not standard in an AVI container. Use MP3 or PCM.\n");
            }
            if (vCodec == "libx265" || vCodec == "libvpx-vp9") {
                warning += tr("⚠️ HEVC and VP9 are not recommended in AVI. Use MP4 or MKV.\n");
            }
        }

        if (vCodec == "copy" && m_rangeCombo->currentData().toString() == "last") {
            warning += tr("⚠️ Video stream copy: the cut snaps to the nearest keyframe, so the start may be off by a few frames.\n");
        }

        if (vCodec == "copy" && !m_expResolutionCombo->currentData().toString().isEmpty()) {
            warning += tr("⚠️ Resizing is ignored when copying the video stream: the original resolution is kept.\n");
        }

        if (m_expResolutionCombo->currentData().toString() == "custom" && vCodec != "copy") {
            int w = m_customWidthSpin->value();
            int h = m_customHeightSpin->value();
            if (w % 2 != 0 || h % 2 != 0) {
                warning += tr("⚠️ FFmpeg needs even dimensions (divisible by 2) to encode H.264/H.265.\n");
            }
        }

        if (m_expRateControlCombo->currentData().toString() == "crf" && vCodec != "copy") {
            int crf = m_expCrfSlider->value();
            if (crf < 10) {
                warning += tr("⚠️ Very low CRF (%1): the video file will be huge.\n").arg(crf);
            } else if (crf > 35) {
                warning += tr("⚠️ Very high CRF (%1): the video will be heavily pixelated.\n").arg(crf);
            }
        }

        if (aCodec != "copy" && !aCodec.startsWith("pcm")) {
            int aBitrate = m_expAudioBitrateSpin->value();
            if (aBitrate < 64) {
                warning += tr("⚠️ Low audio bitrate (%1 kbps): compression artifacts will be audible.\n").arg(aBitrate);
            } else if (aBitrate > 320 && aCodec == "aac") {
                warning += tr("⚠️ Unnecessary audio bitrate (%1 kbps): AAC gains nothing above 320 kbps.\n").arg(aBitrate);
            }
        }
    }

    updateWarningText(warning);
}

void ExportDialog::updateWarningText(const QString &warning)
{
    m_warningLabel->setText(warning);
    m_warningLabel->setVisible(!warning.isEmpty());
}

void ExportDialog::onVolumeSliderChanged(int value)
{
    QObject *snd = sender();
    if (snd == m_originalVolumeSlider) {
        m_originalVolPercentLabel->setText(percentText(value));
        if (value > 0 && m_originalMuteBtn->isChecked()) {
            m_originalMuteBtn->blockSignals(true);
            m_originalMuteBtn->setChecked(false);
            m_originalMuteBtn->blockSignals(false);
        }
    } else {
        for (int i = 0; i < m_trackSliders.size(); ++i) {
            if (snd == m_trackSliders[i]) {
                m_trackPercentLabels[i]->setText(percentText(value));
                if (value > 0 && m_trackMuteBtns[i]->isChecked()) {
                    m_trackMuteBtns[i]->blockSignals(true);
                    m_trackMuteBtns[i]->setChecked(false);
                    m_trackMuteBtns[i]->blockSignals(false);
                }
                break;
            }
        }
    }
}

void ExportDialog::onMuteToggled()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;

    bool muted = btn->isChecked();
    if (btn == m_originalMuteBtn) {
        if (muted) {
            m_originalVolumeSlider->setValue(0);
        } else {
            m_originalVolumeSlider->setValue(qBound(10, static_cast<int>(m_defaultOriginalVolume * 100), 100));
        }
    } else {
        int idx = m_trackMuteBtns.indexOf(btn);
        if (idx >= 0) {
            if (muted) {
                m_trackSliders[idx]->setValue(0);
            } else {
                const float defVol = m_tracks.value(idx).volume;
                m_trackSliders[idx]->setValue(qBound(10, static_cast<int>(defVol * 100), 100));
            }
        }
    }
}

ExportConfig ExportDialog::exportConfig() const
{
    ExportConfig config;
    config.videoPath = m_sourceVideoPath;
    config.outputPath = m_outputPathEdit->text().trimmed();
    
    config.originalVolume = m_originalMuteBtn->isChecked() ? 0.0f : (m_originalVolumeSlider->value() / 100.0f);
    for (int i = 0; i < m_trackSliders.size(); ++i) {
        const float vol = m_trackMuteBtns[i]->isChecked() ? 0.0f : (m_trackSliders[i]->value() / 100.0f);
        for (ExportSegment segment : m_tracks[i].segments) {
            segment.volume = vol;
            config.segments.append(segment);
        }
    }

    // Time Range
    if (m_rangeCombo->currentData().toString() == "last") {
        config.durationMs   = m_lastRecordedDurationMs;
        config.rangeStartMs = m_lastRecordedStartMs;
    } else {
        config.durationMs   = -1;
        config.rangeStartMs = 0;
    }
    
    config.expertMode = m_advancedCheck->isChecked();

    if (config.expertMode) {
        config.format = m_expFormatCombo->currentData().toString();
        config.videoCodec = m_expVideoCodecCombo->currentData().toString();
        config.audioCodec = m_expAudioCodecCombo->currentData().toString();
        config.speedPreset = m_expPresetCombo->currentData().toString();
        
        QString resSelection = m_expResolutionCombo->currentData().toString();
        if (resSelection == "custom") {
            config.scaleResolution = QString("%1:%2").arg(m_customWidthSpin->value()).arg(m_customHeightSpin->value());
        } else {
            config.scaleResolution = resSelection;
        }

        QString rc = m_expRateControlCombo->currentData().toString();
        if (rc == "bitrate") {
            config.videoBitrateMbps = m_expBitrateSpin->value();
            config.crf = -1;
        } else {
            config.videoBitrateMbps = 0;
            config.crf = m_expCrfSlider->value();
        }

        config.audioBitrateKbps = m_expAudioBitrateSpin->value();
        config.sampleRateHz = m_expSampleRateCombo->currentData().toInt();
        config.customFFmpegFlags = m_expCustomFlagsEdit->text().trimmed();
    } else {
        config.format = m_formatCombo->currentData().toString();
        config.scaleResolution = m_resolutionCombo->currentData().toString();
        config.speedPreset = m_videoQualityCombo->currentData().toString();
        
        if (config.speedPreset == "superfast") {
            config.crf = 26;
        } else if (config.speedPreset == "slow") {
            config.crf = 16;
        } else {
            config.crf = 21;
        }

        config.audioBitrateKbps = m_audioQualityCombo->currentData().toInt();
        config.videoCodec = "libx264";
        config.audioCodec = "aac";
        config.videoBitrateMbps = 0;
        config.sampleRateHz = 0;
        config.customFFmpegFlags = "";
    }

    return config;
}
