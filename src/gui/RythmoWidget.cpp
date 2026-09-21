/**
 * @file RythmoWidget.cpp
 * @brief Implementation of the RythmoWidget class.
 */

#include "RythmoWidget.h"
#include "../core/SettingsManager.h"
#include "Palette.h"

#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

RythmoWidget::RythmoWidget(QWidget *parent)
    : QWidget(parent), m_currentPosition(0), m_speed(100), m_charMs(0.0),
      m_isPlaying(false), m_editable(true),
      m_barColor(QColor(0, 0, 0, 0)), m_playingBarColor(QColor(0, 0, 0, 0)),
      m_lastMouseX(0), m_cachedCharWidth(-1), m_seekTimer(new QTimer(this)),
      m_pendingSeekPosition(0), m_animationTimer(new QTimer(this)),
      m_lastSyncPosition(0), m_lastSyncTime(0) {
  m_charMs = nominalCharMs();
  m_syncClock.start();
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

void RythmoWidget::setTrackStyle(const RythmoTrackStyle &style) {
  m_style = style;
  m_cachedCharWidth = -1;
  // An empty band has no sync to preserve: it follows the nominal speed.
  // Otherwise the time grid stays put and the band is only rescaled on screen.
  if (m_text.trimmed().isEmpty()) {
    m_charMs = nominalCharMs();
  }
  update();
}

RythmoTrackStyle RythmoWidget::trackStyle() const { return m_style; }

void RythmoWidget::setSpeed(int speed) {
  if (m_speed != speed && speed > 0) {
    // Relative, not recomputed from the font: the grid may come from a project
    // file or predate a font change.
    m_charMs *= static_cast<double>(m_speed) / speed;
    m_speed = speed;
    emit speedChanged(m_speed);
    update();
  }
}

int RythmoWidget::speed() const { return m_speed; }

double RythmoWidget::charMs() const { return m_charMs; }

void RythmoWidget::setCharMs(double ms) {
  m_charMs = ms > 0.0 ? ms : nominalCharMs();
  update();
}

void RythmoWidget::setEditable(bool editable) { m_editable = editable; }

bool RythmoWidget::isEditable() const { return m_editable; }

void RythmoWidget::setText(const QString &text) {
  if (m_text != text) {
    m_text = text;
    emit textChanged(m_text);
    update();
  }
}

QString RythmoWidget::text() const { return m_text; }

// =============================================================================
// Data Input Slots
// =============================================================================

void RythmoWidget::updateDisplay(qint64 positionMs, const QString &text,
                                 int speed) {
  m_currentPosition = positionMs;
  m_text = text;
  setSpeed(speed);
  update();
}

void RythmoWidget::setPlaying(bool playing) {
  if (m_isPlaying != playing) {
    m_isPlaying = playing;

    if (m_isPlaying) {
      // Start animation loop
      m_lastSyncPosition = m_currentPosition;
      m_lastSyncTime = m_syncClock.elapsed();
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
  m_lastSyncTime = m_syncClock.elapsed();

  // If not animating (paused), update strictly to valid position
  if (!m_isPlaying) {
    if (m_currentPosition != positionMs) {
      m_currentPosition = positionMs;
      update();
    }
  }
}

void RythmoWidget::animate() {
  if (!m_isPlaying)
    return;

  qint64 now = m_syncClock.elapsed();
  qint64 elapsed = now - m_lastSyncTime;

  // Extrapolate position based on time elapsed since last sync
  m_currentPosition = m_lastSyncPosition + elapsed;

  update();
}

// =============================================================================
// Helpers
// =============================================================================

int RythmoWidget::charWidth() const {
  if (m_cachedCharWidth == -1) {
    QFontMetrics fm(m_style.font);
    m_cachedCharWidth = fm.horizontalAdvance('A');
  }
  return m_cachedCharWidth;
}

double RythmoWidget::nominalCharMs() const {
  const int cw = charWidth();
  return cw > 0 ? cw * 1000.0 / m_speed : 40.0; // Fallback ~1 frame
}

double RythmoWidget::pixelsPerMs() const { return charWidth() / m_charMs; }

int RythmoWidget::cursorIndex() const {
  return qRound(m_currentPosition / m_charMs); // Nearest char for intuitive snap
}

// Steps go through the index: adding a rounded duration N times drifts off the
// grid, and the typed characters end up one cell away from the cursor.
qint64 RythmoWidget::timeAtIndex(int index) const {
  return qRound64(std::max(0, index) * m_charMs);
}

void RythmoWidget::requestDebouncedSeek(qint64 positionMs) {
  m_pendingSeekPosition = positionMs;
  m_currentPosition = positionMs;
  update();
  m_seekTimer->start(200); // 200ms debounce
}

void RythmoWidget::triggerSeek() { emit seekRequested(m_pendingSeekPosition); }

// =============================================================================
// Size Hint
// =============================================================================

QSize RythmoWidget::sizeHint() const {
  // 35 base band height + 25 header space for handle/timestamp
  return QSize(QWidget::sizeHint().width(), 60);
}

// =============================================================================
// Paint Event
// =============================================================================

bool RythmoWidget::isDarkTheme() {
  if (m_isDark < 0) {
    const QString themeMode = SettingsManager::instance().theme();
    if (themeMode == "dark") {
      m_isDark = 1;
    } else if (themeMode == "system") {
      m_isDark = palette().color(QPalette::Window).value() < 128;
    } else {
      m_isDark = 0;
    }
  }
  return m_isDark;
}

// applyTheme() swaps the stylesheet (StyleChange) and the app palette (PaletteChange).
void RythmoWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::StyleChange) {
    m_isDark = -1;
    update();
  }
  QWidget::changeEvent(event);
}

void RythmoWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 1. Calculate layout dimensions
  const int headerHeight = 25;

  int bandHeight = height() - headerHeight;
  int bandY = headerHeight;
  QRect bandRect(0, bandY, width(), bandHeight);

  // 2. Calculate drawing parameters
  int cw = charWidth();
  int targetX = width() / 5; // Target line position

  double pixelOffset;
  if (m_isPlaying && cw > 0) {
    // Smooth scrolling using continuous position
    pixelOffset = m_currentPosition * pixelsPerMs();
  } else {
    // Snap to character grid for precise editing alignment when paused
    pixelOffset = static_cast<double>(cursorIndex() * cw);
  }

  double textStartX = targetX - pixelOffset;

  // 3. Draw band background
  QColor bgColor = m_style.backgroundColor;
  // If playing, we might want to slightly lighten or darken it as a visual cue
  if (m_isPlaying) {
    bgColor = bgColor.lighter(110);
  }
  painter.fillRect(bandRect, bgColor);

  // 4. Draw scrolling text (virtualized for performance)
  if (cw > 0 && !m_text.isEmpty()) {
    painter.setFont(m_style.font);
    painter.setPen(m_style.textColor);

    int textY = bandY + (bandHeight + m_style.globalSize) / 2 - 2;

    // Only render visible characters
    int firstVisibleIdx = std::max(0, static_cast<int>(-textStartX / cw));
    int lastVisibleIdx =
        std::min(static_cast<int>(m_text.length()),
                 static_cast<int>((width() - textStartX) / cw) + 1);

    // One cell per character: drawn as a single string, the text follows the
    // fractional advances of the font (12.59 px for a 13 px cell) and slides
    // away from the time grid, by ~120 ms under the target line.
    for (int i = firstVisibleIdx; i < lastVisibleIdx; ++i) {
      const bool isPair = m_text[i].isHighSurrogate() &&
                          i + 1 < m_text.length() &&
                          m_text[i + 1].isLowSurrogate();
      painter.drawText(QPointF(textStartX + i * cw, textY),
                       m_text.mid(i, isPair ? 2 : 1));
      if (isPair)
        ++i;
    }
  }

  const bool isDark = isDarkTheme();
  const QColor accent = isDark ? QColor(Brand::AccentLight) : QColor(Brand::Accent);

  // 5. Draw band border
  QPen borderPen(accent, 2);
  painter.setPen(borderPen);
  painter.drawRect(bandRect);

  // 6. Draw target line (guide)
  QPen targetPen(accent, 2);
  targetPen.setStyle(Qt::DashLine);
  painter.setPen(targetPen);
  painter.drawLine(targetX, bandY, targetX, bandY + bandHeight);

  // 7. Draw edit cursor (always at targetX to align with playback line)
  if (cw > 0) {
    // Force cursor to targetX for perfect alignment with target line
    double cursorScreenX = targetX;

    int lineTop = bandY;
    int lineBottom = bandY + bandHeight;

    // Vertical cursor line
    QPen cursorPen(accent, 3);
    painter.setPen(cursorPen);
    painter.drawLine(cursorScreenX, lineTop, cursorScreenX, lineBottom);

    // Triangle handle
    QPolygon tri;
    tri << QPoint(cursorScreenX, bandY)
        << QPoint(cursorScreenX - 5, bandY - 10)
        << QPoint(cursorScreenX + 5, bandY - 10);
    painter.setBrush(accent);
    painter.drawPolygon(tri);

    // Timestamp label
    int mm = (m_currentPosition / 60000) % 60;
    int ss = (m_currentPosition / 1000) % 60;
    int ms = m_currentPosition % 1000;
    QString timeStr = QString("%1:%2.%3")
                          .arg(mm, 2, 10, QChar('0'))
                          .arg(ss, 2, 10, QChar('0'))
                          .arg(ms, 3, 10, QChar('0'));

    QFont smallFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    smallFont.setPointSize(8);
    smallFont.setBold(true);
    painter.setFont(smallFont);
    int tw = painter.fontMetrics().horizontalAdvance(timeStr);
    int th = painter.fontMetrics().height();

    // Draw a subtle translucent background pill for the timestamp (always readable over video)
    QRectF pillRect(cursorScreenX - tw / 2.0 - 6, bandY - 12 - th + 2, tw + 12, th + 4);
    painter.setBrush(QColor(13, 13, 18, 160)); // 62% opacity dark background
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(pillRect, 4, 4);

    // Draw text
    painter.setPen(isDark ? QColor(243, 243, 246) : QColor(255, 255, 255));
    painter.drawText(cursorScreenX - tw / 2, bandY - 12, timeStr);
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

  if (charWidth() <= 0) {
    return;
  }

  int targetX = width() / 5;
  int clickX = event->pos().x();
  int deltaPixels = clickX - targetX;

  // Calculate new position
  double timeDeltaMs = deltaPixels / pixelsPerMs();
  qint64 newTime =
      std::max(qint64(0), m_currentPosition + static_cast<qint64>(timeDeltaMs));

  requestDebouncedSeek(newTime);
  setFocus();
}

void RythmoWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton)) {
    return;
  }

  if (charWidth() <= 0) {
    return;
  }

  int currentX = event->pos().x();
  int deltaX = currentX - m_lastMouseX;
  m_lastMouseX = currentX;

  // Dragging: reverse direction for intuitive feel
  double timeDeltaMs = deltaX / pixelsPerMs();
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
  const int idx = cursorIndex();

  // Navigation
  if (event->key() == Qt::Key_Left) {
    requestDebouncedSeek(timeAtIndex(idx - 1));
    return;
  }
  if (event->key() == Qt::Key_Right) {
    requestDebouncedSeek(timeAtIndex(idx + 1));
    return;
  }

  // Escape: Insert space (push text) and play
  if (event->key() == Qt::Key_Escape) {
    if (!m_editable)
      return;
    while (m_text.length() < idx) {
      m_text.append(' ');
    }
    m_text.insert(idx, ' ');
    requestDebouncedSeek(timeAtIndex(idx + 1));
    emit textChanged(m_text);
    emit playRequested();
    return;
  }

  // Text Editing
  if (event->key() == Qt::Key_Backspace) {
    if (!m_editable)
      return;
    // If we are BEYOND the text, just move back
    if (idx > m_text.length()) {
      requestDebouncedSeek(timeAtIndex(idx - 1));
    }
    // If we are AT or WITHIN text, delete character and move back
    else if (idx > 0 && idx <= m_text.length()) {
      m_text.remove(idx - 1, 1);
      requestDebouncedSeek(timeAtIndex(idx - 1));
      emit textChanged(m_text);
    }
    return;
  }

  if (event->key() == Qt::Key_Delete) {
    if (!m_editable)
      return;
    if (idx >= 0 && idx < m_text.length()) {
      m_text.remove(idx, 1);
      emit textChanged(m_text);
      update();
    }
    return;
  }

  // Printable Characters
  if (!m_editable)
    return;
  if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
    // Pad with spaces if needed
    while (m_text.length() < idx) {
      m_text.append(' ');
    }
    m_text.insert(idx, event->text());
    requestDebouncedSeek(timeAtIndex(idx + event->text().length()));
    emit textChanged(m_text);
  }
}
