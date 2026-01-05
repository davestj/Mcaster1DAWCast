// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QList>
#include <cstdint>

class QPainter;

namespace dawcast { class Clip; }
namespace dawcast { class Timeline; }

namespace dawcast::widgets {

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget() override;

    void setTimeline(Timeline* timeline);
    void setZoom(float zoom);
    void setScroll(int64_t position);

    QList<int> selectedClips() const;

signals:
    void clipMoved(int clipId, int64_t newPosition);
    void clipSelected(int clipId);
    void playheadMoved(int64_t position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void drawRuler(QPainter& painter, int viewWidth);
    void drawClip(QPainter& painter, Clip* clip, int trackIndex, int clipIndex, int yTop);

    Timeline* m_timeline = nullptr;
    float           m_zoom     = 1.0f;
    int64_t         m_scroll   = 0;
    QList<int>      m_selectedClips;
    bool            m_dragging = false;
    QPoint          m_dragStart;
    QRect           m_rubberBand;
};

} // namespace dawcast::widgets
