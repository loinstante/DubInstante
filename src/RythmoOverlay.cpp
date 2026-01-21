#include "RythmoOverlay.h"
#include <QEvent>
#include <QPainter>

RythmoOverlay::RythmoOverlay(QWidget *parent) : QWidget(parent) {
  // Essential for overlay over video
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_TransparentForMouseEvents,
               false); // We need mouse events for RythmoWidgets

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0); // 0px gap ("Unified Slider")
  m_layout->addStretch(1); // Push everything to bottom

  // Create Tracks
  // Note: In a VBox, widgets are added Top-to-Bottom.
  // We want Track 1 ABOVE Track 2.
  // So we add m_rythmo1 then m_rythmo2.

  m_rythmo1 = new RythmoWidget(this);
  // Ensure they have StrongFocus for keyboard editing
  m_rythmo1->setFocusPolicy(Qt::StrongFocus);
  m_layout->addWidget(m_rythmo1);

  m_rythmo2 = new RythmoWidget(this);
  m_rythmo2->setFocusPolicy(Qt::StrongFocus);
  m_layout->addWidget(m_rythmo2);

  // Initial State: Track 2 Hidden
  m_rythmo2->setVisible(false);
}

RythmoWidget *RythmoOverlay::track1() const { return m_rythmo1; }

RythmoWidget *RythmoOverlay::track2() const { return m_rythmo2; }

void RythmoOverlay::setTrack2Visible(bool visible) {
  m_rythmo2->setVisible(visible);
  if (visible) {
    // Unified Mode
    m_rythmo1->setVisualStyle(RythmoWidget::UnifiedTop);
    // Track 2 handles the bottom part
    m_rythmo2->setVisualStyle(RythmoWidget::UnifiedBottom);
  } else {
    // Single Mode
    m_rythmo1->setVisualStyle(RythmoWidget::Standalone);
    m_rythmo2->setVisualStyle(RythmoWidget::Standalone); // Reset
  }
}

bool RythmoOverlay::isTrack2Visible() const { return m_rythmo2->isVisible(); }

void RythmoOverlay::sync(qint64 positionMs) {
  m_rythmo1->sync(positionMs);
  if (m_rythmo2->isVisible()) {
    m_rythmo2->sync(positionMs);
  }
}

void RythmoOverlay::setPlaying(bool playing) {
  m_rythmo1->setPlaying(playing);
  if (m_rythmo2->isVisible()) {
    m_rythmo2->setPlaying(playing);
  }
}

void RythmoOverlay::setSpeed(int speed) {
  m_rythmo1->setSpeed(speed);
  m_rythmo2->setSpeed(speed);
}

void RythmoOverlay::paintEvent(QPaintEvent *event) {
  // We don't paint anything ourselves, just the transparent background
  QWidget::paintEvent(event);
}
