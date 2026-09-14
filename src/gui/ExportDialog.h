#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include "../core/ExportService.h"
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QSlider>
#include <QVector>
#include <QVBoxLayout>
#include <QTabWidget>

class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(const QString &sourceVideo,
                          const QString &primaryAudio,
                          const QStringList &extraAudios,
                          qint64 lastRecordedDurationMs,
                          qint64 lastRecordedStartMs,
                          const QVector<qint64> &trackOffsetsMs,
                          float currentOriginalVolume,
                          const QVector<float> &currentTrackVolumes,
                          const QVector<bool> &currentTrackMutes,
                          QWidget *parent = nullptr);
    ~ExportDialog() override = default;

    ExportConfig exportConfig() const;

private slots:
    void onFormatChanged(int index);
    void onBrowseClicked();
    void onAdvancedToggled(bool checked);
    void validateSettings();
    void onVolumeSliderChanged(int value);
    void onMuteToggled();

    // Expert Mode slots
    void onResolutionSelectionChanged(int index);
    void onRateControlSelectionChanged(int index);
    void onCustomResolutionWidthChanged(int width);
    void onCustomResolutionHeightChanged(int height);
    void onCodecChanged();

private:
    void setupUi();
    void populateFields();
    void updateWarningText(const QString &warning);
    void addTrackRow(QWidget *parent, const QString &title, int index);

    // Initial parameters passed from Main Window
    QString m_sourceVideoPath;
    QString m_primaryAudioPath;
    QStringList m_extraAudioPaths;
    qint64 m_lastRecordedDurationMs;
    qint64 m_lastRecordedStartMs;
    QVector<qint64> m_trackOffsetsMs;
    
    float m_defaultOriginalVolume;
    QVector<float> m_defaultTrackVolumes;
    QVector<bool> m_defaultTrackMutes;

    // Track original size to calculate custom resolution aspect ratio
    int m_videoOriginalWidth;
    int m_videoOriginalHeight;
    float m_videoAspectRatio;
    bool m_updatingCustomRes;

    // UI Components
    QLineEdit *m_outputPathEdit;
    QPushButton *m_browseButton;
    
    QCheckBox *m_advancedCheck;
    QLabel *m_warningLabel;

    // Dual Interface Containers
    QWidget *m_basicWidget;
    QTabWidget *m_expertWidget;

    // 1. Basic Widgets
    QComboBox *m_formatCombo;
    QComboBox *m_resolutionCombo;
    QComboBox *m_videoQualityCombo;
    QComboBox *m_audioQualityCombo;
    QComboBox *m_rangeCombo;
    
    // 2. Expert Widgets
    QComboBox *m_expFormatCombo;
    QComboBox *m_expVideoCodecCombo;
    QComboBox *m_expAudioCodecCombo;
    
    QComboBox *m_expResolutionCombo;
    QWidget *m_expCustomResContainer;
    QSpinBox *m_customWidthSpin;
    QSpinBox *m_customHeightSpin;
    QCheckBox *m_aspectLockCheck;
    
    QComboBox *m_expRateControlCombo;
    QWidget *m_expCrfContainer;
    QSlider *m_expCrfSlider;
    QLabel *m_expCrfPercentLabel;
    
    QWidget *m_expBitrateContainer;
    QSpinBox *m_expBitrateSpin;
    
    QComboBox *m_expPresetCombo;
    QSpinBox *m_expAudioBitrateSpin;
    QComboBox *m_expSampleRateCombo;
    QLineEdit *m_expCustomFlagsEdit;

    // Audio mixing panel
    QVBoxLayout *m_audioTracksLayout;
    
    QSlider *m_originalVolumeSlider;
    QLabel *m_originalVolPercentLabel;
    QPushButton *m_originalMuteBtn;
    
    QVector<QSlider *> m_trackSliders;
    QVector<QLabel *> m_trackPercentLabels;
    QVector<QPushButton *> m_trackMuteBtns;

    QPushButton *m_btnExport;
    QPushButton *m_btnCancel;
};

#endif // EXPORTDIALOG_H
