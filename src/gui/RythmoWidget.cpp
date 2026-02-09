/**
 * @file RythmoWidget.cpp
 * @brief Implementation of the RythmoWidget class.
 */

#include "RythmoWidget.h"

#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <QDateTime>
#include <algorithm>

RythmoWidget::RythmoWidget(QWidget *parent)
    : QWidget(parent), m_cursorIndex(0), m_currentPosition(0), m_speed(100),
      m_isPlaying(false), m_visualStyle(Standalone), m_fontSize(16),
      m_verticalPadding(4), m_textColor(QColor(34, 34, 34)),
      m_barColor(QColor(0, 0, 0, 0)), m_playingBarColor(QColor(0, 0, 0, 0)),
      m_lastMouseX(0), m_cachedCharWidth(-1), m_seekTimer(new QTimer(this)),
      m_pendingSeekPosition(0), m_animationTimer(new QTimer(this)),
      m_lastSyncPosition(0), m_lastSyncTime(0) {
  m_seekTimer->setSingleShot(true);
  connect(m_seekTimer, &QTimer::timeout, this, &RythmoWidget::triggerSeek);

  // 60 FPS animation loop
  m_animationTimer->setInterval(16);
  connect(m_animationTimer, &QTimer::timeout, this, &RythmoWidget::animate);

  setAutoFillBackground(false);
  setAttribute(Qt::WA_TranslucentBackground);
  setFocusPolicy(Qt::StrongFocus);
}

// =============================================================================
// Configuration
// =============================================================================

void RythmoWidget::setVisualStyle(VisualStyle style) {
  if (m_visualStyle != style) {
    m_visualStyle = style;
    updateGeometry();
    update();
  }
}

RythmoWidget::VisualStyle RythmoWidget::visualStyle() const {
  return m_visualStyle;
}

void RythmoWidget::setTextColor(const QColor &color) {
  if (m_textColor != color) {
    m_textColor = color;
    update();
  }
}

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

// =============================================================================
// Data Input Slots
// =============================================================================

void RythmoWidget::updateDisplay(int cursorIndex, qint64 positionMs,
                                 const QString &text, int speed) {
  m_cursorIndex = cursorIndex;
  m_currentPosition = positionMs;
  m_text = text;
  m_speed = speed;
  update();
}

void RythmoWidget::updatePosition(int cursorIndex, qint64 positionMs) {
  m_cursorIndex = cursorIndex;
  m_currentPosition = positionMs;
  update();
}

void RythmoWidget::setPlaying(bool playing) {
  if (m_isPlaying != playing) {
    m_isPlaying = playing;

    if (m_isPlaying) {
      // Start animation loop
      m_lastSyncPosition = m_currentPosition;
      m_lastSyncTime = QDateTime::currentMSecsSinceEpoch();
      m_animationTimer->start();
    } else {
      // Stop animation loop
      m_animationTimer->stop();
      // Force one last update
      update();
    }
  }
}

void RythmoWidget::sync(qint64 positionMs) {
  // Always update anchor points for interpolation
  m_lastSyncPosition = positionMs;
  m_lastSyncTime = QDateTime::currentMSecsSinceEpoch();

  // If not animating (paused), update strictly to valid position
  if (!m_isPlaying) {
    if (m_currentPosition != positionMs) {
      m_currentPosition = positionMs;
      // Recalculate cursor index locally (for backward compatibility)
      int cw = charWidth();
      if (cw > 0) {
        double distPixels =
            (static_cast<double>(positionMs) / 1000.0) * m_speed;
        m_cursorIndex = static_cast<int>(distPixels / cw);
      }
      update();
    }
  }
}

void RythmoWidget::animate() {
  if (!m_isPlaying)
    return;

  qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 elapsed = now - m_lastSyncTime;

  // Extrapolate position based on time elapsed since last sync
  m_currentPosition = m_lastSyncPosition + elapsed;

  // Note: m_cursorIndex will be calculated on-the-fly in paintEvent
  // via cursorIndex() method, or we could update it here if needed.
  // Given paintEvent uses cursorIndex(), just updating m_currentPosition is
  // enough.

  update();
}

// =============================================================================
// Helpers
// =============================================================================

