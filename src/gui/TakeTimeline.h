/**
 * @file TakeTimeline.h
 * @brief Timeline under the video: every take of every track, and the comp.
 *
 * Per track, a comp lane shows what is heard; unfolded, one lane per take
 * (newest on top) lets the user pick the take heard in a comp region.
 * Edits are proposed through trackEdited(): the widget never owns the data.
 *
 * @note Part of the GUI layer.
 */

#ifndef TAKETIMELINE_H
#define TAKETIMELINE_H

#include "../core/TakeTrack.h"

#include <QVector>
#include <QWidget>

class TakeTimeline : public QWidget {
  Q_OBJECT

public:
  explicit TakeTimeline(QWidget *parent = nullptr);

  void setDuration(qint64 durationMs);
  void setPosition(qint64 positionMs);
  void setPlaying(bool playing);
  void setTrackCount(int count);
  void setTrack(int index, const TakeTrack &takes);
  /// Cuts the selected track's comp at the playhead.
  void splitSelectedAtPlayhead();

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

signals:
  void seekRequested(qint64 positionMs);
  void playRequested();
  void trackEdited(int index, const TakeTrack &takes);
  void contentHeightChanged(int height);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  struct Row {
    int track = 0;
    int takeId = 0;          ///< 0 = the comp lane of the track
    int y = 0;
    int height = 0;
  };

  QList<Row> rows() const;
  int rowAt(int y) const;
  int contentHeight() const;
  void relayout();

  int laneWidth() const;
  double msPerPx() const;
  qint64 visibleSpanMs() const;
  int xAt(qint64 timeMs) const;
  qint64 timeAt(int x) const;
  void clampView();
  void zoomAround(int x, double factor);
  QList<qint64> cuts(int track) const;
  /// Comp cut drawn within a few pixels of x, or -1.
  qint64 cutNear(int track, int x) const;
  int takeNumber(int track, int takeId) const;
  QColor trackColor(int track) const;
  void confirmRemoveTake(int track, int takeId);

  QVector<TakeTrack> m_tracks;
  QVector<bool> m_expanded;
  int m_selectedTrack = 0;
  int m_contentHeight = 0;
  qint64 m_durationMs = 0;
  qint64 m_positionMs = 0;
  bool m_playing = false;
  qint64 m_viewStartMs = 0;
  double m_zoomMsPerPx = 0.0;   ///< 0 = whole video fits the width
};

#endif // TAKETIMELINE_H
