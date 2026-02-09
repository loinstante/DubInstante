/**
 * @file ClickableSlider.h
 * @brief Custom slider with click-to-seek behavior.
 * 
 * Standard QSlider requires dragging the handle. This subclass allows
 * clicking anywhere on the groove to jump to that position immediately.
 * 
 * @note Part of the GUI layer - header-only implementation.
 */

#ifndef CLICKABLESLIDER_H
#define CLICKABLESLIDER_H

#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>

/**
 * @class ClickableSlider
 * @brief QSlider subclass with click-to-position functionality.
 * 
 * Features:
 * - Click anywhere on groove to jump to that position
 * - Standard drag behavior still works on handle
 * - Supports both horizontal and vertical orientations
 */
class ClickableSlider : public QSlider {
    Q_OBJECT

public:
    explicit ClickableSlider(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSlider(orientation, parent)
    {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QSlider::mousePressEvent(event);
            return;
        }
        
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        
        // Get the handle rect
        QRect handleRect = style()->subControlRect(
            QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        
        // If clicked on handle, use standard behavior
        if (handleRect.contains(event->pos())) {
            QSlider::mousePressEvent(event);
            return;
        }
        
        // Calculate new value based on click position
        int newValue = calculateValueFromPosition(event->pos(), handleRect);
        
        if (invertedAppearance()) {
            setValue(maximum() - newValue + minimum());
        } else {
            setValue(newValue);
        }
        
        event->accept();
        emit sliderMoved(value());
        emit sliderPressed();
    }

private:
    int calculateValueFromPosition(const QPoint &pos, const QRect &handleRect) const
    {
        int val = 0;
        
        if (orientation() == Qt::Horizontal) {
            double halfHandleWidth = (0.5 * handleRect.width()) + 0.5;
            int adaptedPosX = pos.x();
            
            // Clamp to valid range
            adaptedPosX = qBound(
                static_cast<int>(halfHandleWidth),
                adaptedPosX,
                static_cast<int>(width() - halfHandleWidth)
            );
            
            double usableWidth = width() - halfHandleWidth * 2;
            double normalizedPosition = (adaptedPosX - halfHandleWidth) / usableWidth;
            val = minimum() + static_cast<int>((maximum() - minimum()) * normalizedPosition);
        } else {
            // Vertical slider (inverted Y axis)
            double halfHandleHeight = (0.5 * handleRect.height()) + 0.5;
            int adaptedPosY = height() - pos.y();
            
            adaptedPosY = qBound(
                static_cast<int>(halfHandleHeight),
                adaptedPosY,
                static_cast<int>(height() - halfHandleHeight)
            );
            
            double usableHeight = height() - halfHandleHeight * 2;
            double normalizedPosition = (adaptedPosY - halfHandleHeight) / usableHeight;
            val = minimum() + static_cast<int>((maximum() - minimum()) * normalizedPosition);
        }
        
        return val;
    }
};

#endif // CLICKABLESLIDER_H
