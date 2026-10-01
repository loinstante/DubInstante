#include "TakeTimeline.h"

#include "TimeFormatter.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>
#include <iterator>

namespace {
constexpr int kRulerHeight = 22;
constexpr int kHeaderWidth = 112;
constexpr int kCompHeight = 34;
constexpr int kTakeHeight = 22;
constexpr int kTrackGap = 6;
constexpr int kCutGrabPx = 5;
constexpr double kMinMsPerPx = 1.0;
constexpr qint64 kMinSpanMs = 60000; // no video yet: a readable minute, not 0 s
} // namespace

TakeTimeline::TakeTimeline(QWidget *parent) : QWidget(parent) {
  // Space, arrows and letters keep driving the transport and the rythmo band
  setFocusPolicy(Qt::NoFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  relayout();
}

void TakeTimeline::setDuration(qint64 durationMs) {
  m_durationMs = qMax<qint64>(0, durationMs);
  clampView();
  update();
}

void TakeTimeline::setPosition(qint64 positionMs) {
  m_positionMs = positionMs;
  // Zoomed in while playing: page the view so the playhead stays on screen
  if (m_playing && m_zoomMsPerPx > 0.0) {
    const int x = xAt(positionMs);
    if (x < kHeaderWidth || x > width() - 20) {
      m_viewStartMs = positionMs - visibleSpanMs() / 10;
      clampView();
    }
  }
  update();
}

void TakeTimeline::setPlaying(bool playing) { m_playing = playing; }

void TakeTimeline::setTrackCount(int count) {
  m_tracks.resize(count);
  m_expanded.resize(count);
  m_selectedTrack = qBound(0, m_selectedTrack, qMax(0, count - 1));
  relayout();
}

void TakeTimeline::setTrack(int index, const TakeTrack &takes) {
  if (index < 0 || index >= m_tracks.size())
    return;
  m_tracks[index] = takes;
  relayout();
}

void TakeTimeline::splitSelectedAtPlayhead() {
  if (m_selectedTrack >= m_tracks.size())
    return;
  TakeTrack edited = m_tracks[m_selectedTrack];
  edited.split(m_positionMs);
  emit trackEdited(m_selectedTrack, edited);
}

QSize TakeTimeline::sizeHint() const { return QSize(600, contentHeight()); }

QSize TakeTimeline::minimumSizeHint() const {
  return QSize(kHeaderWidth + 100, kRulerHeight + kCompHeight + kTrackGap);
}

// =============================================================================
// Layout and time mapping
// =============================================================================

QList<TakeTimeline::Row> TakeTimeline::rows() const {
  QList<Row> out;
  int y = kRulerHeight;
  for (int track = 0; track < m_tracks.size(); ++track) {
    out.append(Row{track, 0, y, kCompHeight});
    y += kCompHeight;
    if (m_expanded.value(track)) {
      const QList<Take> &takes = m_tracks[track].takes();
      for (int k = takes.size() - 1; k >= 0; --k) {
        out.append(Row{track, takes[k].id, y, kTakeHeight});
        y += kTakeHeight;
      }
    }
    y += kTrackGap;
  }
  return out;
}

int TakeTimeline::rowAt(int y) const {
  const QList<Row> all = rows();
  for (int i = 0; i < all.size(); ++i)
    if (y >= all[i].y && y < all[i].y + all[i].height)
      return i;
  return -1;
}

int TakeTimeline::contentHeight() const {
  const QList<Row> all = rows();
  return all.isEmpty() ? kRulerHeight : all.last().y + all.last().height + kTrackGap;
}

void TakeTimeline::relayout() {
  // Fixed height inside the scroll area: unfolded tracks scroll vertically
  setFixedHeight(contentHeight());
  update();
}

int TakeTimeline::laneWidth() const { return qMax(1, width() - kHeaderWidth); }

double TakeTimeline::msPerPx() const {
  if (m_zoomMsPerPx > 0.0)
    return m_zoomMsPerPx;
  return qMax(m_durationMs, kMinSpanMs) / double(laneWidth());
}

qint64 TakeTimeline::visibleSpanMs() const { return qint64(laneWidth() * msPerPx()); }

int TakeTimeline::xAt(qint64 timeMs) const {
  return kHeaderWidth + int(std::lround((timeMs - m_viewStartMs) / msPerPx()));
}

qint64 TakeTimeline::timeAt(int x) const {
  const qint64 t = m_viewStartMs + qint64((x - kHeaderWidth) * msPerPx());
  return qBound<qint64>(0, t, qMax<qint64>(m_durationMs, 0));
}

void TakeTimeline::clampView() {
  if (m_zoomMsPerPx <= 0.0) {
    m_viewStartMs = 0;
    return;
  }
  const qint64 maxStart = qMax<qint64>(0, m_durationMs - visibleSpanMs());
  m_viewStartMs = qBound<qint64>(0, m_viewStartMs, maxStart);
}

void TakeTimeline::zoomAround(int x, double factor) {
  const double fit = qMax(m_durationMs, kMinSpanMs) / double(laneWidth());
  const qint64 anchor = m_viewStartMs + qint64((x - kHeaderWidth) * msPerPx());
  const double next = qBound(kMinMsPerPx, msPerPx() * factor, fit);
  m_zoomMsPerPx = next >= fit ? 0.0 : next;
  m_viewStartMs = anchor - qint64((x - kHeaderWidth) * msPerPx());
  clampView();
  update();
}

// Edges of silence are region bounds too, but only a cut between two takes is one
QList<qint64> TakeTimeline::cuts(int track) const {
  QList<qint64> out;
  const QList<CompRegion> &regions = m_tracks[track].regions();
  for (int i = 1; i < regions.size(); ++i)
    if (regions[i - 1].takeId != 0 && regions[i].takeId != 0)
      out.append(regions[i].startMs);
  return out;
}

qint64 TakeTimeline::cutNear(int track, int x) const {
  for (qint64 cut : cuts(track))
    if (qAbs(xAt(cut) - x) <= kCutGrabPx)
      return cut;
  return -1;
}

int TakeTimeline::takeNumber(int track, int takeId) const {
  const QList<Take> &takes = m_tracks[track].takes();
  for (int k = 0; k < takes.size(); ++k)
    if (takes[k].id == takeId)
      return k + 1;
  return 0;
}

QColor TakeTimeline::trackColor(int track) const {
  static const QColor colors[] = {QColor("#5f8fbf"), QColor("#e8a33d"),
                                  QColor("#2fb49c"), QColor("#e0567a")};
  return colors[track % 4];
}

// =============================================================================
// Painting
// =============================================================================

// Translated labels run longer than the English the header was sized for
static void drawFitted(QPainter &p, const QRect &rect, const QString &text) {
  p.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
             p.fontMetrics().elidedText(text, Qt::ElideRight, rect.width()));
}

