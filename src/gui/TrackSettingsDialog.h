#ifndef TRACKSETTINGSDIALOG_H
#define TRACKSETTINGSDIALOG_H

#include "../core/RythmoManager.h"
#include <QButtonGroup> // Added this include for QButtonGroup
#include <QComboBox>
#include <QDialog>
#include <QFontComboBox>
#include <QPushButton>
#include <QSpinBox>

class RythmoManager;
class RythmoWidget;

class TrackSettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit TrackSettingsDialog(RythmoManager *rythmoManager,
                               QWidget *parent = nullptr);
  ~TrackSettingsDialog() override = default;

private slots:
  void onTrackSelected(int index);
  void applyPreset();
  void updateFont();
  void updateTextColor();
  void updateBackgroundColor();
  void updateGlobalSize();

  // Listen to changes from outside
  void onManagerStyleChanged(int trackIndex, const RythmoTrackStyle &style);

private:
  void setupUi();
  void loadCurrentTrackStyle();
  void setPreviewStyle(const RythmoTrackStyle &style);
  QColor chooseColor(const QColor &initialColor, const QString &title);
  void updateColorButton(QPushButton *btn, const QColor &color);

  RythmoManager *m_rythmoManager;
  int m_currentTrackIndex;

  // UI Elements
  QButtonGroup *m_trackGroup;
  QPushButton *m_btnTrack1;
  QPushButton *m_btnTrack2;

  // Fine controls
  QFontComboBox *m_fontComboBox;
  QPushButton *m_textColorButton;
  QPushButton *m_backgroundColorButton;
  QSpinBox *m_globalSizeSpinBox;

  // Presets
  QPushButton *m_presetClassic;
  QPushButton *m_presetDark;
  QPushButton *m_presetBlue;
  QPushButton *m_presetRed;
  QPushButton *m_presetGreen;
  QPushButton *m_presetYellow;

  // Preview
  RythmoWidget *m_previewWidget;

  // Styles
  QColor m_currentTextColor;
  QColor m_currentBackgroundColor;
};

#endif // TRACKSETTINGSDIALOG_H