QFont RythmoWidget::getFont() const {
  if (m_cachedCharWidth == -1) {
    m_cachedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_cachedFont.setPointSize(m_fontSize);
    m_cachedFont.setBold(true);
  }
  return m_cachedFont;
}

int RythmoWidget::charWidth() const {
  if (m_cachedCharWidth == -1) {
    QFontMetrics fm(getFont());
    m_cachedCharWidth = fm.horizontalAdvance('A');
  }
  return m_cachedCharWidth;
}

int RythmoWidget::cursorIndex() const {
  int cw = charWidth();
  if (cw <= 0)
    return 0;
  double distPixels = (double(m_currentPosition) / 1000.0) * m_speed;
  return qRound(distPixels / cw); // Round to nearest char for intuitive snap
}

qint64 RythmoWidget::charDurationMs() const {
  int cw = charWidth();
  if (cw <= 0 || m_speed <= 0)
    return 40; // Fallback ~1 frame
  return static_cast<qint64>((double(cw) / m_speed) * 1000.0);
}

void RythmoWidget::requestDebouncedSeek(qint64 positionMs) {
  m_pendingSeekPosition = positionMs;
  m_currentPosition = positionMs;
  // Recalculate cursor for immediate visual feedback
  int cw = charWidth();
  if (cw > 0) {
    double distPixels = (static_cast<double>(positionMs) / 1000.0) * m_speed;
    m_cursorIndex = static_cast<int>(distPixels / cw);
  }
  update();
  m_seekTimer->start(200); // 200ms debounce
}

void RythmoWidget::triggerSeek() { emit seekRequested(m_pendingSeekPosition); }

// =============================================================================
// Size Hint
// =============================================================================

QSize RythmoWidget::sizeHint() const {
  int h = 35; // Base band height

  if (m_visualStyle == Standalone || m_visualStyle == UnifiedTop) {
    h += 25; // Header space for handle/timestamp
  }

  return QSize(QWidget::sizeHint().width(), h);
}

// =============================================================================
// Paint Event
// =============================================================================

void RythmoWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 1. Calculate layout dimensions
  int headerHeight = 0;
  if (m_visualStyle == Standalone || m_visualStyle == UnifiedTop) {
    headerHeight = 25;
  }

  int bandHeight = height() - headerHeight;
  int bandY = headerHeight;
  QRect bandRect(0, bandY, width(), bandHeight);

  // 2. Calculate drawing parameters
  int cw = charWidth();
  int targetX = width() / 5; // Target line position

  double pixelOffset;
  if (m_isPlaying && cw > 0) {
    // Smooth scrolling using continuous position
    pixelOffset = (static_cast<double>(m_currentPosition) / 1000.0) * m_speed;
  } else {
    // Snap to character grid for precise editing alignment when paused
    pixelOffset = static_cast<double>(cursorIndex() * cw);
  }

  double textStartX = targetX - pixelOffset;

  // 3. Draw band background
  QColor bgColor = m_isPlaying ? m_playingBarColor : m_barColor;
  painter.fillRect(bandRect, bgColor);

  // 4. Draw scrolling text (virtualized for performance)
  if (cw > 0 && !m_text.isEmpty()) {
    QFont font = getFont();
    painter.setFont(font);
    painter.setPen(m_textColor);

    int textY = bandY + (bandHeight + m_fontSize) / 2 - 2;

    // Only render visible characters
    int firstVisibleIdx = std::max(0, static_cast<int>(-textStartX / cw));
    int lastVisibleIdx =
        std::min(static_cast<int>(m_text.length()),
                 static_cast<int>((width() - textStartX) / cw) + 1);

    if (firstVisibleIdx < lastVisibleIdx) {
      QString visibleText =
          m_text.mid(firstVisibleIdx, lastVisibleIdx - firstVisibleIdx);
      painter.drawText(QPointF(textStartX + (firstVisibleIdx * cw), textY),
                       visibleText);
    }
  }

  // 5. Draw band border
  QPen borderPen(QColor(0, 120, 215), 2);
  painter.setPen(borderPen);
  painter.drawRect(bandRect);

  // 6. Draw target line (guide)
  QPen targetPen(QColor(0, 120, 215), 2);
  targetPen.setStyle(Qt::DashLine);
  painter.setPen(targetPen);
  painter.drawLine(targetX, bandY, targetX, bandY + bandHeight);

  // 7. Draw edit cursor (always at targetX to align with playback line)
  if (cw > 0) {
    // Force cursor to targetX for perfect alignment with target line
    double cursorScreenX = targetX;

    bool drawHandle =
        (m_visualStyle == Standalone || m_visualStyle == UnifiedTop);
    bool drawLabel =
        (m_visualStyle == Standalone || m_visualStyle == UnifiedTop);

    int lineTop = bandY;
    int lineBottom = bandY + bandHeight;

    if (m_visualStyle == UnifiedTop)
      lineBottom += 2;
    if (m_visualStyle == UnifiedBottom)
      lineTop -= 2;

    // Vertical cursor line
    QPen cursorPen(QColor(0, 120, 215), 3);
    painter.setPen(cursorPen);
    painter.drawLine(cursorScreenX, lineTop, cursorScreenX, lineBottom);

    // Triangle handle
    if (drawHandle) {
      QPolygon tri;
      tri << QPoint(cursorScreenX, bandY)
          << QPoint(cursorScreenX - 5, bandY - 10)
          << QPoint(cursorScreenX + 5, bandY - 10);
      painter.setBrush(QColor(0, 120, 215));
      painter.drawPolygon(tri);
    }

    // Timestamp label
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
      painter.drawText(cursorScreenX - tw / 2, bandY - 12, timeStr);
    }
  }
}

