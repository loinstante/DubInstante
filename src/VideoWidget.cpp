#include "VideoWidget.h"
#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_videoSink(new QVideoSink(this)) {
  setAttribute(Qt::WA_OpaquePaintEvent);

  connect(m_videoSink, &QVideoSink::videoFrameChanged, this,
          &VideoWidget::handleFrame);
}

QVideoSink *VideoWidget::videoSink() const { return m_videoSink; }

QSize VideoWidget::sizeHint() const { return QSize(640, 360); }

bool VideoWidget::hasHeightForWidth() const { return true; }

int VideoWidget::heightForWidth(int width) const { return width * 9 / 16; }

QRect VideoWidget::videoRect() const {
  if (m_currentImage.isNull()) {
    return rect();
  }

  QSize imageSize = m_currentImage.size();
  QSize widgetSize = size();

  float imageRatio = static_cast<float>(imageSize.width()) / imageSize.height();
  float widgetRatio =
      static_cast<float>(widgetSize.width()) / widgetSize.height();

  if (imageRatio > widgetRatio) {
    int targetHeight = static_cast<int>(widgetSize.width() / imageRatio);
    return QRect(0, (widgetSize.height() - targetHeight) / 2,
                 widgetSize.width(), targetHeight);
  } else {
    int targetWidth = static_cast<int>(widgetSize.height() * imageRatio);
    return QRect((widgetSize.width() - targetWidth) / 2, 0, targetWidth,
                 widgetSize.height());
  }
}

void VideoWidget::handleFrame(const QVideoFrame &frame) {
  if (!frame.isValid()) {
    return;
  }

  // Make a copy so we can map it
  QVideoFrame clonedFrame(frame);

  if (!clonedFrame.map(QVideoFrame::ReadOnly)) {
    return;
  }

  // Convert to image
  QImage img = clonedFrame.toImage();
  clonedFrame.unmap();

  if (img.isNull()) {
    return;
  }

  // Store the image and trigger repaint
  m_currentImage = img.convertToFormat(QImage::Format_RGB888);
  update();
}

void VideoWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  if (m_currentImage.isNull()) {
    painter.fillRect(rect(), QColor(240, 240, 240));
    painter.setPen(QColor(150, 150, 150));
    painter.setFont(QFont("Segoe UI", 12));
    painter.drawText(rect(), Qt::AlignCenter, "Aucune vidéo chargée");
    return;
  }

  // Calculate aspect ratio scaling
  QSize imageSize = m_currentImage.size();
  QSize widgetSize = size();

  float imageRatio = static_cast<float>(imageSize.width()) / imageSize.height();
  float widgetRatio =
      static_cast<float>(widgetSize.width()) / widgetSize.height();

  QRect targetRect;
  if (imageRatio > widgetRatio) {
    // Image is wider than widget
    int targetHeight = static_cast<int>(widgetSize.width() / imageRatio);
    targetRect = QRect(0, (widgetSize.height() - targetHeight) / 2,
                       widgetSize.width(), targetHeight);
  } else {
    // Image is taller than widget
    int targetWidth = static_cast<int>(widgetSize.height() * imageRatio);
    targetRect = QRect((widgetSize.width() - targetWidth) / 2, 0, targetWidth,
                       widgetSize.height());
  }
  // Fill with black first (for letterboxing/pillarboxing)
  painter.fillRect(rect(), Qt::black);

  painter.drawImage(targetRect, m_currentImage);
}
