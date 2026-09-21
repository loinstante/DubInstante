#include "AudioMeterWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QPaintEvent>

AudioMeterWidget::AudioMeterWidget(QWidget *parent)
    : QWidget(parent)
    , m_level(0.0f)
    , m_displayedLevel(0.0f)
    , m_peakLevel(0.0f)
    , m_peakHoldTimer(0)
{
    // Configure default properties
    setMinimumHeight(12);
    setMaximumHeight(18);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Setup animation timer (approx 60 FPS)
    connect(&m_animationTimer, &QTimer::timeout, this, &AudioMeterWidget::updateAnimation);
    // Timer runs only while visible (see showEvent/hideEvent)
}

void AudioMeterWidget::showEvent(QShowEvent *event)
{
    m_animationTimer.start(16);
    QWidget::showEvent(event);
}

void AudioMeterWidget::hideEvent(QHideEvent *event)
{
    m_animationTimer.stop();
    QWidget::hideEvent(event);
}

void AudioMeterWidget::setLevel(float level)
{
    float clampedLevel = qBound(0.0f, level, 1.0f);
    m_level = clampedLevel;

    // Immediately jump up if the new level is higher
    if (m_level > m_displayedLevel) {
        m_displayedLevel = m_level;
    }

    // Handle peak logic
    if (m_level >= m_peakLevel) {
        m_peakLevel = m_level;
        m_peakHoldTimer = 60; // Hold peak for approx 1 second (60 frames at 16ms)
    }

    // The timer stops itself once everything has settled
    if (isVisible() && !m_animationTimer.isActive()) {
        m_animationTimer.start(16);
    }
}

void AudioMeterWidget::updateAnimation()
{
    bool needsUpdate = false;

    // Smoothly decay displayed level (falling meter)
    if (m_displayedLevel > m_level) {
        m_displayedLevel -= 0.02f; // Decay rate
        if (m_displayedLevel < m_level) {
            m_displayedLevel = m_level;
        }
        needsUpdate = true;
    }

    // Handle peak hold and decay
    if (m_peakHoldTimer > 0) {
        m_peakHoldTimer--;
        if (m_peakLevel > 0.0f) {
            needsUpdate = true; // Keep drawing if peak is visible
        }
    } else if (m_peakLevel > m_level) {
        m_peakLevel -= 0.005f; // Slow decay for peak indicator
        if (m_peakLevel < m_level) {
            m_peakLevel = m_level;
        }
        needsUpdate = true;
    }

    if (needsUpdate || m_level > 0.0f) {
        update();
    } else {
        // Nothing left to animate on a silent track: setLevel() restarts it
        m_animationTimer.stop();
    }
}

void AudioMeterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    if (!painter.isActive()) {
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();

    // 1. Draw Background (dark gray/black with slight border radius)
    QColor bgColor(25, 25, 25);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(rect, 3, 3);

    // 2. Draw active meter bar
    if (m_displayedLevel > 0.01f) {
        QRectF fillRect(rect.left(), rect.top(), rect.width() * m_displayedLevel, rect.height());

        // Create continuous gradient: Green -> Yellow -> Red
        QLinearGradient gradient(rect.topLeft(), rect.topRight());
        gradient.setColorAt(0.0, QColor(40, 200, 40));       // Green start
        gradient.setColorAt(0.7, QColor(220, 200, 40));      // Yellow at 70%
        gradient.setColorAt(0.9, QColor(220, 40, 40));       // Red at 90%
        gradient.setColorAt(1.0, QColor(255, 0, 0));         // Solid red end

        painter.setBrush(gradient);
        
        // Clip to rounded rect shape
        QPainterPath path;
        path.addRoundedRect(fillRect, 3, 3);
        painter.drawPath(path);
    }

    // 3. Draw Peak Indicator Line
    if (m_peakLevel > 0.01f) {
        float peakX = rect.left() + rect.width() * m_peakLevel;
        // Ensure peak line stays within bounds
        peakX = qMin(peakX, static_cast<float>(rect.right() - 2.0f)); 

        painter.setPen(QPen(QColor(255, 255, 255, 200), 2));
        painter.drawLine(QPointF(peakX, rect.top()), QPointF(peakX, rect.bottom()));
    }
}
