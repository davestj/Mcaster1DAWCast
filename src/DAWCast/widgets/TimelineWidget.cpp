// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimelineWidget.h"
#include "TimeStretchDialog.h"
#include "CrossfadeEditorDialog.h"
#include "Timeline.h"
#include "AudioTrack.h"
#include "VideoTrack.h"
#include "MidiTrack.h"
#include "MidiClip.h"
#include "MidiEvent.h"
#include "Clip.h"
#include "Marker.h"
#include "Automation.h"
#include "../audio_engine/WaveformCache.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QMenu>
#include <QFontMetrics>
#include <QFileInfo>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
}
#endif

#include <cmath>
#include <algorithm>

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

// Automation curve colors by parameter name
const QColor kAutoVolumeColor(255, 204, 0);       // yellow  #FFCC00
const QColor kAutoPanColor(0, 204, 255);           // cyan    #00CCFF
const QColor kAutoEffectColor(0, 204, 102);        // green   #00CC66
constexpr int kAutoPointRadius = 3;                // px radius for automation breakpoints
constexpr int kAutoPointHitRadius = 6;             // px hit-test radius
} // anonymous namespace

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMinimumHeight(kRulerHeight + kTrackHeight * 2);

    // Repaint when waveform data becomes available
    connect(WaveformCache::instance(), &WaveformCache::waveformReady,
            this, &TimelineWidget::onWaveformReady);
}

void TimelineWidget::onWaveformReady(const QString& /*filePath*/)
{
    update();
}

TimelineWidget::~TimelineWidget() = default;

