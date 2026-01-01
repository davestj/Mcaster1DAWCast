// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimelineWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>

namespace dawcast::widgets {

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

TimelineWidget::~TimelineWidget() = default;

void TimelineWidget::setTimeline(core::Timeline* timeline)
{
    m_timeline = timeline;
    update();
}

void TimelineWidget::setZoom(float zoom)
{
    m_zoom = zoom;
    update();
}

void TimelineWidget::setScroll(int64_t position)
{
    m_scroll = position;
    update();
}

QList<int> TimelineWidget::selectedClips() const
{
    return m_selectedClips;
}

void TimelineWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());

    // TODO: draw tracks, clips (waveform/thumbnail), playhead, markers, selection
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    m_dragging = true;
    m_dragStart = event->pos();
    QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        // TODO: rubber-band select or clip drag
        m_rubberBand = QRect(m_dragStart, event->pos()).normalized();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    m_rubberBand = QRect();
    update();
    QWidget::mouseReleaseEvent(event);
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(tr("Cut"));
    menu.addAction(tr("Copy"));
    menu.addAction(tr("Paste"));
    menu.addAction(tr("Delete"));
    menu.exec(event->globalPos());
}

} // namespace dawcast::widgets
