// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimelineScrollBar.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEnterEvent>
#include <QLinearGradient>
#include <QtGlobal>

#include <algorithm>

namespace dawcast::widgets {

namespace {
constexpr int kTrackThickness = 18;   // total scrollbar widget thickness
constexpr int kThumbInset     = 2;    // visual padding inside the track
} // anonymous namespace

TimelineScrollBar::TimelineScrollBar(Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent)
    , m_orientation(orientation)
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    if (m_orientation == Qt::Horizontal) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(kTrackThickness);
    } else {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setFixedWidth(kTrackThickness);
    }
    setToolTip(tr("Drag the thumb body to pan. Drag either end of the "
                  "thumb to zoom in or out — like Logic / Reaper."));
}

QSize TimelineScrollBar::sizeHint() const
{
    if (m_orientation == Qt::Horizontal)
        return QSize(200, kTrackThickness);
    return QSize(kTrackThickness, 200);
}

QSize TimelineScrollBar::minimumSizeHint() const
{
    if (m_orientation == Qt::Horizontal)
        return QSize(60, kTrackThickness);
    return QSize(kTrackThickness, 60);
}

// ── Property setters ─────────────────────────────────────────────────────

void TimelineScrollBar::setContentSize(int contentPx)
{
    contentPx = qMax(1, contentPx);
    if (m_contentPx == contentPx) return;
    m_contentPx = contentPx;
    if (m_viewportPx > m_contentPx) m_viewportPx = m_contentPx;
    if (m_value + m_viewportPx > m_contentPx)
        m_value = qMax(0, m_contentPx - m_viewportPx);
    update();
}

void TimelineScrollBar::setViewportSize(int viewportPx)
{
    viewportPx = qBound(1, viewportPx, m_contentPx);
    if (m_viewportPx == viewportPx) return;
    m_viewportPx = viewportPx;
    if (m_value + m_viewportPx > m_contentPx)
        m_value = qMax(0, m_contentPx - m_viewportPx);
    update();
}

void TimelineScrollBar::setValue(int valuePx)
{
    valuePx = qBound(0, valuePx, qMax(0, m_contentPx - m_viewportPx));
    if (m_value == valuePx) return;
    m_value = valuePx;
    update();
}

// ── Geometry helpers ─────────────────────────────────────────────────────

int TimelineScrollBar::activeLength() const
{
    return (m_orientation == Qt::Horizontal) ? width() : height();
}

int TimelineScrollBar::axisCoord(const QPoint& p) const
{
    return (m_orientation == Qt::Horizontal) ? p.x() : p.y();
}

void TimelineScrollBar::computeThumbRect(int& start, int& length) const
{
    const int track = activeLength();
    if (m_contentPx <= 0) {
        start = 0;
        length = track;
        return;
    }
    const double scale = static_cast<double>(track) / static_cast<double>(m_contentPx);
    length = static_cast<int>(std::round(m_viewportPx * scale));
    if (length < m_minThumbPx) length = m_minThumbPx;
    if (length > track) length = track;
    start = static_cast<int>(std::round(m_value * scale));
    if (start + length > track) start = track - length;
    if (start < 0) start = 0;
}

TimelineScrollBar::Zone TimelineScrollBar::hitTest(const QPoint& pos) const
{
    int start = 0, length = 0;
    computeThumbRect(start, length);

    const int c = axisCoord(pos);
    const int gripL = start;
    const int gripR = start + length - 1;

    if (c < gripL) return ZoneTrackLeft;
    if (c > gripR) return ZoneTrackRight;

    // Grip zones are ALWAYS active — even on tiny thumbs. Each grip
    // takes the lesser of m_gripPx and 1/3 of the thumb length, with a
    // hard floor of 6 px so the user can always grab them.
    int gripWidth = qMax(6, qMin(m_gripPx, length / 3));
    if (length < 18) {
        // Very small thumb: split the whole thumb into left half / right
        // half so each side is still draggable as a grip.
        gripWidth = length / 2;
    }
    if (c <= gripL + gripWidth) return ZoneLeftGrip;
    if (c >= gripR - gripWidth) return ZoneRightGrip;
    return ZoneBody;
}

// ── Painting ─────────────────────────────────────────────────────────────

void TimelineScrollBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();

    // Track background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(232, 232, 238));
    p.drawRoundedRect(r, 4, 4);
    p.setPen(QPen(QColor(200, 200, 208), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 4, 4);

    // Thumb
    int start = 0, length = 0;
    computeThumbRect(start, length);

    QRect thumbRect;
    if (m_orientation == Qt::Horizontal) {
        thumbRect = QRect(start + kThumbInset,
                          kThumbInset,
                          length - 2 * kThumbInset,
                          r.height() - 2 * kThumbInset);
    } else {
        thumbRect = QRect(kThumbInset,
                          start + kThumbInset,
                          r.width() - 2 * kThumbInset,
                          length - 2 * kThumbInset);
    }

    QLinearGradient grad;
    if (m_orientation == Qt::Horizontal) {
        grad = QLinearGradient(thumbRect.topLeft(), thumbRect.bottomLeft());
    } else {
        grad = QLinearGradient(thumbRect.topLeft(), thumbRect.topRight());
    }
    QColor base = m_hover ? QColor(150, 165, 195) : QColor(170, 180, 200);
    if (m_dragZone != ZoneNone) base = QColor(110, 130, 175);
    grad.setColorAt(0.0, base.lighter(115));
    grad.setColorAt(0.5, base);
    grad.setColorAt(1.0, base.darker(115));

    p.setPen(QPen(QColor(70, 80, 100), 1));
    p.setBrush(grad);
    p.drawRoundedRect(thumbRect, 3, 3);

    // Grip handles at each end of the thumb — bold and obvious so the
    // user can SEE where the zoom-by-stretch hot zones are.
    {
        const QColor gripDark(20, 30, 50);
        const QColor gripLight(255, 255, 255, 200);
        p.setPen(Qt::NoPen);

        if (m_orientation == Qt::Horizontal && thumbRect.width() >= 12) {
            const int gh = thumbRect.height() - 4;
            const int gy = thumbRect.top() + 2;

            // Left grip block — solid bar with bright edge
            QRect leftGripR(thumbRect.left() + 1, gy, 5, gh);
            p.fillRect(leftGripR, gripDark);
            p.setPen(QPen(gripLight, 1));
            p.drawLine(leftGripR.right() + 1, gy + 1,
                       leftGripR.right() + 1, gy + gh - 1);
            p.setPen(Qt::NoPen);

            // Right grip block
            QRect rightGripR(thumbRect.right() - 5, gy, 5, gh);
            p.fillRect(rightGripR, gripDark);
            p.setPen(QPen(gripLight, 1));
            p.drawLine(rightGripR.left() - 1, gy + 1,
                       rightGripR.left() - 1, gy + gh - 1);

            // Tiny + / − marks centred on each grip when there's room
            if (thumbRect.width() >= 24) {
                p.setPen(QPen(QColor(255, 255, 255, 230), 1.5));
                int lcx = leftGripR.center().x();
                int rcx = rightGripR.center().x();
                int cy  = thumbRect.center().y();
                // left grip = expand outward (zoom out) → outward arrow
                p.drawLine(lcx - 2, cy, lcx + 2, cy);  // horizontal bar (−/+)
                // right grip
                p.drawLine(rcx - 2, cy, rcx + 2, cy);
            }
        } else if (m_orientation == Qt::Vertical && thumbRect.height() >= 12) {
            const int gw = thumbRect.width() - 4;
            const int gx = thumbRect.left() + 2;

            QRect topGripR(gx, thumbRect.top() + 1, gw, 5);
            p.fillRect(topGripR, gripDark);
            p.setPen(QPen(gripLight, 1));
            p.drawLine(gx + 1, topGripR.bottom() + 1,
                       gx + gw - 1, topGripR.bottom() + 1);
            p.setPen(Qt::NoPen);

            QRect botGripR(gx, thumbRect.bottom() - 5, gw, 5);
            p.fillRect(botGripR, gripDark);
            p.setPen(QPen(gripLight, 1));
            p.drawLine(gx + 1, botGripR.top() - 1,
                       gx + gw - 1, botGripR.top() - 1);

            if (thumbRect.height() >= 24) {
                p.setPen(QPen(QColor(255, 255, 255, 230), 1.5));
                int tcy = topGripR.center().y();
                int bcy = botGripR.center().y();
                int cx  = thumbRect.center().x();
                p.drawLine(cx, tcy - 2, cx, tcy + 2);
                p.drawLine(cx, bcy - 2, cx, bcy + 2);
            }
        }
    }
}