// =============================================================================
// Mouse Events
// =============================================================================

void RythmoWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }

  m_lastMouseX = event->pos().x();

  int targetX = width() / 5;
  int clickX = event->pos().x();
  int deltaPixels = clickX - targetX;

  // Calculate new position
  double timeDeltaMs = (static_cast<double>(deltaPixels) * 1000.0) / m_speed;
  qint64 newTime =
      std::max(qint64(0), m_currentPosition + static_cast<qint64>(timeDeltaMs));

  requestDebouncedSeek(newTime);
  setFocus();
}

void RythmoWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton)) {
    return;
  }

  int currentX = event->pos().x();
  int deltaX = currentX - m_lastMouseX;
  m_lastMouseX = currentX;

  // Dragging: reverse direction for intuitive feel
  double timeDeltaMs = (static_cast<double>(deltaX) * 1000.0) / m_speed;
  qint64 newTime =
      std::max(qint64(0), m_currentPosition - static_cast<qint64>(timeDeltaMs));

  requestDebouncedSeek(newTime);
}

void RythmoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  mousePressEvent(event);
}

// =============================================================================
// Keyboard Events
// =============================================================================

void RythmoWidget::keyPressEvent(QKeyEvent *event) {
  qint64 step = charDurationMs();

  // Navigation
  if (event->key() == Qt::Key_Left) {
    qint64 newTime = std::max(qint64(0), m_currentPosition - step);
    requestDebouncedSeek(newTime);
    return;
  }
  if (event->key() == Qt::Key_Right) {
    qint64 newTime = m_currentPosition + step;
    requestDebouncedSeek(newTime);
    return;
  }

  // Escape: Insert space (push text) and play
  if (event->key() == Qt::Key_Escape) {
    int idx = cursorIndex();
    while (m_text.length() < idx) {
      m_text.append(' ');
    }
    m_text.insert(idx, ' ');
    qint64 newTime = m_currentPosition + step;
    requestDebouncedSeek(newTime);
    emit playRequested();
    return;
  }

  // Text Editing
  int idx = cursorIndex();

  if (event->key() == Qt::Key_Backspace) {
    // If we are BEYOND the text, just move back
    if (idx > m_text.length()) {
      qint64 newTime = std::max(qint64(0), m_currentPosition - step);
      requestDebouncedSeek(newTime);
    }
    // If we are AT or WITHIN text, delete character and move back
    else if (idx > 0 && idx <= m_text.length()) {
      m_text.remove(idx - 1, 1);
      qint64 newTime = std::max(qint64(0), m_currentPosition - step);
      requestDebouncedSeek(newTime);
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
    qint64 newTime = m_currentPosition + step;
    requestDebouncedSeek(newTime);
  }
}
