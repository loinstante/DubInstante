#ifndef RYTHMOOVERLAY_H
#define RYTHMOOVERLAY_H

#include "RythmoWidget.h"
#include <QVBoxLayout>
#include <QWidget>

class RythmoOverlay : public QWidget {
  Q_OBJECT

public:
  explicit RythmoOverlay(QWidget *parent = nullptr);

  RythmoWidget *track1() const;
  RythmoWidget *track2() const; // May return visible or hidden widget

  void setTrack2Visible(bool visible);
  bool isTrack2Visible() const;

  // Proxy methods for convenience (optional, but cleaner)
  void sync(qint64 positionMs);
  void setPlaying(bool playing);
  void setSpeed(int speed);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  RythmoWidget *m_rythmo1;
  RythmoWidget *m_rythmo2;
  QVBoxLayout *m_layout;
};

#endif // RYTHMOOVERLAY_H
