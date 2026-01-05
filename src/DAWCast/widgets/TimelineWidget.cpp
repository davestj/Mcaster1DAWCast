// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimelineWidget.h"
#include "Timeline.h"
#include "AudioTrack.h"
#include "VideoTrack.h"
#include "Clip.h"
#include "Marker.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFontMetrics>
#include <QFileInfo>

#include <cmath>

namespace dawcast::widgets {

namespace {
constexpr int kRulerHeight     = 28;
constexpr int kTrackHeight     = 60;
constexpr int kMarkerSize      = 8;
constexpr float kMinZoom       = 0.001f;
constexpr float kMaxZoom       = 100.0f;
constexpr float kZoomFactor    = 1.15f;

// Alternating track lane colors
const QColor kTrackEven(42, 42, 48);
const QColor kTrackOdd(48, 48, 54);
const QColor kRulerBg(30, 30, 36);
const QColor kRulerText(180, 180, 180);
const QColor kRulerTick(100, 100, 100);
const QColor kPlayheadColor(220, 40, 40);
const QColor kClipBorder(200, 200, 200, 100);
const QColor kSelectionHighlight(100, 160, 255, 80);
const QColor kRubberBandFill(100, 160, 255, 40);
const QColor kRubberBandBorder(100, 160, 255, 160);

// Default clip colors when none is set
const QColor kClipColors[] = {
    QColor(70, 130, 200),
    QColor(200, 100, 70),
    QColor(80, 170, 100),
    QColor(180, 140, 60),
    QColor(140, 90, 180),
    QColor(60, 160, 170),
};
constexpr int kClipColorCount = sizeof(kClipColors) / sizeof(kClipColors[0]);
} // anonymous namespace

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kRulerHeight + kTrackHeight * 2);
}

TimelineWidget::~TimelineWidget() = default;

void TimelineWidget::setTimeline(Timeline* timeline)
{
    m_timeline = timeline;
    update();
}

void TimelineWidget::setZoom(float zoom)
{
    m_zoom = qBound(kMinZoom, zoom, kMaxZoom);
    update();
}

void TimelineWidget::setScroll(int64_t position)
{
    m_scroll = qMax(int64_t(0), position);
    update();
}

QList<int> TimelineWidget::selectedClips() const
{
    return m_selectedClips;
}

// Convert sample position to pixel x-coordinate
static inline int sampleToPixel(int64_t sample, int64_t scrollOffset, float zoom)
{
    return static_cast<int>((sample - scrollOffset) * static_cast<double>(zoom));
}

// Convert pixel x-coordinate to sample position
static inline int64_t pixelToSample(int px, int64_t scrollOffset, float zoom)
{
    return static_cast<int64_t>(px / static_cast<double>(zoom)) + scrollOffset;
}

void TimelineWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // Background
    p.fillRect(rect(), QColor(36, 36, 42));

    // --- Time ruler ---
    drawRuler(p, w);

    // --- Track lanes ---
    int trackCount = 0;
    if (m_timeline) {
        trackCount = m_timeline->trackCount();
    }

    for (int t = 0; t < trackCount; ++t) {
        int yTop = kRulerHeight + t * kTrackHeight;
        if (yTop > h) break;

        // Alternating lane background
        QColor laneBg = (t % 2 == 0) ? kTrackEven : kTrackOdd;
        p.fillRect(0, yTop, w, kTrackHeight, laneBg);

        // Lane separator line
        p.setPen(QPen(QColor(60, 60, 66), 1));
        p.drawLine(0, yTop + kTrackHeight - 1, w, yTop + kTrackHeight - 1);

        // Track name
        QObject* trackObj = m_timeline->track(t);
        QString trackName;
        auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
        auto* videoTrack = qobject_cast<VideoTrack*>(trackObj);
        if (audioTrack) trackName = audioTrack->name();
        else if (videoTrack) trackName = videoTrack->name();
        if (trackName.isEmpty()) trackName = tr("Track %1").arg(t + 1);

        p.setPen(QColor(160, 160, 170));
        QFont nameFont = font();
        nameFont.setPointSize(9);
        p.setFont(nameFont);
        p.drawText(4, yTop + 14, trackName);

        // Draw clips on this track
        int clipCount = 0;
        if (audioTrack) clipCount = audioTrack->clipCount();
        else if (videoTrack) clipCount = videoTrack->clipCount();

        for (int c = 0; c < clipCount; ++c) {
            Clip* clip = nullptr;
            if (audioTrack) clip = audioTrack->clip(c);
            else if (videoTrack) clip = videoTrack->clip(c);
            if (!clip) continue;

            drawClip(p, clip, t, c, yTop);
        }
    }

    // --- Playhead ---
    if (m_timeline) {
        int phX = sampleToPixel(m_timeline->playhead(), m_scroll, m_zoom);
        if (phX >= 0 && phX <= w) {
            p.setPen(QPen(kPlayheadColor, 2));
            p.drawLine(phX, 0, phX, h);

            // Playhead top triangle
            QPainterPath tri;
            tri.moveTo(phX - 5, 0);
            tri.lineTo(phX + 5, 0);
            tri.lineTo(phX, 8);
            tri.closeSubpath();
            p.setBrush(kPlayheadColor);
            p.setPen(Qt::NoPen);
            p.drawPath(tri);
        }
    }

    // --- Rubber-band selection ---
    if (m_dragging && !m_rubberBand.isNull()) {
        p.setBrush(kRubberBandFill);
        p.setPen(QPen(kRubberBandBorder, 1, Qt::DashLine));
        p.drawRect(m_rubberBand);
    }
}

void TimelineWidget::drawRuler(QPainter& p, int viewWidth)
{
    p.fillRect(0, 0, viewWidth, kRulerHeight, kRulerBg);

    // Bottom line
    p.setPen(QPen(QColor(80, 80, 90), 1));
    p.drawLine(0, kRulerHeight - 1, viewWidth, kRulerHeight - 1);

    if (!m_timeline) return;

    int sampleRate = m_timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;

    // Determine a nice tick interval based on zoom
    // samplesPerPixel = 1.0 / zoom
    double samplesPerPixel = 1.0 / static_cast<double>(m_zoom);
    double secondsPerPixel = samplesPerPixel / sampleRate;

    // We want major ticks roughly every 100 pixels
    double majorSeconds = secondsPerPixel * 100.0;
    // Snap to nice values: 0.1, 0.5, 1, 2, 5, 10, 30, 60...
    static const double niceIntervals[] = {
        0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0
    };
    double chosenInterval = niceIntervals[sizeof(niceIntervals)/sizeof(niceIntervals[0]) - 1];
    for (double interval : niceIntervals) {
        if (interval >= majorSeconds) {
            chosenInterval = interval;
            break;
        }
    }

    // First visible time in seconds
    double startTimeSec = static_cast<double>(m_scroll) / sampleRate;
    double firstTick = std::ceil(startTimeSec / chosenInterval) * chosenInterval;

    QFont rulerFont = font();
    rulerFont.setPointSize(8);
    p.setFont(rulerFont);
    QFontMetrics fm(rulerFont);

    for (double timeSec = firstTick; ; timeSec += chosenInterval) {
        int64_t sample = static_cast<int64_t>(timeSec * sampleRate);
        int px = sampleToPixel(sample, m_scroll, m_zoom);
        if (px > viewWidth) break;

        // Major tick
        p.setPen(kRulerTick);
        p.drawLine(px, kRulerHeight - 10, px, kRulerHeight - 1);

        // Time label
        p.setPen(kRulerText);
        int minutes = static_cast<int>(timeSec) / 60;
        double secs = timeSec - minutes * 60;
        QString label;
        if (chosenInterval < 1.0)
            label = QString("%1:%2").arg(minutes).arg(secs, 5, 'f', 2, QChar('0'));
        else
            label = QString("%1:%2").arg(minutes).arg(static_cast<int>(secs), 2, 10, QChar('0'));

        int textW = fm.horizontalAdvance(label);
        p.drawText(px - textW / 2, kRulerHeight - 13, label);

        // Minor ticks (4 subdivisions)
        double minorInterval = chosenInterval / 4.0;
        for (int m = 1; m < 4; ++m) {
            double minorSec = timeSec + m * minorInterval;
            int64_t minorSample = static_cast<int64_t>(minorSec * sampleRate);
            int mpx = sampleToPixel(minorSample, m_scroll, m_zoom);
            if (mpx >= 0 && mpx <= viewWidth) {
                p.setPen(QPen(kRulerTick, 1));
                p.drawLine(mpx, kRulerHeight - 5, mpx, kRulerHeight - 1);
            }
        }
    }
}

