#include "RythmoWidget.h"
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

RythmoWidget::RythmoWidget(QWidget *parent)
    : QWidget(parent), m_speed(100), m_currentPosition(0), m_text(""),
      m_fontSize(16), m_verticalPadding(4), m_textColor(QColor(34, 34, 34)),
      m_barColor(QColor(0, 0, 0, 0)), m_playingBarColor(QColor(0, 0, 0, 0)),
      m_isPlaying(false), m_lastMouseX(0) {
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TranslucentBackground);
}

// ============================================================================
// Helpers (Centralized logic)
// ============================================================================

QFont RythmoWidget::getFont() const {
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(m_fontSize);
  font.setBold(true);
  return font;
}

int RythmoWidget::charWidth() const {
  QFontMetrics fm(getFont());
  return fm.horizontalAdvance('A');
}

int RythmoWidget::cursorIndex() const {
  int cw = charWidth();
  if (cw <= 0)
    return 0;
  double distPixels = (double(m_currentPosition) / 1000.0) * m_speed;
  return static_cast<int>(distPixels / cw);
}

qint64 RythmoWidget::charDurationMs() const {
  int cw = charWidth();
  if (cw <= 0 || m_speed <= 0)
    return 40; // Fallback ~1 frame
  return static_cast<qint64>((double(cw) / m_speed) * 1000.0);
}

// ============================================================================
// Properties
// ============================================================================

void RythmoWidget::setSpeed(int speed) {
  if (m_speed != speed && speed > 0) {
    m_speed = speed;
    emit speedChanged(m_speed);
    update();
  }
}

int RythmoWidget::speed() const { return m_speed; }

void RythmoWidget::setText(const QString &text) {
  if (m_text != text) {
    m_text = text;
    update();
  }
}

QString RythmoWidget::text() const { return m_text; }

// ============================================================================
// Sync (Video position → Widget)
// ============================================================================

void RythmoWidget::sync(qint64 positionMs) {
  if (m_currentPosition != positionMs) {
    m_currentPosition = positionMs;
    update();
  }
}

void RythmoWidget::setPlaying(bool playing) {
  if (m_isPlaying != playing) {
    m_isPlaying = playing;
    update();
  }
}

// ============================================================================
// Resize
// ============================================================================

void RythmoWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

// ============================================================================
// Paint
// ============================================================================

void RythmoWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 1. Calculate dimensions
  int bandHeight = m_fontSize + m_verticalPadding * 2;
  int bandY = (height() - bandHeight) / 2;
  QRect bandRect(0, bandY, width(), bandHeight);

  // 2. Draw Band Background (purple when playing)
  QColor bgColor = m_isPlaying ? m_playingBarColor : m_barColor;
  painter.fillRect(bandRect, bgColor);

  // 3. Draw Target Line (Playhead position - 20% from left)
  int targetX = width() / 5;
  QPen targetPen(QColor(0, 120, 215), 2);
  targetPen.setStyle(Qt::DashLine);
  painter.setPen(targetPen);
  painter.drawLine(targetX, bandY, targetX, bandY + bandHeight);

  // 4. Draw Scrolling Text
  // Text starts at targetX when m_currentPosition = 0
  // As time advances, text scrolls left (negative shift)
  double pixelOffset = (double(m_currentPosition) / 1000.0) * m_speed;
  double textStartX = targetX - pixelOffset;

  QFont font = getFont();
  painter.setFont(font);
  painter.setPen(m_textColor);

  int textY = bandY + m_fontSize + (m_verticalPadding / 2) - 2;
  painter.drawText(QPointF(textStartX, textY), m_text);

  // 5. Draw Band Border
  painter.setPen(QPen(QColor(0, 120, 215), 2));
  painter.drawRect(bandRect);

  // 6. Draw Edit Cursor (Orange Playhead)
  int cw = charWidth();
  if (cw > 0) {
    int idx = cursorIndex();
    double cursorScreenX = textStartX + (idx * cw);

    // Vertical Line
    QPen cursorPen(QColor(0, 120, 215), 2);
    painter.setPen(cursorPen);
    painter.drawLine(cursorScreenX, bandY - 5, cursorScreenX,
                     bandY + bandHeight + 5);

    // Triangle Handle
    QPolygon tri;
    tri << QPoint(cursorScreenX, bandY - 5)
        << QPoint(cursorScreenX - 4, bandY - 10)
        << QPoint(cursorScreenX + 4, bandY - 10);
    painter.setBrush(QColor(0, 120, 215));
    painter.drawPolygon(tri);

    // Timestamp Label
    int mm = (m_currentPosition / 60000) % 60;
    int ss = (m_currentPosition / 1000) % 60;
    int ms = m_currentPosition % 1000;
    QString timeStr = QString("%1:%2.%3")
                          .arg(mm, 2, 10, QChar('0'))
                          .arg(ss, 2, 10, QChar('0'))
                          .arg(ms, 3, 10, QChar('0'));

    painter.setPen(QColor(34, 34, 34));
    QFont smallFont("Segoe UI", 8, QFont::Bold);
    painter.setFont(smallFont);
    int tw = painter.fontMetrics().horizontalAdvance(timeStr);
    painter.drawText(cursorScreenX - tw / 2, bandY - 15, timeStr);
  }
}

