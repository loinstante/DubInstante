#include "TrackSettingsDialog.h"

#include "../gui/RythmoWidget.h"
#include "../core/SettingsManager.h"
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

TrackSettingsDialog::TrackSettingsDialog(RythmoManager *rythmoManager,
                                         int trackCount,
                                         int initialTrackIndex,
                                         QWidget *parent)
    : QDialog(parent), m_rythmoManager(rythmoManager),
      m_currentTrackIndex(qBound(0, initialTrackIndex, qBound(1, trackCount, 4) - 1)),
      m_trackCount(qBound(1, trackCount, 4)) {

  setupUi();

  // Pre-select the initial track button
  if (m_currentTrackIndex < m_trackButtons.size()) {
    m_trackButtons[m_currentTrackIndex]->setChecked(true);
  }

  connect(m_rythmoManager, &RythmoManager::trackStyleChanged, this,
          &TrackSettingsDialog::onManagerStyleChanged);

  loadCurrentTrackStyle();
}

void TrackSettingsDialog::setupUi() {
  setObjectName("trackSettingsDialog");
  setWindowTitle(tr("Personnalisation de la Bande Rythmo"));
  setMinimumSize(680, 560);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(14);

  QLabel *titleLabel = new QLabel(tr("Personnaliser la Bande Rythmo"), this);
  titleLabel->setObjectName("settingsDialogTitle");
  mainLayout->addWidget(titleLabel);

  QLabel *subtitleLabel =
      new QLabel(tr("Réglages visuels en direct, sans interrompre le flux."),
                 this);
  subtitleLabel->setObjectName("settingsDialogSubtitle");
  mainLayout->addWidget(subtitleLabel);

  // Track Selector
  QFrame *trackSelectorCard = new QFrame(this);
  trackSelectorCard->setObjectName("settingsCardTrackSelector");
  QHBoxLayout *topLayout = new QHBoxLayout(trackSelectorCard);
  topLayout->setContentsMargins(14, 12, 14, 12);
  topLayout->setSpacing(8);

  QLabel *trackSelectorLabel =
      new QLabel(tr("Piste à modifier"), trackSelectorCard);
  trackSelectorLabel->setProperty("cssClass", "settingsLabel");
  topLayout->addWidget(trackSelectorLabel);

  m_trackGroup = new QButtonGroup(this);
  m_trackGroup->setExclusive(true);

  for (int i = 0; i < m_trackCount; ++i) {
    QPushButton *btn =
        new QPushButton(tr("Piste %1").arg(i + 1), trackSelectorCard);
    btn->setCheckable(true);
    btn->setProperty("cssClass", "trackSelectorButton");
    btn->setMinimumHeight(30);
    if (i == 0)
      btn->setChecked(true);
    m_trackGroup->addButton(btn, i);
    m_trackButtons.append(btn);
    topLayout->addWidget(btn);
  }

  connect(m_trackGroup, &QButtonGroup::idClicked, this,
          &TrackSettingsDialog::onTrackSelected);

  topLayout->addStretch();
    mainLayout->addWidget(trackSelectorCard);

  // Live Preview
    QGroupBox *previewGroup = new QGroupBox(tr("Aperçu en direct"), this);
    previewGroup->setObjectName("settingsGroupPreview");
  QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setContentsMargins(14, 20, 14, 14);
    previewLayout->setSpacing(10);

    m_previewWidget = new RythmoWidget(previewGroup);
  m_previewWidget->setVisualStyle(RythmoWidget::Standalone);
  m_previewWidget->setEditable(false);
  m_previewWidget->setSpeed(100);
  m_previewWidget->setPlaying(true);
    m_previewWidget->setMinimumHeight(92);
  m_previewWidget->updateDisplay(
      0, 0, "Hello, voici un aperçu de la piste Rythmo...  ", 100);

  // Animate preview using its internal loop by providing changing simulated
  // position
  QTimer *animTimer = new QTimer(this);
  connect(animTimer, &QTimer::timeout, this, [this]() {
    m_simulatedPosMs += 20;
    // Loop position roughly based on text length
    if (m_simulatedPosMs > 5000) {
      m_simulatedPosMs = 0;
    }
    m_previewWidget->sync(m_simulatedPosMs);
  });
  animTimer->start(20);

  previewLayout->addWidget(m_previewWidget);
  mainLayout->addWidget(previewGroup);

  // Presets
  QGroupBox *presetsGroup = new QGroupBox(tr("Préréglages"), this);
  presetsGroup->setObjectName("settingsGroupPresets");
  QHBoxLayout *presetsLayout = new QHBoxLayout(presetsGroup);
  presetsLayout->setContentsMargins(14, 20, 14, 14);
  presetsLayout->setSpacing(8);

  m_presetClassic = new QPushButton(tr("Classique"), presetsGroup);
  m_presetDark = new QPushButton(tr("Sombre"), presetsGroup);
  m_presetBlue = new QPushButton(tr("Bleu"), presetsGroup);
  m_presetRed = new QPushButton(tr("Rouge"), presetsGroup);
  m_presetGreen = new QPushButton(tr("Vert"), presetsGroup);
  m_presetYellow = new QPushButton(tr("Jaune"), presetsGroup);

  QVector<QPushButton *> presetButtons = {m_presetClassic, m_presetDark,
                                          m_presetBlue,    m_presetRed,
                                          m_presetGreen,   m_presetYellow};
  for (QPushButton *presetButton : presetButtons) {
    presetButton->setProperty("cssClass", "presetButton");
    presetButton->setMinimumHeight(30);
  }

  presetsLayout->addWidget(m_presetClassic);
  presetsLayout->addWidget(m_presetDark);
  presetsLayout->addWidget(m_presetBlue);
  presetsLayout->addWidget(m_presetRed);
  presetsLayout->addWidget(m_presetGreen);
  presetsLayout->addWidget(m_presetYellow);

  connect(m_presetClassic, &QPushButton::clicked, this,
          &TrackSettingsDialog::applyPreset);
  connect(m_presetDark, &QPushButton::clicked, this,
          &TrackSettingsDialog::applyPreset);
  connect(m_presetBlue, &QPushButton::clicked, this,
          &TrackSettingsDialog::applyPreset);
  connect(m_presetRed, &QPushButton::clicked, this,
          &TrackSettingsDialog::applyPreset);
  connect(m_presetGreen, &QPushButton::clicked, this,
          &TrackSettingsDialog::applyPreset);
  connect(m_presetYellow, &QPushButton::clicked, this,
          &TrackSettingsDialog::applyPreset);

  mainLayout->addWidget(presetsGroup);

  // Fine Controls
    QGroupBox *fineGroup = new QGroupBox(tr("Réglages Fins"), this);
    fineGroup->setObjectName("settingsGroupFine");
  QGridLayout *fineLayout = new QGridLayout(fineGroup);
    fineLayout->setContentsMargins(14, 20, 14, 14);
    fineLayout->setHorizontalSpacing(12);
    fineLayout->setVerticalSpacing(10);

    QLabel *fontLabel = new QLabel(tr("Police"), fineGroup);
    fontLabel->setProperty("cssClass", "fineLabel");
    fineLayout->addWidget(fontLabel, 0, 0);

    m_fontComboBox = new QFontComboBox(fineGroup);
    m_fontComboBox->setObjectName("settingsFontCombo");
  m_fontComboBox->setFontFilters(QFontComboBox::ScalableFonts);
  m_fontComboBox->setEditable(false);
  m_fontComboBox->setMinimumWidth(250);
  m_fontComboBox->setMaxVisibleItems(15);
  connect(m_fontComboBox, &QFontComboBox::currentFontChanged, this,
          &TrackSettingsDialog::updateFont);
  fineLayout->addWidget(m_fontComboBox, 0, 1);
  fineLayout->setColumnStretch(1, 1);

    QLabel *sizeLabel = new QLabel(tr("Taille globale"), fineGroup);
    sizeLabel->setProperty("cssClass", "fineLabel");
    fineLayout->addWidget(sizeLabel, 1, 0);

    m_globalSizeSpinBox = new QSpinBox(fineGroup);
    m_globalSizeSpinBox->setObjectName("settingsGlobalSizeSpin");
  m_globalSizeSpinBox->setRange(10, 50);
  connect(m_globalSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &TrackSettingsDialog::updateGlobalSize);
  fineLayout->addWidget(m_globalSizeSpinBox, 1, 1);

    QLabel *textColorLabel = new QLabel(tr("Couleur texte"), fineGroup);
    textColorLabel->setProperty("cssClass", "fineLabel");
    fineLayout->addWidget(textColorLabel, 2, 0);

    m_textColorButton = new QPushButton(fineGroup);
    m_textColorButton->setObjectName("settingsTextColorButton");
    m_textColorButton->setProperty("cssClass", "colorPickerButton");
    m_textColorButton->setMinimumHeight(30);
  connect(m_textColorButton, &QPushButton::clicked, this,
          &TrackSettingsDialog::updateTextColor);
  fineLayout->addWidget(m_textColorButton, 2, 1);

    QLabel *backgroundColorLabel = new QLabel(tr("Couleur fond"), fineGroup);
    backgroundColorLabel->setProperty("cssClass", "fineLabel");
    fineLayout->addWidget(backgroundColorLabel, 3, 0);

    m_backgroundColorButton = new QPushButton(fineGroup);
    m_backgroundColorButton->setObjectName("settingsBackgroundColorButton");
    m_backgroundColorButton->setProperty("cssClass", "colorPickerButton");
    m_backgroundColorButton->setMinimumHeight(30);
  connect(m_backgroundColorButton, &QPushButton::clicked, this,
          &TrackSettingsDialog::updateBackgroundColor);
  fineLayout->addWidget(m_backgroundColorButton, 3, 1);

  mainLayout->addWidget(fineGroup);

  // Add close button
  QHBoxLayout *bottomLayout = new QHBoxLayout();
  bottomLayout->setContentsMargins(0, 4, 0, 0);
  bottomLayout->addStretch();
  QPushButton *closeButton = new QPushButton(tr("Fermer"), this);
  closeButton->setObjectName("settingsCloseButton");
  closeButton->setMinimumHeight(34);
  closeButton->setMinimumWidth(110);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
  bottomLayout->addWidget(closeButton);
  mainLayout->addLayout(bottomLayout);
}

