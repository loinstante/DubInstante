#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QVideoFrame>
#include <QVideoSink>

/**
 * @brief VideoWidget uses QOpenGLWidget for GPU-accelerated video rendering.
 * Maintains aspect ratio scaling and handles video frames from QVideoSink.
 */
class VideoWidget : public QOpenGLWidget {
  Q_OBJECT

public:
  explicit VideoWidget(QWidget *parent = nullptr);
  ~VideoWidget() override = default;

  QVideoSink *videoSink() const;
  QRect videoRect() const; // Returns the actual video content bounds

  QSize sizeHint() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void handleFrame(const QVideoFrame &frame);

private:
  QVideoSink *m_videoSink;
  QImage m_currentImage;
};

#endif // VIDEOWIDGET_H
