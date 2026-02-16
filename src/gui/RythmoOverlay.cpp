/**
 * @file RythmoOverlay.cpp
 * @brief Implementation of the RythmoOverlay class.
 */

#include "RythmoOverlay.h"

#include <QPainter>

RythmoOverlay::RythmoOverlay(QWidget *parent)
    : QWidget(parent), m_rythmo1(new RythmoWidget(this)),
      m_rythmo2(new RythmoWidget(this)), m_layout(new QVBoxLayout(this)) {
  // Configure transparency
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);

  // Configure layout
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  // Position tracks at bottom of overlay
  m_layout->addStretch(1);
  m_layout->addWidget(m_rythmo1);
  m_layout->addWidget(m_rythmo2);

  // Configure visual styles for unified look
  m_rythmo1->setVisualStyle(RythmoWidget::UnifiedTop);
  m_rythmo2->setVisualStyle(RythmoWidget::UnifiedBottom);

  // Track 2 hidden by default
  m_rythmo2->setVisible(false);
}

// =============================================================================
// Track Access
// =============================================================================

RythmoWidget *RythmoOverlay::track1() const { return m_rythmo1; }

RythmoWidget *RythmoOverlay::track2() const { return m_rythmo2; }

void RythmoOverlay::setTrack2Visible(bool visible) {
  m_rythmo2->setVisible(visible);
}

bool RythmoOverlay::isTrack2Visible() const { return m_rythmo2->isVisible(); }

// =============================================================================
// Proxy Methods
// =============================================================================

void RythmoOverlay::sync(qint64 positionMs) {
  m_rythmo1->sync(positionMs);
  m_rythmo2->sync(positionMs);
}

void RythmoOverlay::setPlaying(bool playing) {
  m_rythmo1->setPlaying(playing);
  m_rythmo2->setPlaying(playing);
}

void RythmoOverlay::setSpeed(int speed) {
  m_rythmo1->setSpeed(speed);
  m_rythmo2->setSpeed(speed);
}

void RythmoOverlay::setTextColor(const QColor &color) {
  m_rythmo1->setTextColor(color);
  m_rythmo2->setTextColor(color);
}

void RythmoOverlay::setEditable(bool editable) {
  m_rythmo1->setEditable(editable);
  m_rythmo2->setEditable(editable);
}

// =============================================================================
// Paint Event
// =============================================================================

void RythmoOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  // Transparent background - nothing to paint
}
