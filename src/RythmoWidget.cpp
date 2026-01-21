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

// Visual Style
void RythmoWidget::setVisualStyle(VisualStyle style) {
  if (m_visualStyle != style) {
    m_visualStyle = style;
    updateGeometry(); // Trigger re-layout
    update();
  }
}

RythmoWidget::VisualStyle RythmoWidget::visualStyle() const {
  return m_visualStyle;
}

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

QSize RythmoWidget::sizeHint() const {
  // Base band height (slim look)
  int h = 35;

  // Add Header space for Handle/Timestamp if needed
  if (m_visualStyle == Standalone || m_visualStyle == UnifiedTop) {
    h += 25; // 25px Header (Increased from 15 to fix text clipping)
  }

  return QSize(QWidget::sizeHint().width(), h);
}

// ============================================================================
// Paint
// ============================================================================

void RythmoWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 1. Calculate Layout Dimensions
  int headerHeight = 0;
  if (m_visualStyle == Standalone || m_visualStyle == UnifiedTop) {
    headerHeight = 25; // Matching sizeHint (Increased from 15)
  }

  int bandHeight = height() - headerHeight;
  int bandY = headerHeight;
  QRect bandRect(0, bandY, width(), bandHeight);

  // 2. Draw Band Background
  QColor bgColor = m_isPlaying ? m_playingBarColor : m_barColor;
  painter.fillRect(bandRect, bgColor);

  // 3. Draw Scrolling Text (Layer 1)
  double pixelOffset = (double(m_currentPosition) / 1000.0) * m_speed;
  int targetX = width() / 5;
  double textStartX = targetX - pixelOffset;

  QFont font = getFont();
  painter.setFont(font);
  painter.setPen(m_textColor);

  // Vertically center text in the BAND area
  int textY = bandY + (bandHeight + m_fontSize) / 2 - 2;
  painter.drawText(QPointF(textStartX, textY), m_text);

  // 4. Draw Band Border (Layer 2)
  QPen borderPen(QColor(0, 120, 215), 2);
  painter.setPen(borderPen);

  if (m_visualStyle == Standalone) {
    painter.drawRect(bandRect);
  } else if (m_visualStyle == UnifiedTop) {
    painter.drawRect(bandRect);
  } else if (m_visualStyle == UnifiedBottom) {
    painter.drawRect(bandRect);
  }

  // 5. Draw Target Line (Guide)
  QPen targetPen(QColor(0, 120, 215), 2);
  targetPen.setStyle(Qt::DashLine);
  painter.setPen(targetPen);
  painter.drawLine(targetX, bandY, targetX, bandY + bandHeight);

  // 6. Draw Edit Cursor (Unified Blue Line)
  int cw = charWidth();
  if (cw > 0) {
    int idx = cursorIndex();
    double cursorScreenX = textStartX + (idx * cw);

    bool drawHandle =
        (m_visualStyle == Standalone || m_visualStyle == UnifiedTop);
    bool drawLabel =
        (m_visualStyle == Standalone || m_visualStyle == UnifiedTop);

    // Line Geometry
    int lineTop = bandY;
    int lineBottom = bandY + bandHeight;

    // Adjust line for Unified look
    if (m_visualStyle == UnifiedTop)
      lineBottom += 2;
    if (m_visualStyle == UnifiedBottom)
      lineTop -= 2;

    // Vertical Line
    QPen cursorPen(QColor(0, 120, 215), 3);
    painter.setPen(cursorPen);
    painter.drawLine(cursorScreenX, lineTop, cursorScreenX, lineBottom);

    // Triangle Handle (In Header Area)
    if (drawHandle) {
      // Draw handle at bottom of header, pointing down to band
      // Handle height ~10px.
      // Position: 15 to 25.
      QPolygon tri;
      tri << QPoint(cursorScreenX, bandY)           // Point at thread (25)
          << QPoint(cursorScreenX - 5, bandY - 10)  // Left Top (15)
          << QPoint(cursorScreenX + 5, bandY - 10); // Right Top (15)
      painter.setBrush(QColor(0, 120, 215));
      painter.drawPolygon(tri);
    }

    if (drawLabel) {
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
      // Draw centered above handle in the 15px space
      // Header available: 0..25.
      // Handle occupies 15..25.
      // Text space: 0..15.
      // Baseline at 12 should fit nicely directly under the top edge.
      painter.drawText(cursorScreenX - tw / 2, bandY - 12, timeStr);
    }
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

  // Dragging logic
  double timeDeltaMs = (double(deltaX) * 1000.0) / m_speed;
  qint64 newTime = m_currentPosition - static_cast<qint64>(timeDeltaMs);

  emit scrubRequested(std::max(qint64(0), newTime));
}

void RythmoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
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