void TrackSettingsDialog::updateColorButton(QPushButton *btn,
                                            const QColor &color) {
  bool isDark = false;
  QString themeMode = SettingsManager::instance().theme();
  if (themeMode == "dark") {
    isDark = true;
  } else if (themeMode == "system") {
    isDark = (palette().color(QPalette::Window).value() < 128);
  }

  QString borderCol = isDark ? "#3b3b52" : "#cbd5e1";

  if (color.alpha() == 0) {
    QString bgCol = isDark ? "#161622" : "#ffffff";
    QString fgCol = isDark ? "#8a8a9e" : "#64748b";
    btn->setStyleSheet(QString("background-color: %1;"
                               "border: 1px dashed %2;"
                               "border-radius: 8px;"
                               "color: %3;"
                               "padding: 4px 10px;")
                           .arg(bgCol, borderCol, fgCol));
    btn->setText(tr("Transparent"));
  } else {
    const QString textColor = color.lightnessF() > 0.55 ? "#0f172a" : "#ffffff";
    btn->setStyleSheet(
        QString("background-color: %1;"
                "border: 1px solid %2;"
                "border-radius: 8px;"
                "color: %3;"
                "padding: 4px 10px;")
            .arg(color.name(QColor::HexArgb), borderCol, textColor));
    btn->setText(color.name(QColor::HexRgb).toUpper());
  }
}

