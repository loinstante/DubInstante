/**
 * @file VideoWidget.h
 * @brief OpenGL-accelerated video rendering widget.
 * 
 * This widget receives video frames via QVideoSink and renders them
 * using QOpenGLWidget for GPU acceleration. It maintains aspect ratio
 * and handles frame scaling automatically.
 * 
 * @note Part of the GUI layer - pure rendering, no business logic.
 */

#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QImage>
#include <QOpenGLWidget>
#include <QVideoFrame>
#include <QVideoSink>

/**
 * @class VideoWidget
 * @brief GPU-accelerated video display widget.
 * 
 * Usage:
 * 1. Create the widget
 * 2. Pass videoSink() to PlaybackEngine::setVideoSink()
 * 3. Widget automatically displays frames
 */
class VideoWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override = default;

    /**
     * @brief Returns the video sink for connecting to media player.
     * @return Pointer to internal QVideoSink.
     */
    QVideoSink *videoSink() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void handleFrame(const QVideoFrame &frame);

private:
    QVideoSink *m_videoSink;
    QImage m_currentImage;
};

#endif // VIDEOWIDGET_H