// ── Mouse handling ───────────────────────────────────────────────────────

void TimelineScrollBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    Zone zone = hitTest(event->pos());
    m_dragZone     = zone;
    m_dragStart    = axisCoord(event->pos());
    m_dragValue    = m_value;
    m_dragViewport = m_viewportPx;

    // Track click → page step
    if (zone == ZoneTrackLeft) {
        int newValue = qMax(0, m_value - m_viewportPx);
        if (newValue != m_value) {
            m_value = newValue;
            if (!m_quiet) emit valueChanged(m_value);
            update();
        }
        m_dragZone = ZoneNone;
    } else if (zone == ZoneTrackRight) {
        int maxV = qMax(0, m_contentPx - m_viewportPx);
        int newValue = qMin(maxV, m_value + m_viewportPx);
        if (newValue != m_value) {
            m_value = newValue;
            if (!m_quiet) emit valueChanged(m_value);
            update();
        }
        m_dragZone = ZoneNone;
    }

    update();
}

void TimelineScrollBar::mouseMoveEvent(QMouseEvent* event)
{
    // Update cursor based on hover zone (when not dragging)
    if (m_dragZone == ZoneNone) {
        Zone zone = hitTest(event->pos());
        Qt::CursorShape c = Qt::ArrowCursor;
        if (zone == ZoneBody) {
            c = (m_orientation == Qt::Horizontal)
                ? Qt::OpenHandCursor : Qt::OpenHandCursor;
        } else if (zone == ZoneLeftGrip || zone == ZoneRightGrip) {
            c = (m_orientation == Qt::Horizontal)
                ? Qt::SizeHorCursor : Qt::SizeVerCursor;
        }
        setCursor(c);
        return;
    }

    // Active drag
    const int delta = axisCoord(event->pos()) - m_dragStart;
    const int track = activeLength();
    if (track <= 0) return;
    const double pxPerContent = static_cast<double>(track) /
                                static_cast<double>(qMax(1, m_contentPx));
    const int contentDelta = static_cast<int>(std::round(delta / pxPerContent));

    switch (m_dragZone) {
    case ZoneBody: {
        int newValue = qBound(0,
                              m_dragValue + contentDelta,
                              qMax(0, m_contentPx - m_viewportPx));
        if (newValue != m_value) {
            m_value = newValue;
            if (!m_quiet) emit valueChanged(m_value);
            update();
        }
        break;
    }
    case ZoneLeftGrip: {
        // Drag left grip: move start, keep end fixed.
        int oldEnd     = m_dragValue + m_dragViewport;
        int newValue   = qBound(0, m_dragValue + contentDelta, oldEnd - 1);
        int newViewport = oldEnd - newValue;
        if (newViewport < 1) newViewport = 1;
        if (newViewport > m_contentPx) newViewport = m_contentPx;
        m_value      = newValue;
        m_viewportPx = newViewport;
        if (!m_quiet) emit zoomChanged(m_viewportPx, m_value, m_dragViewport);
        update();
        break;
    }
    case ZoneRightGrip: {
        // Drag right grip: keep start fixed, move end.
        int newViewport = qBound(1,
                                 m_dragViewport + contentDelta,
                                 m_contentPx - m_dragValue);
        m_viewportPx = newViewport;
        if (!m_quiet) emit zoomChanged(m_viewportPx, m_value, m_dragViewport);
        update();
        break;
    }
    default:
        break;
    }
}

void TimelineScrollBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragZone != ZoneNone) {
        m_dragZone = ZoneNone;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void TimelineScrollBar::wheelEvent(QWheelEvent* event)
{
    int steps = event->angleDelta().y() / 120;
    if (steps == 0) steps = event->angleDelta().x() / 120;
    if (steps == 0) return;

    int delta = -steps * qMax(1, m_viewportPx / 8);
    int maxV  = qMax(0, m_contentPx - m_viewportPx);
    int newValue = qBound(0, m_value + delta, maxV);
    if (newValue != m_value) {
        m_value = newValue;
        if (!m_quiet) emit valueChanged(m_value);
        update();
    }
    event->accept();
}

void TimelineScrollBar::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void TimelineScrollBar::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(event);
}

} // namespace dawcast::widgets