void TimelineWidget::setTimeline(Timeline* timeline)
{
    m_timeline = timeline;

    if (m_timeline) {
        // Repaint when the playhead moves (e.g., during real-time playback)
        connect(m_timeline, &Timeline::playheadChanged,
                this, [this](int64_t) { update(); });
        // Repaint when tracks are added or removed
        connect(m_timeline, &Timeline::trackAdded,
                this, [this](int) { update(); });
        connect(m_timeline, &Timeline::trackRemoved,
                this, [this](int) { update(); });
    }

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
        auto* midiTrack  = qobject_cast<MidiTrack*>(trackObj);
        if (audioTrack) trackName = audioTrack->name();
        else if (videoTrack) trackName = videoTrack->name();
        else if (midiTrack) trackName = midiTrack->name();
        if (trackName.isEmpty()) trackName = tr("Track %1").arg(t + 1);

        p.setPen(QColor(160, 160, 170));
        QFont nameFont = font();
        nameFont.setPointSize(9);
        p.setFont(nameFont);
        p.drawText(4, yTop + 14, trackName);

        // Draw clips on this track
        if (midiTrack) {
            // MIDI track: draw MIDI clips
            for (int c = 0; c < midiTrack->clipCount(); ++c) {
                MidiClip* mclip = midiTrack->clip(c);
                if (!mclip) continue;
                drawMidiClip(p, mclip, t, c, yTop);
            }
        } else {
            // Audio/Video track: draw audio clips
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

        // Draw automation curves on audio tracks (overlaid on clips)
        if (audioTrack) {
            drawAutomation(p, audioTrack, t);
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

    // Waveform rendering (real data or loading placeholder)
    if (clipW > 10) {
        drawWaveform(p, clip, baseColor, x1, clipY, clipW, clipH);
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

void TimelineWidget::drawMidiClip(QPainter& p, MidiClip* clip, int trackIndex, int clipIndex, int yTop)
{
    if (!m_timeline) return;

    double bpm = m_timeline->tempo();
    int sr = m_timeline->sampleRate();

    int64_t clipStartSample = clip->timelinePosition();
    int64_t clipEndSample   = clipStartSample + MidiClip::ticksToSamples(clip->durationTicks(), bpm, sr);

    int x1 = sampleToPixel(clipStartSample, m_scroll, m_zoom);
    int x2 = sampleToPixel(clipEndSample, m_scroll, m_zoom);
    if (x2 < 0 || x1 > width()) return;

    int clipY = yTop + 18;
    int clipH = kTrackHeight - 22;
    int clipW = qMax(4, x2 - x1);

    // MIDI clips use a purple/teal color scheme
    static const QColor kMidiClipColors[] = {
        QColor(120, 80, 200),
        QColor(80, 160, 180),
        QColor(160, 100, 180),
        QColor(100, 180, 140),
    };
    QColor baseColor = kMidiClipColors[(trackIndex * 3 + clipIndex) % 4];
    QColor fillColor = baseColor;
    fillColor.setAlpha(160);

    // Rounded rectangle body
    QRect clipRect(x1, clipY, clipW, clipH);
    p.setBrush(fillColor);
    p.setPen(QPen(QColor(200, 200, 200, 100), 1));
    p.drawRoundedRect(clipRect, 3, 3);

    // Draw miniature note lines (piano roll preview)
    if (clipW > 10) {
        const auto& events = clip->events();
        int noteMin = 127, noteMax = 0;

        // Find note range for scaling
        for (const MidiEvent& ev : events) {
            if (ev.type != MidiEvent::NoteOn) continue;
            noteMin = std::min(noteMin, static_cast<int>(ev.note));
            noteMax = std::max(noteMax, static_cast<int>(ev.note));
        }
        if (noteMin > noteMax) { noteMin = 48; noteMax = 84; } // default range
        int noteRange = qMax(1, noteMax - noteMin + 1);

        // Scale: map tick range to clip width, note range to clip height
        int64_t totalTicks = clip->durationTicks();
        if (totalTicks <= 0) totalTicks = 1;

        p.setPen(Qt::NoPen);
        QColor noteColor = baseColor.lighter(160);
        noteColor.setAlpha(200);
        p.setBrush(noteColor);

        for (const MidiEvent& ev : events) {
            if (ev.type != MidiEvent::NoteOn) continue;

            double xFrac = static_cast<double>(ev.tick) / totalTicks;
            double wFrac = static_cast<double>(ev.durationTicks) / totalTicks;
            double yFrac = 1.0 - static_cast<double>(ev.note - noteMin) / noteRange;

            int nx = x1 + static_cast<int>(xFrac * clipW);
            int nw = qMax(2, static_cast<int>(wFrac * clipW));
            int ny = clipY + 2 + static_cast<int>(yFrac * (clipH - 4));
            int nh = qMax(1, (clipH - 4) / noteRange);

            p.drawRect(nx, ny, nw, nh);
        }
    }

    // Clip label: instrument name or "MIDI"
    if (clipW > 30) {
        p.setPen(QColor(240, 240, 240));
        QFont clipFont = font();
        clipFont.setPointSize(8);
        p.setFont(clipFont);
        QFontMetrics fm(clipFont);
        QString label = QStringLiteral("MIDI");
        QString elided = fm.elidedText(label, Qt::ElideRight, clipW - 6);
        p.drawText(x1 + 3, clipY + 11, elided);
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    m_dragging = true;
    m_draggingAuto = false;
    m_draggingAutoPoint = -1;
    m_dragStart = event->pos();

    // Click in ruler area: set playhead
    if (event->pos().y() < kRulerHeight && m_timeline) {
        int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
        m_timeline->setPlayhead(qMax(int64_t(0), sample));
        emit playheadMoved(m_timeline->playhead());
        update();
        return;
    }

    // Click in track area
    if (m_timeline && event->pos().y() >= kRulerHeight) {
        int trackIdx = (event->pos().y() - kRulerHeight) / kTrackHeight;
        if (trackIdx >= 0 && trackIdx < m_timeline->trackCount()) {
            QObject* trackObj = m_timeline->track(trackIdx);
            auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
            auto* videoTrack = qobject_cast<VideoTrack*>(trackObj);

            // Check for automation point hit first (when Alt is held)
            if (audioTrack && m_editingAutoTrack == trackIdx
                && !m_editingAutoParam.isEmpty()) {
                int ptIdx = hitTestAutomationPoint(trackIdx, event->pos().x(), event->pos().y());
                if (ptIdx >= 0) {
                    // Right-click on a point: delete it
                    if (event->button() == Qt::RightButton) {
                        Automation* automation = audioTrack->automation(m_editingAutoParam);
                        if (automation) {
                            automation->removePoint(ptIdx);
                            emit automationPointRemoved(trackIdx, m_editingAutoParam, ptIdx);
                        }
                        m_dragging = false;
                        update();
                        return;
                    }
                    // Left-click: start dragging the point
                    m_draggingAuto = true;
                    m_draggingAutoPoint = ptIdx;
                    update();
                    return;
                }

                // Click on automation lane (not on a point): add a new point
                if (event->button() == Qt::LeftButton
                    && (event->modifiers() & Qt::AltModifier)) {
                    Automation* automation = audioTrack->automation(m_editingAutoParam);
                    if (automation) {
                        int64_t clickSample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
                        int yTop = kRulerHeight + trackIdx * kTrackHeight;
                        float value = 1.0f - static_cast<float>(event->pos().y() - yTop)
                                           / static_cast<float>(kTrackHeight);
                        value = std::clamp(value, 0.0f, 1.0f);
                        automation->addPoint(clickSample, value);
                        emit automationPointAdded(trackIdx, m_editingAutoParam,
                                                  clickSample, value);
                    }
                    update();
                    return;
                }
            }

            // Normal clip selection
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
        // Automation point drag
        if (m_draggingAuto && m_draggingAutoPoint >= 0 && m_timeline
            && m_editingAutoTrack >= 0 && !m_editingAutoParam.isEmpty()) {
            QObject* trackObj = m_timeline->track(m_editingAutoTrack);
            auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
            if (audioTrack) {
                Automation* automation = audioTrack->automation(m_editingAutoParam);
                if (automation && m_draggingAutoPoint < automation->pointCount()) {
                    int64_t newTime = pixelToSample(event->pos().x(), m_scroll, m_zoom);
                    int yTop = kRulerHeight + m_editingAutoTrack * kTrackHeight;
                    float newValue = 1.0f - static_cast<float>(event->pos().y() - yTop)
                                         / static_cast<float>(kTrackHeight);
                    newValue = std::clamp(newValue, 0.0f, 1.0f);
                    newTime = qMax(int64_t(0), newTime);

                    // Remove old point and re-add at new position
                    automation->removePoint(m_draggingAutoPoint);
                    automation->addPoint(newTime, newValue);

                    // The point may have moved to a different index due to sorting
                    auto pts = automation->points();
                    for (int i = 0; i < pts.size(); ++i) {
                        if (pts[i].time == newTime && qFuzzyCompare(pts[i].value, newValue)) {
                            m_draggingAutoPoint = i;
                            break;
                        }
                    }

                    emit automationPointMoved(m_editingAutoTrack, m_editingAutoParam,
                                              m_draggingAutoPoint, newTime, newValue);
                }
            }
            update();
        }
        // Dragging in ruler: update playhead
        else if (m_dragStart.y() < kRulerHeight && m_timeline) {
            int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            m_timeline->setPlayhead(qMax(int64_t(0), sample));
            emit playheadMoved(m_timeline->playhead());
            update();
        } else if (!m_selectedClips.isEmpty()) {
            // Clip drag: emit move signal based on delta
            int64_t deltaSamples = pixelToSample(event->pos().x(), 0, m_zoom)
                                 - pixelToSample(m_dragStart.x(), 0, m_zoom);
            for (int clipId : std::as_const(m_selectedClips)) {
                emit clipMoved(clipId, deltaSamples);
            }
            m_dragStart = event->pos();
            update();
        } else {
            // Rubber-band selection
            m_rubberBand = QRect(m_dragStart, event->pos()).normalized();
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    m_draggingAuto = false;
    m_draggingAutoPoint = -1;
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

    // ── Edit Crossfade — shown when two clips overlap or are adjacent ──
    if (m_timeline && event->pos().y() >= kRulerHeight) {
        int xfTrackIdx = (event->pos().y() - kRulerHeight) / kTrackHeight;
        if (xfTrackIdx >= 0 && xfTrackIdx < m_timeline->trackCount()) {
            auto* xfTrack = qobject_cast<AudioTrack*>(m_timeline->track(xfTrackIdx));
            if (xfTrack && xfTrack->clipCount() >= 2) {
                // Convert mouse X to timeline sample position
                int64_t clickPos = static_cast<int64_t>(
                    (event->pos().x() / m_zoom) + m_scroll);

                // Find two clips near the click position that overlap or are adjacent
                Clip* clipA = nullptr;
                Clip* clipB = nullptr;
                for (int ci = 0; ci < xfTrack->clipCount() - 1; ++ci) {
                    Clip* a = xfTrack->clip(ci);
                    Clip* b = xfTrack->clip(ci + 1);
                    if (!a || !b) continue;

                    int64_t aEnd   = a->endPosition();
                    int64_t bStart = b->timelinePosition();
                    // Overlapping or adjacent (within a small tolerance)
                    int64_t gap = bStart - aEnd;
                    if (gap <= 4800) { // within ~100ms at 48kHz
                        // Check if click is near the boundary
                        int64_t boundary = (aEnd + bStart) / 2;
                        int64_t tolerance = std::max(int64_t(48000), // 1 second
                                                     (aEnd - a->timelinePosition()) / 4);
                        if (std::abs(clickPos - boundary) < tolerance) {
                            clipA = a;
                            clipB = b;
                            break;
                        }
                    }
                }

                if (clipA && clipB) {
                    menu.addAction(tr("Edit Crossfade..."), this,
                                   [this, clipA, clipB]() {
                        CrossfadeEditorDialog dlg(clipA, clipB, this);
                        if (dlg.exec() == QDialog::Accepted) {
                            update();
                        }
                    });
                    menu.addSeparator();
                }
            }
        }
    }

    // Automation lane selection submenu
    if (m_timeline && event->pos().y() >= kRulerHeight) {
        int trackIdx = (event->pos().y() - kRulerHeight) / kTrackHeight;
        if (trackIdx >= 0 && trackIdx < m_timeline->trackCount()) {
            auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(trackIdx));
            if (audioTrack) {
                auto* autoMenu = menu.addMenu(tr("Automation"));

                // Standard parameter lanes
                struct AutoParam { QString name; QString label; };
                const AutoParam params[] = {
                    { QStringLiteral("volume"), tr("Volume") },
                    { QStringLiteral("pan"),    tr("Pan") },
                    { QStringLiteral("effect"), tr("Effect Send") },
                };

                for (const auto& param : params) {
                    auto* act = autoMenu->addAction(param.label);
                    act->setCheckable(true);
                    act->setChecked(m_editingAutoTrack == trackIdx
                                    && m_editingAutoParam == param.name);
                    connect(act, &QAction::triggered, this,
                            [this, trackIdx, paramName = param.name, audioTrack](bool checked) {
                        if (checked) {
                            m_editingAutoTrack = trackIdx;
                            m_editingAutoParam = paramName;
                            // Ensure the automation lane exists
                            audioTrack->addAutomation(paramName);
                        } else {
                            m_editingAutoTrack = -1;
                            m_editingAutoParam.clear();
                        }
                        update();
                    });
                }

                auto* hideAct = autoMenu->addAction(tr("Hide All"));
                connect(hideAct, &QAction::triggered, this, [this] {
                    m_editingAutoTrack = -1;
                    m_editingAutoParam.clear();
                    update();
                });

                menu.addSeparator();
            }
        }
    }

    // Time Stretch / Pitch Shift (available when a clip is selected)
    if (!m_selectedClips.isEmpty()) {
        menu.addAction(tr("Time Stretch / Pitch Shift..."), this, [this] {
            auto* dialog = new TimeStretchDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &QDialog::accepted, this, [this, dialog] {
                // In a full implementation, this would apply the TimeStretch
                // processor to the selected clip's audio data.
                Q_UNUSED(dialog);
                update();
            });
            dialog->show();
        });
        menu.addSeparator();
    }

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

void TimelineWidget::drawWaveform(QPainter& p, Clip* clip, const QColor& baseColor,
                                  int clipX, int clipY, int clipW, int clipH)
{
    auto* cache = WaveformCache::instance();
    const WaveformData* waveform = cache->getWaveform(clip->sourcePath());

    if (!waveform) {
        // Data not yet decoded — draw a "Loading..." placeholder
        p.setPen(QColor(180, 180, 180, 120));
        QFont loadFont = font();
        loadFont.setPointSize(8);
        p.setFont(loadFont);
        p.drawText(clipX + clipW / 2 - 25, clipY + clipH / 2 + 4,
                   tr("Loading..."));
        return;
    }

    if (waveform->peaks.empty() || waveform->sampleRate <= 0)
        return;

    // Drawing area (leave 2px padding inside the clip rect)
    int drawX = clipX + 2;
    int drawW = clipW - 4;
    if (drawW <= 0) return;

    int midY  = clipY + clipH / 2;
    int halfH = (clipH - 4) / 2;  // vertical amplitude in pixels

    // Map from clip pixel position to source sample position.
    // The clip shows source audio from sourceIn to sourceOut.
    // Each waveform block covers `blockSize` source frames.
    int blockSize   = waveform->blockSize;
    int64_t srcIn   = clip->sourceIn();
    int64_t srcOut  = clip->sourceOut();
    int64_t srcLen  = srcOut - srcIn;
    if (srcLen <= 0) return;

    int numBlocks = static_cast<int>(waveform->peaks.size());

    // Colors: peak outline = lighter base, RMS body = base color
    QColor peakColor = baseColor.lighter(150);
    peakColor.setAlpha(200);
    QColor rmsColor = baseColor;
    rmsColor.setAlpha(220);

    // For each pixel column in the clip, determine which waveform blocks
    // fall under it and take the max peak and max RMS.
    p.setRenderHint(QPainter::Antialiasing, false);

    for (int px = 0; px < drawW; ++px) {
        // Source sample range covered by this pixel column
        double srcFrac0 = static_cast<double>(px)     / drawW;
        double srcFrac1 = static_cast<double>(px + 1) / drawW;
        int64_t samp0 = srcIn + static_cast<int64_t>(srcFrac0 * srcLen);
        int64_t samp1 = srcIn + static_cast<int64_t>(srcFrac1 * srcLen);

        // Corresponding waveform block range
        int blk0 = static_cast<int>(samp0 / blockSize);
        int blk1 = static_cast<int>(samp1 / blockSize);
        blk0 = std::clamp(blk0, 0, numBlocks - 1);
        blk1 = std::clamp(blk1, 0, numBlocks - 1);

        // Gather max peak and max RMS across covered blocks
        float maxPeak = 0.0f;
        float maxRms  = 0.0f;
        for (int b = blk0; b <= blk1; ++b) {
            float pk = waveform->peaks[static_cast<size_t>(b)];
            float rm = waveform->rms[static_cast<size_t>(b)];
            if (pk > maxPeak) maxPeak = pk;
            if (rm > maxRms)  maxRms  = rm;
        }

        // At high zoom (fewer blocks than pixels), interpolate between blocks
        if (blk0 == blk1 && blk0 > 0 && blk0 < numBlocks - 1) {
            double exactBlock = static_cast<double>(samp0) / blockSize;
            double frac = exactBlock - std::floor(exactBlock);
            int bPrev = blk0;
            int bNext = std::min(blk0 + 1, numBlocks - 1);
            maxPeak = static_cast<float>(
                waveform->peaks[static_cast<size_t>(bPrev)] * (1.0 - frac) +
                waveform->peaks[static_cast<size_t>(bNext)] * frac);
            maxRms = static_cast<float>(
                waveform->rms[static_cast<size_t>(bPrev)] * (1.0 - frac) +
                waveform->rms[static_cast<size_t>(bNext)] * frac);
        }

        // Clamp to [0, 1]
        maxPeak = std::clamp(maxPeak, 0.0f, 1.0f);
        maxRms  = std::clamp(maxRms,  0.0f, 1.0f);

        int peakH = static_cast<int>(maxPeak * halfH);
        int rmsH  = static_cast<int>(maxRms  * halfH);

        int x = drawX + px;

        // Draw peak line (outer extent)
        if (peakH > 0) {
            p.setPen(peakColor);
            p.drawLine(x, midY - peakH, x, midY + peakH);
        }

        // Draw RMS body (inner, darker region)
        if (rmsH > 0) {
            p.setPen(rmsColor);
            p.drawLine(x, midY - rmsH, x, midY + rmsH);
        }
    }

    p.setRenderHint(QPainter::Antialiasing, true);
}

// ── Automation Lane Visualization ──────────────────────────────────────────

void TimelineWidget::drawAutomation(QPainter& painter, AudioTrack* track, int trackIndex)
{
    if (!track) return;

    const QList<Automation*> automations = track->automations();
    const QStringList names = track->automationNames();

    if (automations.isEmpty()) return;

    int yTop = kRulerHeight + trackIndex * kTrackHeight;
    int laneH = kTrackHeight;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int a = 0; a < automations.size(); ++a) {
        Automation* automation = automations.at(a);
        if (!automation || automation->pointCount() == 0) continue;

        const QString& paramName = names.at(a);
        QColor curveColor = automationColor(paramName);
        QColor fillColor = curveColor;
        fillColor.setAlpha(40);

        // Determine if this is the actively-edited lane (draw brighter)
        bool isActive = (m_editingAutoTrack == trackIndex
                         && m_editingAutoParam == paramName);
        if (!isActive) {
            curveColor.setAlpha(100);
            fillColor.setAlpha(20);
        }

        auto points = automation->points();
        if (points.isEmpty()) continue;

        // Build the automation curve path
        QPainterPath curvePath;
        QPainterPath fillPath;

        bool firstVisible = true;
        for (int i = 0; i < points.size(); ++i) {
            int px = sampleToPixel(points[i].time, m_scroll, m_zoom);
            // Map value (0.0..1.0) to y position within the track lane
            // value=1.0 -> top of lane, value=0.0 -> bottom of lane
            float val = std::clamp(points[i].value, 0.0f, 1.0f);
            int py = yTop + static_cast<int>((1.0f - val) * laneH);

            if (firstVisible) {
                curvePath.moveTo(px, py);
                fillPath.moveTo(px, yTop + laneH); // start at baseline
                fillPath.lineTo(px, py);
                firstVisible = false;
            } else {
                curvePath.lineTo(px, py);
                fillPath.lineTo(px, py);
            }
        }

        // Close the fill path along the bottom of the lane
        if (!firstVisible) {
            int lastPx = sampleToPixel(points.last().time, m_scroll, m_zoom);
            fillPath.lineTo(lastPx, yTop + laneH);
            fillPath.closeSubpath();

            // Draw semi-transparent fill below the curve
            painter.setBrush(fillColor);
            painter.setPen(Qt::NoPen);
            painter.drawPath(fillPath);

            // Draw the curve line
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(curveColor, isActive ? 2.0 : 1.0));
            painter.drawPath(curvePath);
        }

        // Draw breakpoint circles
        for (int i = 0; i < points.size(); ++i) {
            int px = sampleToPixel(points[i].time, m_scroll, m_zoom);
            float val = std::clamp(points[i].value, 0.0f, 1.0f);
            int py = yTop + static_cast<int>((1.0f - val) * laneH);

            // Skip off-screen points
            if (px < -kAutoPointRadius || px > width() + kAutoPointRadius) continue;

            if (isActive) {
                // Active lane: filled circles
                painter.setBrush(curveColor);
                painter.setPen(QPen(curveColor.darker(120), 1));
                painter.drawEllipse(QPoint(px, py), kAutoPointRadius, kAutoPointRadius);
            } else {
                // Inactive lane: outline circles
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(curveColor, 1));
                painter.drawEllipse(QPoint(px, py), kAutoPointRadius - 1, kAutoPointRadius - 1);
            }
        }

        // Draw parameter label at left edge if active
        if (isActive && !points.isEmpty()) {
            QFont autoFont = font();
            autoFont.setPointSize(7);
            autoFont.setBold(true);
            painter.setFont(autoFont);
            painter.setPen(curveColor);
            painter.drawText(4, yTop + laneH - 4, paramName.toUpper());
        }
    }

    painter.restore();
}

int TimelineWidget::hitTestAutomationPoint(int trackIndex, int x, int y) const
{
    if (!m_timeline || m_editingAutoParam.isEmpty()) return -1;

    QObject* trackObj = m_timeline->track(trackIndex);
    auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
    if (!audioTrack) return -1;

    Automation* automation = audioTrack->automation(m_editingAutoParam);
    if (!automation) return -1;

    int yTop = kRulerHeight + trackIndex * kTrackHeight;
    int laneH = kTrackHeight;

    auto points = automation->points();
    for (int i = 0; i < points.size(); ++i) {
        int px = sampleToPixel(points[i].time, m_scroll, m_zoom);
        float val = std::clamp(points[i].value, 0.0f, 1.0f);
        int py = yTop + static_cast<int>((1.0f - val) * laneH);

        int dx = x - px;
        int dy = y - py;
        if (dx * dx + dy * dy <= kAutoPointHitRadius * kAutoPointHitRadius) {
            return i;
        }
    }

    return -1;
}

QColor TimelineWidget::automationColor(const QString& paramName) const
{
    if (paramName == QLatin1String("volume"))
        return kAutoVolumeColor;
    if (paramName == QLatin1String("pan"))
        return kAutoPanColor;
    // Default for effect parameters and anything else
    return kAutoEffectColor;
}

// ── Drag-and-Drop File Import ─────────────────────────────────────────────

QStringList TimelineWidget::supportedAudioExtensions()
{
    return { QStringLiteral("wav"), QStringLiteral("mp3"),
             QStringLiteral("flac"), QStringLiteral("aac"),
             QStringLiteral("ogg"), QStringLiteral("opus") };
}

QStringList TimelineWidget::supportedVideoExtensions()
{
    return { QStringLiteral("mp4"), QStringLiteral("mkv"),
             QStringLiteral("webm"), QStringLiteral("avi") };
}

bool TimelineWidget::isSupportedMediaFile(const QString& filePath) const
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    return supportedAudioExtensions().contains(ext)
        || supportedVideoExtensions().contains(ext);
}

bool TimelineWidget::isVideoFile(const QString& filePath) const
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    return supportedVideoExtensions().contains(ext);
}

int64_t TimelineWidget::probeDuration(const QString& filePath) const
{
    int sampleRate = 48000;
    if (m_timeline)
        sampleRate = m_timeline->sampleRate() > 0 ? m_timeline->sampleRate() : 48000;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = nullptr;
    QByteArray pathUtf8 = filePath.toUtf8();
    if (avformat_open_input(&fmtCtx, pathUtf8.constData(), nullptr, nullptr) < 0)
        return sampleRate * 10; // fallback: 10 seconds

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return sampleRate * 10;
    }

    // Use the overall container duration (in AV_TIME_BASE units)
    int64_t durationSamples = sampleRate * 10; // fallback
    if (fmtCtx->duration > 0) {
        double durationSec = static_cast<double>(fmtCtx->duration) / AV_TIME_BASE;
        durationSamples = static_cast<int64_t>(durationSec * sampleRate);
    }

    avformat_close_input(&fmtCtx);
    return durationSamples;
#else
    // Without FFmpeg, fall back to WaveformCache data if available
    auto* cache = WaveformCache::instance();
    const WaveformData* wf = cache->getWaveform(filePath);
    if (wf && wf->totalFrames > 0) {
        return wf->totalFrames;
    }
    // Last resort: assume 10 seconds
    return static_cast<int64_t>(sampleRate) * 10;
#endif
}

void TimelineWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) continue;
        if (isSupportedMediaFile(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
}

void TimelineWidget::dropEvent(QDropEvent* event)
{
    if (!m_timeline) return;

    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    int64_t dropSample = pixelToSample(static_cast<int>(event->position().x()),
                                        m_scroll, m_zoom);
    dropSample = qMax(int64_t(0), dropSample);

    // Determine which track the drop landed on (if any)
    int dropTrackIdx = -1;
    int dropY = static_cast<int>(event->position().y());
    if (dropY >= kRulerHeight) {
        dropTrackIdx = (dropY - kRulerHeight) / kTrackHeight;
        if (dropTrackIdx >= m_timeline->trackCount())
            dropTrackIdx = -1; // dropped below existing tracks
    }

    int64_t cursorPos = dropSample;

    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) continue;
        const QString filePath = url.toLocalFile();
        if (!isSupportedMediaFile(filePath)) continue;

        bool video = isVideoFile(filePath);

        // Find or create the target track
        int targetTrackIdx = dropTrackIdx;
        if (targetTrackIdx < 0) {
            // No valid track at drop position: create a new one
            if (video)
                m_timeline->addVideoTrack();
            else
                m_timeline->addAudioTrack();
            targetTrackIdx = m_timeline->trackCount() - 1;
        }

        // Verify the target track type is compatible, otherwise create new
        QObject* trackObj = m_timeline->track(targetTrackIdx);
        auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
        auto* videoTrack = qobject_cast<VideoTrack*>(trackObj);

        if (video && !videoTrack) {
            m_timeline->addVideoTrack();
            targetTrackIdx = m_timeline->trackCount() - 1;
            trackObj = m_timeline->track(targetTrackIdx);
            videoTrack = qobject_cast<VideoTrack*>(trackObj);
        } else if (!video && !audioTrack) {
            m_timeline->addAudioTrack();
            targetTrackIdx = m_timeline->trackCount() - 1;
            trackObj = m_timeline->track(targetTrackIdx);
            audioTrack = qobject_cast<AudioTrack*>(trackObj);
        }

        // Probe file duration
        int64_t durationSamples = probeDuration(filePath);

        // Create the clip
        auto* clip = new Clip(trackObj);
        clip->setSourcePath(filePath);
        clip->setSourceIn(0);
        clip->setSourceOut(durationSamples);
        clip->setTimelinePosition(cursorPos);

        // Add clip to the appropriate track
        if (audioTrack)
            audioTrack->addClip(clip);
        else if (videoTrack)
            videoTrack->addClip(clip);

        // Request waveform decode for audio files
        if (!video) {
            WaveformCache::instance()->requestWaveform(filePath);
        }

        // Advance cursor for the next dropped file so they don't overlap
        cursorPos += durationSamples;
    }

    event->acceptProposedAction();
    update();
}

} // namespace dawcast::widgets
