/**
 * @file VideoWidget.cpp
 * @brief Implementation of the VideoWidget class.
 */

#include "VideoWidget.h"

#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_videoSink(new QVideoSink(this))
{
    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &VideoWidget::handleFrame);
    
    // Set black background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
}

QVideoSink *VideoWidget::videoSink() const
{
    return m_videoSink;
}

void VideoWidget::handleFrame(const QVideoFrame &frame)
{
    if (!frame.isValid()) {
        return;
    }
    
    // Convert frame to QImage for rendering
    QVideoFrame localFrame = frame;
    if (localFrame.map(QVideoFrame::ReadOnly)) {
        m_currentImage = localFrame.toImage();
        localFrame.unmap();
        update();  // Trigger repaint
    }
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    if (!m_currentImage.isNull()) {
        // Calculate scaled rect maintaining aspect ratio
        QSize imageSize = m_currentImage.size();
        QSize widgetSize = size();
        
        QSize scaledSize = imageSize.scaled(widgetSize, Qt::KeepAspectRatio);
        
        int x = (widgetSize.width() - scaledSize.width()) / 2;
        int y = (widgetSize.height() - scaledSize.height()) / 2;
        
        QRect targetRect(x, y, scaledSize.width(), scaledSize.height());
        
        // Clear background (letterbox)
        painter.fillRect(rect(), Qt::black);
        
        // Draw the video frame
        painter.drawImage(targetRect, m_currentImage);
    } else {
        // No frame yet - show black
        painter.fillRect(rect(), Qt::black);
    }
}
