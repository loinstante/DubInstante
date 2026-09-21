/**
 * @file VideoWidget.h
 * @brief Video rendering widget (QPainter on a QOpenGLWidget).
 * 
 * This widget receives video frames via QVideoSink. The QVideoFrame -> QImage
 * conversion and the scaling run on the GUI thread: costly in HD, to be
 * replaced by a texture upload (post-beta). It maintains aspect ratio.
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
 * @brief Video display widget (software QPainter path).
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

public slots:
    void forceFrame(const QImage &image);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void handleFrame(const QVideoFrame &frame);

private:
    QVideoSink *m_videoSink;
    QImage m_currentImage;
};

#endif // VIDEOWIDGET_H
