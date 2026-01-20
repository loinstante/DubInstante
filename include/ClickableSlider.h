#ifndef CLICKABLESLIDER_H
#define CLICKABLESLIDER_H

#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>

class ClickableSlider : public QSlider {
  Q_OBJECT

public:
  explicit ClickableSlider(Qt::Orientation orientation,
                           QWidget *parent = nullptr)
      : QSlider(orientation, parent) {}

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      QStyleOptionSlider opt;
      initStyleOption(&opt);

      QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt,
                                         QStyle::SC_SliderHandle, this);
      if (event->type() == QEvent::MouseButtonPress &&
          sr.contains(event->pos())) {
        // If they clicked on the handle, standard behavior is fine (dragging)
        QSlider::mousePressEvent(event);
        return;
      }

      // If they clicked on the groove, jump to that position
      int val = 0;
      if (orientation() == Qt::Vertical) {
        // Vertical logic if needed later
        double halfHandleHeight = (0.5 * sr.height()) + 0.5;
        int adaptedPosY = height() - event->pos().y();
        if (adaptedPosY < halfHandleHeight)
          adaptedPosY = halfHandleHeight;
        if (adaptedPosY > height() - halfHandleHeight)
          adaptedPosY = height() - halfHandleHeight;
        double newHeight = height() - halfHandleHeight * 2;
        double normalizedPosition =
            (adaptedPosY - halfHandleHeight) / newHeight;
        val = minimum() + (maximum() - minimum()) * normalizedPosition;
      } else {
        // Horizontal logic
        double halfHandleWidth = (0.5 * sr.width()) + 0.5;
        int adaptedPosX = event->pos().x();
        if (adaptedPosX < halfHandleWidth)
          adaptedPosX = halfHandleWidth;
        if (adaptedPosX > width() - halfHandleWidth)
          adaptedPosX = width() - halfHandleWidth;
        double newWidth = width() - halfHandleWidth * 2;
        double normalizedPosition = (adaptedPosX - halfHandleWidth) / newWidth;
        val = minimum() + (maximum() - minimum()) * normalizedPosition;
      }

      if (invertedAppearance()) {
        setValue(maximum() - val);
      } else {
        setValue(val);
      }

      // Important to accept the event so it doesn't propagate weirdly
      event->accept();

      // Emit needed signals
      emit sliderMoved(value());
      emit sliderPressed();
    } else {
      QSlider::mousePressEvent(event);
    }
  }
};

#endif // CLICKABLESLIDER_H