void TrackSettingsDialog::loadCurrentTrackStyle() {
  RythmoTrackStyle style = m_rythmoManager->trackStyle(m_currentTrackIndex);

  bool wasFontBlocked = m_fontComboBox->signalsBlocked();
  bool wasSizeBlocked = m_globalSizeSpinBox->signalsBlocked();

  m_fontComboBox->blockSignals(true);
  m_globalSizeSpinBox->blockSignals(true);

  m_fontComboBox->setCurrentFont(style.font);
  m_globalSizeSpinBox->setValue(style.globalSize);
  m_currentTextColor = style.textColor;
  m_currentBackgroundColor = style.backgroundColor;

  updateColorButton(m_textColorButton, m_currentTextColor);
  updateColorButton(m_backgroundColorButton, m_currentBackgroundColor);

  m_fontComboBox->blockSignals(wasFontBlocked);
  m_globalSizeSpinBox->blockSignals(wasSizeBlocked);

  setPreviewStyle(style);
}

void TrackSettingsDialog::setPreviewStyle(const RythmoTrackStyle &style) {
  m_previewWidget->setTrackStyle(style);
}

void TrackSettingsDialog::onTrackSelected(int index) {
  m_currentTrackIndex = index;
  loadCurrentTrackStyle();
}

void TrackSettingsDialog::onManagerStyleChanged(int trackIndex,
                                                const RythmoTrackStyle &style) {
  if (trackIndex == m_currentTrackIndex) {
    loadCurrentTrackStyle();
  }
}

