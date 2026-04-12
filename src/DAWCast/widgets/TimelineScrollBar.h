// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// TimelineScrollBar
// ─────────────────
// Drop-in scrollbar widget for the multi-track editor that adds
// zoom-by-stretch behaviour. The thumb represents the visible viewport
// over the total content, and has draggable grips at both ends:
//
//   ┌──────────────[ ●─────body─────● ]──────────┐
//                  ↑               ↑
//             left grip       right grip
//
// Mouse interactions:
//   - Drag thumb body  → pan the view (emit valueChanged)
//   - Drag left grip   → expand/contract from the left  → zoom
//   - Drag right grip  → expand/contract from the right → zoom
//   - Click on track   → page-step in that direction
//   - Wheel            → page-step
//
// The widget speaks pixel coordinates only — the consumer translates
// pixel scroll/viewport into samples and zoom internally. Works for
// horizontal and vertical orientations.

#pragma once

#include <QWidget>
#include <Qt>

namespace dawcast::widgets {

class TimelineScrollBar : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineScrollBar(Qt::Orientation orientation,
                               QWidget* parent = nullptr);
    ~TimelineScrollBar() override = default;

    /// Total content size in pixels.
    void setContentSize(int contentPx);
    int  contentSize() const { return m_contentPx; }

    /// Currently visible viewport size in pixels (must be <= contentSize).
    void setViewportSize(int viewportPx);
    int  viewportSize() const { return m_viewportPx; }

    /// Current scroll position in pixels (0 .. contentSize - viewportSize).
    void setValue(int valuePx);
    int  value() const { return m_value; }

    /// Block emitting signals while applying programmatic updates.
    void setUpdatesQuietly(bool quiet) { m_quiet = quiet; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// Emitted when the user pans (drags thumb body, clicks track,
    /// or scroll-wheels). The viewport size is unchanged.
    void valueChanged(int valuePx);

    /// Emitted when the user resizes the thumb (drags a grip). The new
    /// viewport size, new scroll value (which may have shifted), and the
    /// *original* viewport at drag-start are all reported so the consumer
    /// can compute the multiplicative zoom factor as oldViewport/newViewport.
    /// Consumers should update both scroll AND zoom together.
    void zoomChanged(int newViewportPx, int newValuePx, int oldViewportPx);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum Zone { ZoneNone, ZoneLeftGrip, ZoneBody, ZoneRightGrip,
                ZoneTrackLeft, ZoneTrackRight };

    /// Compute the thumb rect in widget coordinates.
    void computeThumbRect(int& start, int& length) const;
    /// Hit-test mouse position to one of the zones.
    Zone hitTest(const QPoint& pos) const;
    /// Length of the active dimension (width if horizontal, height if vertical).
    int  activeLength() const;
    /// Coordinate from QPoint along the active axis.
    int  axisCoord(const QPoint& p) const;

    Qt::Orientation m_orientation;
    int m_contentPx  = 1000;
    int m_viewportPx = 200;
    int m_value      = 0;
    int m_gripPx     = 10;     // pixel width of each end grip
    int m_minThumbPx = 24;     // smallest the thumb can shrink to (zoom-in cap)
    bool m_quiet     = false;

    // ── Drag state ───────────────────────────────────────────────────
    Zone   m_dragZone   = ZoneNone;
    int    m_dragStart  = 0;
    int    m_dragValue  = 0;
    int    m_dragViewport = 0;
    bool   m_hover      = false;
};

} // namespace dawcast::widgets