void TimelineWidget::drawClip(QPainter& p, Clip* clip, int trackIndex, int clipIndex, int yTop)
{
    int x1 = sampleToPixel(clip->timelinePosition(), m_scroll, m_zoom);
    int x2 = sampleToPixel(clip->endPosition(), m_scroll, m_zoom);
    if (x2 < 0 || x1 > width()) return;

    int clipY = yTop + 18;
    int clipH = kTrackHeight - 22;
    int clipW = qMax(4, x2 - x1);

    QColor baseColor = kClipColors[(trackIndex * 3 + clipIndex) % kClipColorCount];
    QColor fillColor = baseColor;
    fillColor.setAlpha(180);

    // Rounded rectangle clip body
    QRect clipRect(x1, clipY, clipW, clipH);
    p.setBrush(fillColor);
    p.setPen(QPen(kClipBorder, 1));
    p.drawRoundedRect(clipRect, 3, 3);

    // Waveform placeholder -- draw a simple sine-wave silhouette
    if (clipW > 10) {
        p.setPen(QPen(baseColor.lighter(140), 1));
        int midY = clipY + clipH / 2;
        int amp = clipH / 4;
        QPainterPath wavePath;
        for (int px = x1 + 2; px < x1 + clipW - 2; ++px) {
            double phase = (px - x1) * 0.08;
            double wave1 = std::sin(phase) * 0.6;
            double wave2 = std::sin(phase * 2.7) * 0.3;
            double wave3 = std::sin(phase * 0.3) * 0.8;
            double combined = (wave1 + wave2 + wave3) * 0.5;
            int y = midY - static_cast<int>(combined * amp);
            if (px == x1 + 2) wavePath.moveTo(px, y);
            else wavePath.lineTo(px, y);
        }
        p.setBrush(Qt::NoBrush);
        p.drawPath(wavePath);
    }

    // Filename text
    QFileInfo fi(clip->sourcePath());
    QString clipName = fi.fileName();
    if (!clipName.isEmpty() && clipW > 30) {
        p.setPen(QColor(240, 240, 240));
        QFont clipFont = font();
        clipFont.setPointSize(8);
        p.setFont(clipFont);
        QFontMetrics fm(clipFont);
        QString elided = fm.elidedText(clipName, Qt::ElideRight, clipW - 6);
        p.drawText(x1 + 3, clipY + 11, elided);
    }

    // Selection highlight
    if (m_selectedClips.contains(clipIndex)) {
        p.setBrush(kSelectionHighlight);
        p.setPen(QPen(QColor(100, 160, 255, 200), 2));
        p.drawRoundedRect(clipRect, 3, 3);
    }

    // Fade-in indicator
    if (clip->fadeIn() > 0) {
        int fadeW = sampleToPixel(clip->timelinePosition() + clip->fadeIn(), m_scroll, m_zoom) - x1;
        fadeW = qBound(0, fadeW, clipW);
        p.setPen(QPen(QColor(255, 255, 255, 120), 1));
        p.drawLine(x1, clipY + clipH, x1 + fadeW, clipY);
    }

    // Fade-out indicator
    if (clip->fadeOut() > 0) {
        int fadeW = x2 - sampleToPixel(clip->endPosition() - clip->fadeOut(), m_scroll, m_zoom);
        fadeW = qBound(0, fadeW, clipW);
        p.setPen(QPen(QColor(255, 255, 255, 120), 1));
        p.drawLine(x2 - fadeW, clipY, x2, clipY + clipH);
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    m_dragging = true;
    m_dragStart = event->pos();

    // Click in ruler area: set playhead
    if (event->pos().y() < kRulerHeight && m_timeline) {
        int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
        m_timeline->setPlayhead(qMax(int64_t(0), sample));
        emit playheadMoved(m_timeline->playhead());
        update();
        return;
    }

    // Click in track area: select clip
    if (m_timeline && event->pos().y() >= kRulerHeight) {
        int trackIdx = (event->pos().y() - kRulerHeight) / kTrackHeight;
        if (trackIdx >= 0 && trackIdx < m_timeline->trackCount()) {
            QObject* trackObj = m_timeline->track(trackIdx);
            auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
            auto* videoTrack = qobject_cast<VideoTrack*>(trackObj);

            int clipCount = 0;
            if (audioTrack) clipCount = audioTrack->clipCount();
            else if (videoTrack) clipCount = videoTrack->clipCount();

            int64_t clickSample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            bool found = false;
            for (int c = 0; c < clipCount; ++c) {
                Clip* clip = nullptr;
                if (audioTrack) clip = audioTrack->clip(c);
                else if (videoTrack) clip = videoTrack->clip(c);
                if (!clip) continue;

                if (clickSample >= clip->timelinePosition() && clickSample < clip->endPosition()) {
                    if (!(event->modifiers() & Qt::ControlModifier))
                        m_selectedClips.clear();
                    if (!m_selectedClips.contains(c))
                        m_selectedClips.append(c);
                    emit clipSelected(c);
                    found = true;
                    break;
                }
            }
            if (!found && !(event->modifiers() & Qt::ControlModifier)) {
                m_selectedClips.clear();
            }
        }
    }

    update();
    QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        // Dragging in ruler: update playhead
        if (m_dragStart.y() < kRulerHeight && m_timeline) {
            int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            m_timeline->setPlayhead(qMax(int64_t(0), sample));
            emit playheadMoved(m_timeline->playhead());
        } else if (!m_selectedClips.isEmpty()) {
            // Clip drag: emit move signal based on delta
            int64_t deltaSamples = pixelToSample(event->pos().x(), 0, m_zoom)
                                 - pixelToSample(m_dragStart.x(), 0, m_zoom);
            for (int clipId : std::as_const(m_selectedClips)) {
                emit clipMoved(clipId, deltaSamples);
            }
            m_dragStart = event->pos();
        } else {
            // Rubber-band selection
            m_rubberBand = QRect(m_dragStart, event->pos()).normalized();
        }
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

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    // Zoom in/out centered on cursor
    float oldZoom = m_zoom;
    if (event->angleDelta().y() > 0)
        m_zoom = qMin(m_zoom * kZoomFactor, kMaxZoom);
    else
        m_zoom = qMax(m_zoom / kZoomFactor, kMinZoom);

    // Adjust scroll to keep the point under the cursor stable
    if (m_timeline) {
        int64_t sampleUnderCursor = pixelToSample(event->position().x(), m_scroll, oldZoom);
        int newPx = sampleToPixel(sampleUnderCursor, 0, m_zoom);
        m_scroll = pixelToSample(newPx - static_cast<int>(event->position().x()), 0, m_zoom);
        if (m_scroll < 0) m_scroll = 0;
    }

    update();
    event->accept();
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(tr("Cut"), this, []{});
    menu.addAction(tr("Copy"), this, []{});
    menu.addAction(tr("Paste"), this, []{});
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, [this]{
        m_selectedClips.clear();
        update();
    });
    menu.addSeparator();
    menu.addAction(tr("Zoom In"), this, [this]{
        setZoom(m_zoom * kZoomFactor);
    });
    menu.addAction(tr("Zoom Out"), this, [this]{
        setZoom(m_zoom / kZoomFactor);
    });
    menu.addAction(tr("Zoom to Fit"), this, [this]{
        if (m_timeline && m_timeline->duration() > 0) {
            m_scroll = 0;
            m_zoom = static_cast<float>(width()) / static_cast<float>(m_timeline->duration());
            update();
        }
    });
    menu.exec(event->globalPos());
}

} // namespace dawcast::widgets