// ============================================================================
// Mouse Events
// ============================================================================

void RythmoWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  m_lastMouseX = event->pos().x();

  int targetX = width() / 5;
  int clickX = event->pos().x();

  // Pixel-perfect seek
  // The text is shifted by pixelOffset = (currentPosition / 1000) * speed
  // Visually: character at 'time' is at: targetX - pixelOffset + (time/1000 *
  // speed) Actually simpler: Visual X = targetX - (currentPos - time) *
  // (speed/1000) We want to find 'time' such that Visual X = clickX. clickX -
  // targetX = -(currentPos - time) * (speed/1000) (clickX - targetX) /
  // (speed/1000) = time - currentPos time = currentPos + (clickX - targetX) *
  // 1000 / speed

  double timeDeltaMs = (double(clickX - targetX) * 1000.0) / m_speed;
  qint64 newTime = m_currentPosition + static_cast<qint64>(timeDeltaMs);

  emit scrubRequested(std::max(qint64(0), newTime));
  setFocus();
}

void RythmoWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton))
    return;

  int currentX = event->pos().x();
  int deltaX = currentX - m_lastMouseX;
  m_lastMouseX = currentX;

  // Dragging logic:
  // If I drag mouse to the right (positive delta), I am pulling the "paper" to
  // the right. This means I am seeing earlier time (rewinding). Visual shift =
  // deltaX. Time shift = (deltaX / speed) * 1000. changing time by -TimeShift.

  double timeDeltaMs = (double(deltaX) * 1000.0) / m_speed;
  qint64 newTime = m_currentPosition - static_cast<qint64>(timeDeltaMs);

  emit scrubRequested(std::max(qint64(0), newTime));
}

void RythmoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  // Treat as single click (don't call mousePressEvent twice!)
  mousePressEvent(event);
}

// ============================================================================
// Keyboard Events
// ============================================================================

void RythmoWidget::keyPressEvent(QKeyEvent *event) {
  qint64 step = charDurationMs();

  // Navigation
  if (event->key() == Qt::Key_Left) {
    emit scrubRequested(std::max(qint64(0), m_currentPosition - step));
    return;
  }
  if (event->key() == Qt::Key_Right) {
    emit scrubRequested(m_currentPosition + step);
    return;
  }

  // Escape: Insert space (push text) and play
  if (event->key() == Qt::Key_Escape) {
    int idx = cursorIndex();
    while (m_text.length() < idx) {
      m_text.append(' ');
    }
    m_text.insert(idx, ' ');
    emit scrubRequested(m_currentPosition + step);
    emit playRequested();
    return;
  }

  // Text Editing
  int idx = cursorIndex();

  if (event->key() == Qt::Key_Backspace) {
    if (idx > 0 && idx <= m_text.length()) {
      m_text.remove(idx - 1, 1);
      emit scrubRequested(std::max(qint64(0), m_currentPosition - step));
    }
    return;
  }

  if (event->key() == Qt::Key_Delete) {
    if (idx >= 0 && idx < m_text.length()) {
      m_text.remove(idx, 1);
      update();
    }
    return;
  }

  // Printable Characters
  if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
    // Pad with spaces if needed
    while (m_text.length() < idx) {
      m_text.append(' ');
    }
    m_text.insert(idx, event->text());
    emit scrubRequested(m_currentPosition + step);
  }
}
