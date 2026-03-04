#include "TrackSettingsDialog.h"

#include "../gui/RythmoWidget.h"
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

TrackSettingsDialog::TrackSettingsDialog(RythmoManager *rythmoManager,
                                         QWidget *parent)
    : QDialog(parent), m_rythmoManager(rythmoManager), m_currentTrackIndex(0) {

  setupUi();

  connect(m_rythmoManager, &RythmoManager::trackStyleChanged, this,
          &TrackSettingsDialog::onManagerStyleChanged);

  loadCurrentTrackStyle();
}

void TrackSettingsDialog::setupUi() {
  setWindowTitle(tr("Personnalisation de la Bande Rythmo"));
  setMinimumWidth(500);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Track Selector
  QHBoxLayout *topLayout = new QHBoxLayout();
  topLayout->addWidget(new QLabel(tr("Piste à modifier :")));

  m_trackGroup = new QButtonGroup(this);
  m_trackGroup->setExclusive(true);

  m_btnTrack1 = new QPushButton(tr("Piste 1"));
  m_btnTrack1->setCheckable(true);
  m_btnTrack1->setChecked(true);

  m_btnTrack2 = new QPushButton(tr("Piste 2"));
  m_btnTrack2->setCheckable(true);

  m_trackGroup->addButton(m_btnTrack1, 0);
  m_trackGroup->addButton(m_btnTrack2, 1);

  connect(m_trackGroup, &QButtonGroup::idClicked, this,
          &TrackSettingsDialog::onTrackSelected);

  topLayout->addWidget(m_btnTrack1);
  topLayout->addWidget(m_btnTrack2);
  topLayout->addStretch();
  mainLayout->addLayout(topLayout);

  // Live Preview
  QGroupBox *previewGroup = new QGroupBox(tr("Aperçu en direct"));
  QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
  m_previewWidget = new RythmoWidget(this);
  m_previewWidget->setVisualStyle(RythmoWidget::Standalone);
  m_previewWidget->setEditable(false);
  m_previewWidget->setSpeed(100);
  m_previewWidget->setPlaying(true);
  m_previewWidget->updateDisplay(
      0, 0, "Hello, voici un aperçu de la piste Rythmo...  ", 100);

  // Animate preview using its internal loop by providing changing simulated
  // position
  QTimer *animTimer = new QTimer(this);
  connect(animTimer, &QTimer::timeout, this, [this]() {
    static qint64 simulatedPosMs = 0;
    simulatedPosMs += 20;
    // Loop position roughly based on text length
    if (simulatedPosMs > 5000) {
      simulatedPosMs = 0;
    }
    m_previewWidget->sync(simulatedPosMs);
  });
  animTimer->start(20);

  previewLayout->addWidget(m_previewWidget);
  mainLayout->addWidget(previewGroup);

  // Presets
  QGroupBox *presetsGroup = new QGroupBox(tr("Préréglages"));
  QHBoxLayout *presetsLayout = new QHBoxLayout(presetsGroup);
  m_presetClassic = new QPushButton(tr("Classique"));
  m_presetDark = new QPushButton(tr("Sombre"));
  m_presetBlue = new QPushButton(tr("Bleu"));
  m_presetRed = new QPushButton(tr("Rouge"));
  m_presetGreen = new QPushButton(tr("Vert"));
  m_presetYellow = new QPushButton(tr("Jaune"));

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
  QGroupBox *fineGroup = new QGroupBox(tr("Réglages Fins"));
  QGridLayout *fineLayout = new QGridLayout(fineGroup);

  fineLayout->addWidget(new QLabel(tr("Police :")), 0, 0);
  m_fontComboBox = new QFontComboBox();
  m_fontComboBox->setFontFilters(QFontComboBox::ScalableFonts);
  m_fontComboBox->setEditable(false);
  m_fontComboBox->setMinimumWidth(250);
  m_fontComboBox->setMaxVisibleItems(15);
  connect(m_fontComboBox, &QFontComboBox::currentFontChanged, this,
          &TrackSettingsDialog::updateFont);
  fineLayout->addWidget(m_fontComboBox, 0, 1);
  fineLayout->setColumnStretch(1, 1);

  fineLayout->addWidget(new QLabel(tr("Taille globale :")), 1, 0);
  m_globalSizeSpinBox = new QSpinBox();
  m_globalSizeSpinBox->setRange(10, 50);
  connect(m_globalSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &TrackSettingsDialog::updateGlobalSize);
  fineLayout->addWidget(m_globalSizeSpinBox, 1, 1);

  fineLayout->addWidget(new QLabel(tr("Couleur Texte :")), 2, 0);
  m_textColorButton = new QPushButton();
  connect(m_textColorButton, &QPushButton::clicked, this,
          &TrackSettingsDialog::updateTextColor);
  fineLayout->addWidget(m_textColorButton, 2, 1);

  fineLayout->addWidget(new QLabel(tr("Couleur Fond :")), 3, 0);
  m_backgroundColorButton = new QPushButton();
  connect(m_backgroundColorButton, &QPushButton::clicked, this,
          &TrackSettingsDialog::updateBackgroundColor);
  fineLayout->addWidget(m_backgroundColorButton, 3, 1);

  mainLayout->addWidget(fineGroup);

  // Add close button
  QHBoxLayout *bottomLayout = new QHBoxLayout();
  bottomLayout->addStretch();
  QPushButton *closeButton = new QPushButton(tr("Fermer"));
  connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
  bottomLayout->addWidget(closeButton);
  mainLayout->addLayout(bottomLayout);
}

void TrackSettingsDialog::updateColorButton(QPushButton *btn,
                                            const QColor &color) {
  if (color.alpha() == 0) {
    btn->setStyleSheet(
        "background-color: transparent; border: 1px dashed #777;");
    btn->setText("Transparent");
  } else {
    btn->setStyleSheet(QString("background-color: %1; border: 1px solid #777;")
                           .arg(color.name(QColor::HexArgb)));
    btn->setText("");
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
