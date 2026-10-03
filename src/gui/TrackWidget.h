#ifndef TRACKWIDGET_H
#define TRACKWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QFrame>
#include <QAudioDevice>

class TrackWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit TrackWidget(int trackIndex, const QString& title, QWidget *parent = nullptr);
    ~TrackWidget() override = default;

    int trackIndex() const { return m_trackIndex; }
    void setVuLevel(int percentage); // 0 to 100
    void populateInputDevices(const QList<QAudioDevice> &devices);
    
    /// Device description, or empty for "no input" (never the translated label)
    QString currentInputDevice() const {
        return m_inputCombo->currentIndex() > 0 ? m_inputCombo->currentText() : QString();
    }
    int currentVolume() const { return m_volumeSlider->value(); }
    bool isArmed() const { return m_recordArmButton->isChecked(); }
    void setArmed(bool armed) { m_recordArmButton->setChecked(armed); }
    void setRecordingState(const QString &state);
    void setInputDevice(const QString& device);
    void setVolume(int volume);

signals:
    void optionsClicked();
    void inputDeviceIndexChanged(int deviceIndex);
    void volumeChanged(int volume);

private slots:
    void onVolumeSliderChanged(int value);

private:
    void setupUi(const QString& title);
    void setupConnections();

    int m_trackIndex;

    // UI Components
    QLabel *m_titleLabel;
    QPushButton *m_optionsButton;
    QComboBox *m_inputCombo;
    QSlider *m_volumeSlider;
    QPushButton *m_recordArmButton;
    QLabel *m_recordingStateLabel;
    
    // Custom VU Meter
    class AudioMeterWidget *m_vuMeter;
};

#endif // TRACKWIDGET_H