// White or near-black, whichever reads better on the block as painted (alpha
// over the lane). 0.2 is the luminance where both contrast equally.
static QColor labelColor(const QColor &block, const QColor &lane) {
  auto channel = [&](qreal fg, qreal bg) {
    const qreal v = fg * block.alphaF() + bg * (1 - block.alphaF());
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
  };
  const qreal luminance = 0.2126 * channel(block.redF(), lane.redF()) +
                          0.7152 * channel(block.greenF(), lane.greenF()) +
                          0.0722 * channel(block.blueF(), lane.blueF());
  return luminance > 0.2 ? QColor("#1d1d1d") : QColor(Qt::white);
}

void TakeTimeline::paintEvent(QPaintEvent *) {
  QPainter p(this);
  // A painter takes the application direction, not the widget's: Arabic would
  // right-align every label against the time axis
  p.setLayoutDirection(layoutDirection());
  const QPalette pal = palette();
  const QColor base = pal.color(QPalette::Base);
  const QColor headerBg = pal.color(QPalette::AlternateBase);
  const QColor line = pal.color(QPalette::Mid);
  const QColor text = pal.color(QPalette::Text);
  // Not PlaceholderText: a QSS `color` makes Qt derive it as 60% of Text (4.3:1 on the light theme)
  QColor muted = text;
  muted.setAlpha(180);
  const QRect lanes(kHeaderWidth, 0, laneWidth(), height());

  p.fillRect(rect(), base);
  p.fillRect(QRect(0, 0, width(), kRulerHeight), headerBg);

  // Ruler: the smallest step leaving room for a label
  static const qint64 steps[] = {100,   250,   500,    1000,   2000,   5000,  10000,
                                 15000, 30000, 60000, 120000, 300000, 600000, 1800000};
  qint64 step = steps[std::size(steps) - 1];
  for (qint64 s : steps) {
    if (s / msPerPx() >= 80) {
      step = s;
      break;
    }
  }
  QFont small = font();
  small.setPointSizeF(small.pointSizeF() * 0.85);
  p.save();
  p.setFont(small);
  p.setClipRect(lanes);
  const qint64 viewEnd = m_viewStartMs + visibleSpanMs();
  for (qint64 t = (m_viewStartMs / step) * step; t <= viewEnd; t += step) {
    const int x = xAt(t);
    QColor grid = line;
    grid.setAlpha(60);
    p.setPen(grid);
    p.drawLine(x, kRulerHeight, x, height());
    p.setPen(muted);
    p.drawLine(x, kRulerHeight - 6, x, kRulerHeight);
    p.drawText(x + 4, 0, 120, kRulerHeight - 4, Qt::AlignLeft | Qt::AlignBottom,
               step < 1000 ? TimeFormatter::formatWithMillis(t) : TimeFormatter::format(t));
  }
  p.restore();

  p.setRenderHint(QPainter::Antialiasing);
  for (const Row &row : rows()) {
    const TakeTrack &track = m_tracks[row.track];
    const QColor color = trackColor(row.track);
    const QRect header(0, row.y, kHeaderWidth, row.height);
    const QRect lane(kHeaderWidth, row.y + 2, laneWidth(), row.height - 4);
    const QList<Segment> segments = track.segments();

    if (row.takeId == 0) {
      p.fillRect(header, headerBg);
      if (row.track == m_selectedTrack)
        p.fillRect(QRect(0, row.y, 3, row.height), color);
      p.setPen(text);
      drawFitted(p, header.adjusted(10, 3, -6, -row.height / 2),
                 QString("%1  %2").arg(m_expanded.value(row.track) ? "▾" : "▸",
                                       tr("Track %1").arg(row.track + 1)));
      p.setPen(muted);
      p.setFont(small);
      drawFitted(p, header.adjusted(24, row.height / 2, -6, -3),
                 //: A take is one recording pass of a track (audio, not a film shot)
                 tr("%n take(s)", nullptr, int(track.takes().size())));
      p.setFont(font());

      p.save();
      p.setClipRect(lanes);
      QColor laneBg = line;
      laneBg.setAlpha(35);
      p.fillRect(lane, laneBg);
      if (track.isEmpty()) {
        p.setPen(muted);
        drawFitted(p, lane.adjusted(10, 0, -10, 0),
                   tr("No take: arm the track and start recording"));
      }
      for (const Segment &segment : segments) {
        const QRect block(QPoint(xAt(segment.timelineStartMs), lane.top()),
                          QPoint(xAt(segment.timelineEndMs()) - 1, lane.bottom()));
        QColor fill = color;
        fill.setAlpha(200);
        QPainterPath path;
        path.addRoundedRect(block, 3, 3);
        p.fillPath(path, fill);
        if (block.width() > 28) {
          p.setPen(labelColor(fill, base));
          p.drawText(block.adjusted(6, 0, -2, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     //: Very short "Take %1", drawn inside small timeline blocks
                     tr("T%1").arg(takeNumber(row.track, segment.takeId)));
        }
      }
      // Comp cuts
      p.setPen(QPen(text, 2));
      for (qint64 cut : cuts(row.track)) {
        const int x = xAt(cut);
        p.drawLine(x, lane.top() - 1, x, lane.bottom() + 1);
      }
      p.restore();
    } else {
      const Take *take = track.take(row.takeId);
      if (!take)
        continue;
      p.setPen(muted);
      drawFitted(p, header.adjusted(24, 0, -6, 0),
                 tr("Take %1").arg(takeNumber(row.track, row.takeId)));

      p.save();
      p.setClipRect(lanes);
      const QRect full(QPoint(xAt(take->audibleStartMs()), lane.top()),
                       QPoint(xAt(take->audibleEndMs()) - 1, lane.bottom()));
      QColor dim = color;
      dim.setAlpha(55);
      QPainterPath path;
      path.addRoundedRect(full, 3, 3);
      p.fillPath(path, dim);
      QColor heard = color;
      heard.setAlpha(190);
      for (const Segment &segment : segments) {
        if (segment.takeId == take->id)
          p.fillRect(QRect(QPoint(xAt(segment.timelineStartMs), lane.top()),
                           QPoint(xAt(segment.timelineEndMs()) - 1, lane.bottom())),
                     heard);
      }
      p.restore();
    }
  }

  // Playhead
  const int x = xAt(m_positionMs);
  if (x >= kHeaderWidth && x <= width()) {
    // Neutral, with a halo: any track colour (or an accent) can sit under it
    QColor halo = base;
    halo.setAlpha(160);
    p.setPen(QPen(halo, 4));
    p.drawLine(x, 0, x, height());
    p.setPen(QPen(text, 2));
    p.drawLine(x, 0, x, height());
    QPainterPath marker;
    marker.moveTo(x - 5, 0);
    marker.lineTo(x + 5, 0);
    marker.lineTo(x, 7);
    marker.closeSubpath();
    p.fillPath(marker, text);
  }
}

// =============================================================================
// Interaction
// =============================================================================

void TakeTimeline::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QPoint pos = event->position().toPoint();
  if (pos.y() < kRulerHeight) {
    if (pos.x() >= kHeaderWidth)
      emit seekRequested(timeAt(pos.x()));
    return;
  }
  const int index = rowAt(pos.y());
  if (index < 0)
    return;
  const Row row = rows()[index];
  m_selectedTrack = row.track;

  if (pos.x() < kHeaderWidth) {
    if (row.takeId == 0) {
      m_expanded[row.track] = !m_expanded[row.track];
      relayout();
    }
    update();
    return;
  }

  const qint64 t = timeAt(pos.x());
  const Take *take = m_tracks[row.track].take(row.takeId);
  if (take && t >= take->audibleStartMs() && t < take->audibleEndMs()) {
    // Comping by click: this take is now heard in the comp region under t
    TakeTrack edited = m_tracks[row.track];
    edited.choose(t, row.takeId);
    emit trackEdited(row.track, edited);
  } else {
    emit seekRequested(t);
  }
  update();
}

