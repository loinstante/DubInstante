#ifndef AUDIOMETERWIDGET_H
#define AUDIOMETERWIDGET_H

#include <QWidget>
#include <QTimer>

class AudioMeterWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float level READ level WRITE setLevel)

public:
    explicit AudioMeterWidget(QWidget *parent = nullptr);
    ~AudioMeterWidget() override = default;

    float level() const { return m_level; }
    void setLevel(float level);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void updateAnimation();

private:
    float m_level;          // Current actual audio level (0.0 to 1.0)
    float m_displayedLevel; // Smoothly decaying level for visual representation
    float m_peakLevel;      // Max peak level
    int m_peakHoldTimer;    // Frames left to hold the peak before decaying

    QTimer m_animationTimer;
};

#endif // AUDIOMETERWIDGET_H