QColor TrackSettingsDialog::chooseColor(const QColor &initialColor,
                                        const QString &title) {
  return QColorDialog::getColor(initialColor, this, title,
                                QColorDialog::ShowAlphaChannel);
}

void TrackSettingsDialog::updateFont() {
  RythmoTrackStyle style = m_rythmoManager->trackStyle(m_currentTrackIndex);
  QFont newFont = m_fontComboBox->currentFont();
  newFont.setPointSize(style.globalSize); // Retain global size
  newFont.setBold(style.font.bold());     // Inherit bold flag
  style.font = newFont;
  m_rythmoManager->setTrackStyle(m_currentTrackIndex, style);
}

void TrackSettingsDialog::updateGlobalSize() {
  RythmoTrackStyle style = m_rythmoManager->trackStyle(m_currentTrackIndex);
  style.globalSize = m_globalSizeSpinBox->value();
  style.font.setPointSize(style.globalSize);
  m_rythmoManager->setTrackStyle(m_currentTrackIndex, style);
}

void TrackSettingsDialog::updateTextColor() {
  QColor color =
      chooseColor(m_currentTextColor, tr("Choisir la couleur du texte"));
  if (color.isValid() && color != m_currentTextColor) {
    m_currentTextColor = color;
    updateColorButton(m_textColorButton, color);

    RythmoTrackStyle style = m_rythmoManager->trackStyle(m_currentTrackIndex);
    style.textColor = color;
    m_rythmoManager->setTrackStyle(m_currentTrackIndex, style);
  }
}

void TrackSettingsDialog::updateBackgroundColor() {
  QColor color =
      chooseColor(m_currentBackgroundColor, tr("Choisir la couleur de fond"));
  if (color.isValid() && color != m_currentBackgroundColor) {
    m_currentBackgroundColor = color;
    updateColorButton(m_backgroundColorButton, color);

    RythmoTrackStyle style = m_rythmoManager->trackStyle(m_currentTrackIndex);
    style.backgroundColor = color;
    m_rythmoManager->setTrackStyle(m_currentTrackIndex, style);
  }
}

void TrackSettingsDialog::applyPreset() {
  QPushButton *btn = qobject_cast<QPushButton *>(sender());
  if (!btn)
    return;

  RythmoTrackStyle style = m_rythmoManager->trackStyle(m_currentTrackIndex);
  int currentGlobalSize = style.globalSize;

  if (btn == m_presetClassic) {
    style = RythmoTrackStyle();
    style.textColor = QColor(34, 34, 34);
    style.backgroundColor = QColor(255, 255, 255);
    style.globalSize = currentGlobalSize;
    style.font.setPointSize(currentGlobalSize);
  } else if (btn == m_presetDark) {
    style.textColor = QColor(255, 255, 255);
    style.backgroundColor = QColor(34, 34, 34);
  } else if (btn == m_presetBlue) {
    style.textColor = QColor(0, 120, 215);
    style.backgroundColor = QColor(255, 255, 255);
  } else if (btn == m_presetRed) {
    style.textColor = QColor(194, 57, 52);
    style.backgroundColor = QColor(255, 255, 255);
  } else if (btn == m_presetGreen) {
    style.textColor = QColor(39, 174, 96);
    style.backgroundColor = QColor(255, 255, 255);
  } else if (btn == m_presetYellow) {
    style.textColor = QColor(241, 196, 15);
    style.backgroundColor = QColor(255, 255, 255);
  }

  m_rythmoManager->setTrackStyle(m_currentTrackIndex, style);
}