void TakeTimeline::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->position().y() < kRulerHeight) {
    m_zoomMsPerPx = 0.0;
    clampView();
    update();
  }
}

void TakeTimeline::wheelEvent(QWheelEvent *event) {
  const int delta = event->angleDelta().y() != 0 ? event->angleDelta().y()
                                                  : event->angleDelta().x();
  if (event->modifiers() & Qt::ControlModifier) {
    zoomAround(int(event->position().x()), std::pow(1.25, -delta / 120.0));
    event->accept();
  } else if ((event->modifiers() & Qt::ShiftModifier) || event->angleDelta().x() != 0) {
    m_viewStartMs -= qint64(delta / 120.0 * visibleSpanMs() / 10);
    clampView();
    update();
    event->accept();
  } else {
    event->ignore(); // plain wheel scrolls the unfolded takes vertically
  }
}

void TakeTimeline::contextMenuEvent(QContextMenuEvent *event) {
  const int index = rowAt(event->pos().y());
  if (index < 0 || event->pos().x() < kHeaderWidth)
    return;
  const Row row = rows()[index];
  const qint64 t = timeAt(event->pos().x());
  const TakeTrack &track = m_tracks[row.track];
  m_selectedTrack = row.track;
  update();

  QMenu menu(this);
  if (row.takeId == 0) {
    const qint64 cut = cutNear(row.track, event->pos().x());
    if (cut > 0) {
      menu.addAction(tr("Remove the cut"), this, [this, row, cut]() {
        TakeTrack edited = m_tracks[row.track];
        edited.removeCut(cut);
        emit trackEdited(row.track, edited);
      });
    }
    menu.addAction(tr("Cut here"), this, [this, row, t]() {
      TakeTrack edited = m_tracks[row.track];
      edited.split(t);
      emit trackEdited(row.track, edited);
    });
    const int heard = track.regions()[track.regionIndexAt(t)].takeId;
    if (heard != 0 && track.take(heard)) {
      menu.addSeparator();
      menu.addAction(tr("Delete take %1…").arg(takeNumber(row.track, heard)), this,
                     [this, row, heard]() { confirmRemoveTake(row.track, heard); });
    }
  } else if (const Take *take = track.take(row.takeId)) {
    const qint64 start = take->audibleStartMs();
    if (t >= start && t < take->audibleEndMs()) {
      menu.addAction(tr("Use this take here"), this, [this, row, t]() {
        TakeTrack edited = m_tracks[row.track];
        edited.choose(t, row.takeId);
        emit trackEdited(row.track, edited);
      });
    }
    menu.addAction(tr("Listen from the start of the take"), this, [this, start]() {
      emit seekRequested(start);
      emit playRequested();
    });
    menu.addSeparator();
    menu.addAction(tr("Delete take %1…").arg(takeNumber(row.track, row.takeId)),
                   this, [this, row]() { confirmRemoveTake(row.track, row.takeId); });
  }
  menu.exec(event->globalPos());
}

void TakeTimeline::confirmRemoveTake(int track, int takeId) {
  const auto reply = QMessageBox::question(
      this, tr("Delete the take"),
      tr("Delete take %1 from track %2?\n"
         "The regions where it was heard go back to the previous take.")
          .arg(takeNumber(track, takeId))
          .arg(track + 1),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;
  TakeTrack edited = m_tracks[track];
  edited.removeTake(takeId);
  emit trackEdited(track, edited);
}

void TakeTimeline::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  clampView();
}
