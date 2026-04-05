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
#include "TimelineCommands.h"
#include "../audio_engine/WaveformCache.h"
#include "../core/UndoManager.h"
#include "../dsp/DspChain.h"

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
#include <QInputDialog>
#include <QColorDialog>
#include <QFontMetrics>
#include <QFileInfo>
#include <QToolTip>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
}
#endif

#include <climits>
#include <cmath>
#include <algorithm>
#include <functional>

namespace dawcast::widgets {

namespace {
constexpr int kRulerHeight     = 28;
constexpr int kTrackHeight     = 80;
constexpr int kMarkerSize      = 8;
constexpr float kMinZoom       = 0.001f;
constexpr float kMaxZoom       = 100.0f;
constexpr float kZoomFactor    = 1.15f;

// Alternating track lane colors — subtle dark gray matching web version
const QColor kTrackEven(38, 42, 56);
const QColor kTrackOdd(42, 46, 62);
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

// Loop region colors
const QColor kLoopRegionFill(0, 180, 160, 30);     // teal, semi-transparent
const QColor kLoopBracketColor(0, 200, 180);        // teal bracket color
constexpr int kLoopBracketWidth = 6;                // px width of loop start/end brackets
constexpr int kMarkerFlagHeight = 12;               // px height of marker flag triangle
constexpr int kMarkerHitRadius  = 8;                // px hit-test radius for marker flags

// Clip gain envelope colors and sizes
const QColor kGainEnvelopeColor(255, 210, 0);       // gold/yellow
const QColor kGainEnvelopeFill(255, 210, 0, 40);    // semi-transparent fill
constexpr int kGainPointSize = 6;                    // 6x6 diamond handles
constexpr int kGainPointHitRadius = 8;               // px hit-test radius
constexpr float kGainEnvelopeMaxDb  = 12.0f;         // top of envelope range
constexpr float kGainEnvelopeMinDb  = -60.0f;        // bottom of envelope range

// Freeze indicator
const QColor kFreezeColor(120, 180, 255);            // icy blue

// Time selection
const QColor kTimeSelectionFill(80, 140, 255, 50);     // semi-transparent blue
const QColor kTimeSelectionBorder(80, 140, 255, 180);  // blue border
const QColor kTimeSelectionRuler(100, 170, 255, 90);   // ruler highlight
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
        // Repaint when markers or loop region change
        connect(m_timeline, &Timeline::markersChanged,
                this, [this]() { update(); });
        connect(m_timeline, &Timeline::loopChanged,
                this, [this]() { update(); });
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

    // Background — slightly darker than track headers
    p.fillRect(rect(), QColor(30, 34, 48));

    // --- Time ruler ---
    drawRuler(p, w);

    // --- Snap grid (drawn behind clips) ---
    if (m_snapMode != SnapOff)
        drawSnapGrid(p, w);

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

        // Lane separator line — subtle dark gray
        p.setPen(QPen(QColor(42, 46, 62), 1));
        p.drawLine(0, yTop + kTrackHeight - 1, w, yTop + kTrackHeight - 1);

        // Track name is now rendered by TrackHeaderPanel (left of timeline).
        // Resolve track pointers for clip and automation drawing below.
        QObject* trackObj = m_timeline->track(t);
        auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
        auto* videoTrack = qobject_cast<VideoTrack*>(trackObj);
        auto* midiTrack  = qobject_cast<MidiTrack*>(trackObj);

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

        // Draw freeze indicator if the track's DSP chain is bypassed (frozen)
        if (audioTrack) {
            drawFreezeIndicator(p, audioTrack, yTop);
        }
    }

    // --- Time selection region ---
    drawSelection(p, w, h);

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

    // --- Punch-In / Punch-Out markers ---
    drawPunchMarkers(p, h);

    // --- Loop region ---
    drawLoopRegion(p, w, h);

    // --- Markers ---
    drawMarkers(p, w, h);

    // --- Ripple mode indicator ---
    drawRippleIndicator(p, w);

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

void TimelineWidget::drawPunchMarkers(QPainter& painter, int viewHeight)
{
    if (!m_timeline) return;

    const int w = width();
    static const QColor kPunchInColor(60, 200, 60);     // green
    static const QColor kPunchOutColor(220, 60, 60);    // red
    static const QColor kPunchRegionFill(60, 200, 60, 25);

    // Draw the punch region (shaded area between punch-in and punch-out)
    if (m_timeline->punchInEnabled() && m_timeline->punchOutEnabled()) {
        int piX = sampleToPixel(m_timeline->punchIn(), m_scroll, m_zoom);
        int poX = sampleToPixel(m_timeline->punchOut(), m_scroll, m_zoom);
        if (piX < w && poX > 0 && poX > piX) {
            painter.fillRect(piX, kRulerHeight, poX - piX, viewHeight - kRulerHeight,
                             kPunchRegionFill);
        }
    }

    // Punch-In marker (green vertical line + "I" flag)
    if (m_timeline->punchInEnabled()) {
        int piX = sampleToPixel(m_timeline->punchIn(), m_scroll, m_zoom);
        if (piX >= 0 && piX <= w) {
            painter.setPen(QPen(kPunchInColor, 2, Qt::DashLine));
            painter.drawLine(piX, kRulerHeight, piX, viewHeight);

            // Flag at the top of the ruler
            QPainterPath flag;
            flag.moveTo(piX, 2);
            flag.lineTo(piX + 16, 2);
            flag.lineTo(piX + 16, 14);
            flag.lineTo(piX, 14);
            flag.closeSubpath();
            painter.setBrush(kPunchInColor);
            painter.setPen(Qt::NoPen);
            painter.drawPath(flag);

            painter.setPen(Qt::white);
            QFont flagFont = font();
            flagFont.setPointSize(8);
            flagFont.setBold(true);
            painter.setFont(flagFont);
            painter.drawText(piX + 4, 12, QStringLiteral("I"));
        }
    }

    // Punch-Out marker (red vertical line + "O" flag)
    if (m_timeline->punchOutEnabled()) {
        int poX = sampleToPixel(m_timeline->punchOut(), m_scroll, m_zoom);
        if (poX >= 0 && poX <= w) {
            painter.setPen(QPen(kPunchOutColor, 2, Qt::DashLine));
            painter.drawLine(poX, kRulerHeight, poX, viewHeight);

            // Flag at the top of the ruler
            QPainterPath flag;
            flag.moveTo(poX - 16, 2);
            flag.lineTo(poX, 2);
            flag.lineTo(poX, 14);
            flag.lineTo(poX - 16, 14);
            flag.closeSubpath();
            painter.setBrush(kPunchOutColor);
            painter.setPen(Qt::NoPen);
            painter.drawPath(flag);

            painter.setPen(Qt::white);
            QFont flagFont = font();
            flagFont.setPointSize(8);
            flagFont.setBold(true);
            painter.setFont(flagFont);
            painter.drawText(poX - 12, 12, QStringLiteral("O"));
        }
    }
}

// ── Marker flags on the ruler ──────────────────────────────────────────────

void TimelineWidget::drawMarkers(QPainter& painter, int viewWidth, int viewHeight)
{
    if (!m_timeline) return;

    QFont markerFont = font();
    markerFont.setPointSize(7);
    QFontMetrics fm(markerFont);

    for (int i = 0; i < m_timeline->markerCount(); ++i) {
        const Marker& mkr = m_timeline->marker(i);
        int px = sampleToPixel(mkr.position(), m_scroll, m_zoom);
        if (px < -20 || px > viewWidth + 20) continue;

        QColor color = mkr.color();

        // Draw vertical line through the track area (thinner, semi-transparent)
        QColor lineColor = color;
        lineColor.setAlpha(80);
        painter.setPen(QPen(lineColor, 1, Qt::DotLine));
        painter.drawLine(px, kRulerHeight, px, viewHeight);

        // Draw triangular flag at the top of the ruler
        QPainterPath flag;
        flag.moveTo(px, 2);
        flag.lineTo(px + kMarkerFlagHeight, 2);
        flag.lineTo(px + kMarkerFlagHeight, 2 + kMarkerFlagHeight);
        flag.lineTo(px, 2 + kMarkerFlagHeight / 2);
        flag.closeSubpath();

        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPath(flag);

        // Draw marker name next to the flag (small font, truncated)
        painter.setFont(markerFont);
        painter.setPen(QColor(220, 220, 220));
        QString label = mkr.name();
        int maxTextWidth = 60;
        if (fm.horizontalAdvance(label) > maxTextWidth) {
            label = fm.elidedText(label, Qt::ElideRight, maxTextWidth);
        }
        painter.drawText(px + kMarkerFlagHeight + 2, 11, label);
    }
}

// ── Loop region ────────────────────────────────────────────────────────────

void TimelineWidget::drawLoopRegion(QPainter& painter, int viewWidth, int viewHeight)
{
    if (!m_timeline || !m_timeline->loopEnabled()) return;

    int64_t loopStart = m_timeline->loopStart();
    int64_t loopEnd   = m_timeline->loopEnd();
    if (loopEnd <= loopStart) return;

    int lsX = sampleToPixel(loopStart, m_scroll, m_zoom);
    int leX = sampleToPixel(loopEnd, m_scroll, m_zoom);

    // Clamp to visible area
    if (leX < 0 || lsX > viewWidth) return;

    // Semi-transparent teal fill spanning the loop region (ruler + tracks)
    painter.fillRect(qMax(0, lsX), 0, qMin(viewWidth, leX) - qMax(0, lsX), viewHeight,
                     kLoopRegionFill);

    // Loop start bracket (left bracket)
    if (lsX >= 0 && lsX <= viewWidth) {
        painter.setPen(QPen(kLoopBracketColor, 2));
        painter.drawLine(lsX, 0, lsX, viewHeight);

        // Bracket cap at top
        painter.drawLine(lsX, 0, lsX + kLoopBracketWidth, 0);
        painter.drawLine(lsX, kRulerHeight - 1, lsX + kLoopBracketWidth, kRulerHeight - 1);

        // Small "L" label
        QFont bracketFont = font();
        bracketFont.setPointSize(7);
        bracketFont.setBold(true);
        painter.setFont(bracketFont);
        painter.setPen(kLoopBracketColor);
        painter.drawText(lsX + 2, kRulerHeight - 4, QStringLiteral("L"));
    }

    // Loop end bracket (right bracket)
    if (leX >= 0 && leX <= viewWidth) {
        painter.setPen(QPen(kLoopBracketColor, 2));
        painter.drawLine(leX, 0, leX, viewHeight);

        // Bracket cap at top
        painter.drawLine(leX - kLoopBracketWidth, 0, leX, 0);
        painter.drawLine(leX - kLoopBracketWidth, kRulerHeight - 1, leX, kRulerHeight - 1);

        // Small "L" label
        QFont bracketFont = font();
        bracketFont.setPointSize(7);
        bracketFont.setBold(true);
        painter.setFont(bracketFont);
        painter.setPen(kLoopBracketColor);
        painter.drawText(leX - 8, kRulerHeight - 4, QStringLiteral("L"));
    }
}

// ── Clip drawing ───────────────────────────────────────────────────────────

void TimelineWidget::drawClip(QPainter& p, Clip* clip, int trackIndex, int clipIndex, int yTop)
{
    int x1 = sampleToPixel(clip->timelinePosition(), m_scroll, m_zoom);
    int x2 = sampleToPixel(clip->endPosition(), m_scroll, m_zoom);
    if (x2 < 0 || x1 > width()) return;

    int clipY = yTop + 4;
    int clipH = kTrackHeight - 8;
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
        float vZoom = verticalZoomForTrack(trackIndex);
        drawWaveform(p, clip, baseColor, x1, clipY, clipW, clipH, vZoom);
    }

    // Gain envelope overlay (rubber-band line + handles)
    if (!clip->gainEnvelope().isEmpty() && clipW > 10) {
        drawGainEnvelope(p, clip, x1, clipY, clipW, clipH);
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

    int clipY = yTop + 4;
    int clipH = kTrackHeight - 8;
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
    m_dragging = false;  // Only set true when we confirm a drag target
    m_draggingAuto = false;
    m_draggingAutoPoint = -1;
    m_draggingGainPoint = false;
    m_gainPointIndex = -1;
    m_gainPointClip = nullptr;
    m_draggingMarkerIdx = -1;
    m_draggingLoopStart = false;
    m_draggingLoopEnd   = false;
    m_dragStart = event->pos();

    // Click in ruler area: check markers, loop brackets, then set playhead
    if (event->pos().y() < kRulerHeight && m_timeline) {
        int mouseX = event->pos().x();

        // Hit-test loop brackets first (if loop is enabled)
        if (m_timeline->loopEnabled()) {
            int lsX = sampleToPixel(m_timeline->loopStart(), m_scroll, m_zoom);
            int leX = sampleToPixel(m_timeline->loopEnd(), m_scroll, m_zoom);

            if (qAbs(mouseX - lsX) <= kMarkerHitRadius) {
                m_draggingLoopStart = true;
                return;
            }
            if (qAbs(mouseX - leX) <= kMarkerHitRadius) {
                m_draggingLoopEnd = true;
                return;
            }

            // Double-click on the loop bar to toggle looping
            if (event->type() == QEvent::MouseButtonDblClick
                && mouseX > lsX && mouseX < leX) {
                m_timeline->setLoopEnabled(!m_timeline->loopEnabled());
                update();
                return;
            }
        }

        // Hit-test marker flags
        for (int i = 0; i < m_timeline->markerCount(); ++i) {
            int mkrX = sampleToPixel(m_timeline->marker(i).position(), m_scroll, m_zoom);
            if (qAbs(mouseX - mkrX) <= kMarkerHitRadius) {
                if (event->type() == QEvent::MouseButtonDblClick) {
                    // Double-click: open edit dialog
                    Marker mkr = m_timeline->marker(i);
                    bool ok = false;
                    QString name = QInputDialog::getText(
                        this, tr("Edit Marker"), tr("Name:"),
                        QLineEdit::Normal, mkr.name(), &ok);
                    if (ok && !name.isEmpty()) {
                        mkr.setName(name);
                        m_timeline->setMarker(i, mkr);
                    }
                    update();
                    return;
                }
                // Single click: start dragging the marker
                m_draggingMarkerIdx = i;
                return;
            }
        }

        // Shift+click in ruler: start/extend a time selection
        if ((event->modifiers() & Qt::ShiftModifier) && event->button() == Qt::LeftButton) {
            int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            sample = qMax(int64_t(0), sample);
            if (hasSelection()) {
                // Extend the existing selection
                if (sample < m_selectionStart)
                    setSelection(sample, m_selectionEnd);
                else
                    setSelection(m_selectionStart, sample);
            } else {
                // Start a new selection from the playhead to here
                int64_t ph = m_timeline->playhead();
                setSelection(qMin(ph, sample), qMax(ph, sample));
            }
            m_selectingRegion = true;
            m_dragging = true;
            return;
        }

        // Ctrl+click (or Cmd+click on macOS) in ruler: start a region selection drag
        if ((event->modifiers() & Qt::ControlModifier) && event->button() == Qt::LeftButton) {
            int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            sample = qMax(int64_t(0), sample);
            m_selectionStart = sample;
            m_selectionEnd = sample;
            m_selectingRegion = true;
            m_dragging = true;
            return;
        }

        // Default: set playhead (drag will be handled in mouseMoveEvent)
        clearSelection();
        m_dragging = true;  // Enable ruler drag for playhead scrubbing
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

            // ── Slip Editing (Cmd+Alt drag inside a clip) ───────────
            if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
                == (Qt::ControlModifier | Qt::AltModifier)
                && audioTrack && event->button() == Qt::LeftButton) {
                int64_t clickSampleSlip = pixelToSample(event->pos().x(), m_scroll, m_zoom);
                for (int c = 0; c < audioTrack->clipCount(); ++c) {
                    Clip* clip = audioTrack->clip(c);
                    if (!clip) continue;
                    if (clickSampleSlip >= clip->timelinePosition()
                        && clickSampleSlip < clip->endPosition()) {
                        m_slipEditing = true;
                        m_slipClip = clip;
                        m_slipTrackIndex = trackIdx;
                        m_slipStartSourceIn = clip->sourceIn();
                        m_slipDragOrigin = event->pos();
                        setCursor(Qt::SizeHorCursor);
                        update();
                        return;
                    }
                }
            }

            // ── Gain Envelope Editing (Alt+click on clip) ───────────
            if ((event->modifiers() & Qt::AltModifier) && audioTrack) {
                int64_t clickSampleGE = pixelToSample(event->pos().x(), m_scroll, m_zoom);
                for (int c = 0; c < audioTrack->clipCount(); ++c) {
                    Clip* clip = audioTrack->clip(c);
                    if (!clip) continue;
                    if (clickSampleGE < clip->timelinePosition() || clickSampleGE >= clip->endPosition())
                        continue;

                    // Compute clip pixel geometry for hit-testing
                    int cx1 = sampleToPixel(clip->timelinePosition(), m_scroll, m_zoom);
                    int cx2 = sampleToPixel(clip->endPosition(), m_scroll, m_zoom);
                    int cY = kRulerHeight + trackIdx * kTrackHeight + 4;
                    int cH = kTrackHeight - 8;
                    int cW = qMax(4, cx2 - cx1);

                    // Check if Alt+right-click on an existing gain point -> delete
                    int gpIdx = hitTestGainPoint(clip, cx1, cW, cY, cH,
                                                  event->pos().x(), event->pos().y());
                    if (gpIdx >= 0 && event->button() == Qt::RightButton) {
                        clip->removeGainPoint(gpIdx);
                        emit gainEnvelopeChanged(trackIdx, c);
                        m_dragging = false;
                        update();
                        return;
                    }

                    // Alt+left-click on existing point -> start dragging
                    if (gpIdx >= 0 && event->button() == Qt::LeftButton) {
                        m_draggingGainPoint = true;
                        m_gainPointIndex = gpIdx;
                        m_gainPointClip = clip;
                        m_gainPointTrack = trackIdx;
                        m_gainPointClipIndex = c;
                        update();
                        return;
                    }

                    // Alt+left-click on clip (not on a point) -> add new gain point
                    if (event->button() == Qt::LeftButton) {
                        int64_t offsetSamples = clickSampleGE - clip->timelinePosition();
                        // Convert Y to dB
                        float frac = 1.0f - static_cast<float>(event->pos().y() - cY)
                                          / static_cast<float>(cH);
                        frac = std::clamp(frac, 0.0f, 1.0f);
                        float db = kGainEnvelopeMinDb + frac * (kGainEnvelopeMaxDb - kGainEnvelopeMinDb);
                        clip->addGainPoint(offsetSamples, db);
                        emit gainEnvelopeChanged(trackIdx, c);
                        update();
                        return;
                    }
                    break;
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
            if (found) {
                m_dragging = true;  // Enable clip drag
            } else if (!(event->modifiers() & Qt::ControlModifier)) {
                m_selectedClips.clear();
            }
        }
    }

    update();
    QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    // ── Slip editing drag ────────────────────────────────────────────
    if (m_slipEditing && m_slipClip) {
        int dx = event->pos().x() - m_slipDragOrigin.x();
        // Convert pixel delta to sample delta
        int64_t sampleDelta = static_cast<int64_t>(dx / static_cast<double>(m_zoom));
        int64_t clipDuration = m_slipClip->duration();

        // Calculate new sourceIn: shift source window by the drag delta
        int64_t newSourceIn = m_slipStartSourceIn - sampleDelta;
        if (newSourceIn < 0) newSourceIn = 0;

        // Keep the clip's visible duration the same
        int64_t newSourceOut = newSourceIn + clipDuration;

        m_slipClip->setSourceIn(newSourceIn);
        m_slipClip->setSourceOut(newSourceOut);

        update();
        return;
    }

    if (m_dragging) {
        // Gain envelope point drag
        if (m_draggingGainPoint && m_gainPointClip && m_gainPointIndex >= 0) {
            Clip* clip = m_gainPointClip;
            int64_t clickSample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            int64_t newOffset = clickSample - clip->timelinePosition();
            newOffset = std::clamp(newOffset, int64_t(0), clip->duration());

            int cY = kRulerHeight + m_gainPointTrack * kTrackHeight + 4;
            int cH = kTrackHeight - 8;
            float frac = 1.0f - static_cast<float>(event->pos().y() - cY)
                              / static_cast<float>(cH);
            frac = std::clamp(frac, 0.0f, 1.0f);
            float db = kGainEnvelopeMinDb + frac * (kGainEnvelopeMaxDb - kGainEnvelopeMinDb);

            clip->moveGainPoint(m_gainPointIndex, newOffset, db);

            // Find the new index after re-sorting
            const auto& env = clip->gainEnvelope();
            for (int i = 0; i < env.size(); ++i) {
                if (env[i].offsetSamples == newOffset && qFuzzyCompare(env[i].gainDb, db)) {
                    m_gainPointIndex = i;
                    break;
                }
            }

            // Show dB tooltip while dragging
            QString tip = QStringLiteral("%1 dB").arg(db, 0, 'f', 1);
            QToolTip::showText(event->globalPos(), tip, this);

            update();
            return;
        }

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
        // Dragging a marker flag
        else if (m_draggingMarkerIdx >= 0 && m_timeline) {
            int64_t newPos = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            newPos = qMax(int64_t(0), newPos);
            Marker mkr = m_timeline->marker(m_draggingMarkerIdx);
            mkr.setPosition(newPos);
            m_timeline->setMarker(m_draggingMarkerIdx, mkr);
            update();
        }
        // Dragging loop start bracket
        else if (m_draggingLoopStart && m_timeline) {
            int64_t newPos = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            newPos = qMax(int64_t(0), newPos);
            if (newPos < m_timeline->loopEnd()) {
                m_timeline->setLoopStart(newPos);
            }
            update();
        }
        // Dragging loop end bracket
        else if (m_draggingLoopEnd && m_timeline) {
            int64_t newPos = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            newPos = qMax(int64_t(0), newPos);
            if (newPos > m_timeline->loopStart()) {
                m_timeline->setLoopEnd(newPos);
            }
            update();
        }
        // Selection region drag in ruler
        else if (m_selectingRegion && m_dragStart.y() < kRulerHeight && m_timeline) {
            int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            sample = qMax(int64_t(0), sample);
            // Determine selection bounds based on drag start
            int64_t dragStartSample = pixelToSample(m_dragStart.x(), m_scroll, m_zoom);
            dragStartSample = qMax(int64_t(0), dragStartSample);
            setSelection(qMin(dragStartSample, sample), qMax(dragStartSample, sample));
        }
        // Dragging in ruler: update playhead
        else if (m_dragStart.y() < kRulerHeight && m_timeline) {
            int64_t sample = pixelToSample(event->pos().x(), m_scroll, m_zoom);
            m_timeline->setPlayhead(qMax(int64_t(0), sample));
            emit playheadMoved(m_timeline->playhead());
            update();
        } else if (!m_selectedClips.isEmpty() && m_timeline) {
            // Clip drag: move clips by pixel delta
            int64_t deltaSamples = pixelToSample(event->pos().x(), 0, m_zoom)
                                 - pixelToSample(m_dragStart.x(), 0, m_zoom);
            if (deltaSamples != 0) {
                // Find the track and move each selected clip
                int trackIdx = (m_dragStart.y() - kRulerHeight) / kTrackHeight;
                if (trackIdx >= 0 && trackIdx < m_timeline->trackCount()) {
                    auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(trackIdx));
                    if (audioTrack) {
                        for (int clipId : std::as_const(m_selectedClips)) {
                            Clip* clip = audioTrack->clip(clipId);
                            if (clip) {
                                int64_t newPos = clip->timelinePosition() + deltaSamples;
                                if (newPos < 0) newPos = 0;
                                newPos = snapPosition(newPos);
                                clip->setTimelinePosition(newPos);
                            }
                        }
                    }
                }
                emit clipMoved(m_selectedClips.isEmpty() ? -1 : m_selectedClips.first(), deltaSamples);
                m_dragStart = event->pos();
                update();
            }
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
    // If we were dragging a marker, re-sort markers
    if (m_draggingMarkerIdx >= 0 && m_timeline) {
        m_timeline->sortMarkers();
    }

    // End slip editing
    if (m_slipEditing) {
        m_slipEditing = false;
        m_slipClip = nullptr;
        m_slipTrackIndex = -1;
        setCursor(Qt::ArrowCursor);
    }

    m_dragging = false;
    m_draggingAuto = false;
    m_draggingAutoPoint = -1;
    m_draggingGainPoint = false;
    m_gainPointIndex = -1;
    m_gainPointClip = nullptr;
    m_gainPointTrack = -1;
    m_gainPointClipIndex = -1;
    m_draggingMarkerIdx = -1;
    m_draggingLoopStart = false;
    m_draggingLoopEnd   = false;
    m_selectingRegion = false;
    m_rubberBand = QRect();
    update();
    QWidget::mouseReleaseEvent(event);
}

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    // Per-track vertical waveform zoom: Ctrl+Alt+Wheel
    if ((event->modifiers() & Qt::ControlModifier)
        && (event->modifiers() & Qt::AltModifier)
        && m_timeline) {
        int mouseY = static_cast<int>(event->position().y());
        if (mouseY >= kRulerHeight) {
            int trackIdx = (mouseY - kRulerHeight) / kTrackHeight;
            if (trackIdx >= 0 && trackIdx < m_timeline->trackCount()) {
                float current = m_trackVerticalZoom.value(trackIdx, 1.0f);
                if (event->angleDelta().y() > 0)
                    current = std::min(current * 1.15f, 4.0f);
                else
                    current = std::max(current / 1.15f, 0.25f);
                m_trackVerticalZoom[trackIdx] = current;
                update();
                event->accept();
                return;
            }
        }
    }

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

    const bool inRuler = event->pos().y() < kRulerHeight;
    const bool hasClipSelection = !m_selectedClips.isEmpty();
    const bool hasTimeSelection = hasSelection();
    int64_t clickSamplePos = pixelToSample(event->pos().x(), m_scroll, m_zoom);
    clickSamplePos = qMax(int64_t(0), clickSamplePos);

    // ════════════════════════════════════════════════════════════════════
    // RULER right-click menu
    // ════════════════════════════════════════════════════════════════════
    if (inRuler && m_timeline) {
        menu.addAction(tr("Set Playhead Here"), this, [this, clickSamplePos]() {
            m_timeline->setPlayhead(clickSamplePos);
            emit playheadMoved(clickSamplePos);
            update();
        });
        menu.addSeparator();

        menu.addAction(tr("Add Marker Here"), this, [this, clickSamplePos]() {
            bool ok = false;
            QString name = QInputDialog::getText(
                this, tr("Add Marker"), tr("Marker name:"),
                QLineEdit::Normal,
                QStringLiteral("Marker %1").arg(m_timeline->markerCount() + 1),
                &ok);
            if (ok && !name.isEmpty()) {
                Marker mkr(name, clickSamplePos, Marker::Type::Cue);
                m_timeline->addMarker(mkr);
                update();
            }
        });

        menu.addSeparator();

        menu.addAction(tr("Set Loop Start Here"), this, [this, clickSamplePos]() {
            m_timeline->setLoopStart(clickSamplePos);
            if (m_timeline->loopEnd() <= clickSamplePos)
                m_timeline->setLoopEnd(m_timeline->duration());
            m_timeline->setLoopEnabled(true);
            update();
        });
        menu.addAction(tr("Set Loop End Here"), this, [this, clickSamplePos]() {
            m_timeline->setLoopEnd(clickSamplePos);
            if (m_timeline->loopStart() >= clickSamplePos)
                m_timeline->setLoopStart(0);
            m_timeline->setLoopEnabled(true);
            update();
        });

        if (m_timeline->loopEnabled()) {
            menu.addAction(tr("Clear Loop Region"), this, [this]() {
                m_timeline->setLoopEnabled(false);
                update();
            });
        }

        menu.addSeparator();

        menu.addAction(tr("Set Punch In Here"), this, [this, clickSamplePos]() {
            m_timeline->setPunchIn(clickSamplePos);
            m_timeline->setPunchInEnabled(true);
            update();
        });
        menu.addAction(tr("Set Punch Out Here"), this, [this, clickSamplePos]() {
            m_timeline->setPunchOut(clickSamplePos);
            m_timeline->setPunchOutEnabled(true);
            update();
        });

        if (m_timeline->punchInEnabled() || m_timeline->punchOutEnabled()) {
            menu.addAction(tr("Clear Punch Markers"), this, [this]() {
                m_timeline->setPunchInEnabled(false);
                m_timeline->setPunchOutEnabled(false);
                update();
            });
        }

        menu.addSeparator();

        menu.addAction(tr("Set Selection Start Here"), this, [this, clickSamplePos]() {
            setSelection(clickSamplePos, m_selectionEnd > clickSamplePos ? m_selectionEnd : clickSamplePos);
        });
        menu.addAction(tr("Set Selection End Here"), this, [this, clickSamplePos]() {
            setSelection(m_selectionStart < clickSamplePos ? m_selectionStart : clickSamplePos, clickSamplePos);
        });

        if (hasTimeSelection) {
            menu.addAction(tr("Clear Selection"), this, [this]() {
                clearSelection();
            });
            menu.addAction(tr("Zoom to Selection"), this, [this]() {
                if (m_selectionEnd > m_selectionStart) {
                    int64_t range = m_selectionEnd - m_selectionStart;
                    int64_t padding = range / 20;
                    m_scroll = qMax(int64_t(0), m_selectionStart - padding);
                    m_zoom = static_cast<float>(width()) / static_cast<float>(range + padding * 2);
                    m_zoom = qBound(kMinZoom, m_zoom, kMaxZoom);
                    update();
                }
            });
        }

        // Right-click on an existing marker: offer edit/delete
        for (int i = 0; i < m_timeline->markerCount(); ++i) {
            int mkrX = sampleToPixel(m_timeline->marker(i).position(), m_scroll, m_zoom);
            if (qAbs(event->pos().x() - mkrX) <= kMarkerHitRadius) {
                const int markerIdx = i;
                menu.addSeparator();
                menu.addAction(tr("Edit Marker \"%1\"...").arg(m_timeline->marker(i).name()),
                               this, [this, markerIdx]() {
                    Marker mkr = m_timeline->marker(markerIdx);
                    bool ok = false;
                    QString name = QInputDialog::getText(
                        this, tr("Edit Marker"), tr("Name:"),
                        QLineEdit::Normal, mkr.name(), &ok);
                    if (ok && !name.isEmpty()) {
                        mkr.setName(name);
                        m_timeline->setMarker(markerIdx, mkr);
                        update();
                    }
                });
                menu.addAction(tr("Change Marker Color..."), this, [this, markerIdx]() {
                    Marker mkr = m_timeline->marker(markerIdx);
                    QColor color = QColorDialog::getColor(mkr.color(), this, tr("Marker Color"));
                    if (color.isValid()) {
                        mkr.setColor(color);
                        m_timeline->setMarker(markerIdx, mkr);
                        update();
                    }
                });
                menu.addAction(tr("Delete Marker"), this, [this, markerIdx]() {
                    m_timeline->removeMarker(markerIdx);
                    update();
                });
                break;
            }
        }

        menu.exec(event->globalPos());
        return;
    }

    // ════════════════════════════════════════════════════════════���═══════
    // TIME SELECTION right-click menu (when a time region is selected)
    // ════════════════════════════════════════════════════════════════════
    if (hasTimeSelection && !hasClipSelection) {
        menu.addAction(tr("Apply Effect to Selection..."), this, [this]() {
            // Placeholder -- would open an effect chooser dialog
        });
        menu.addAction(tr("Export Selection..."), this, [this]() {
            // Placeholder -- would export the selected time range
        });
        menu.addSeparator();
        menu.addAction(tr("Silence Selection"), this, [this]() {
            // Placeholder -- would silence audio in the selection range
        });
        menu.addAction(tr("Normalize Selection"), this, [this]() {
            normalizeSelectedClips();
        });
        menu.addSeparator();
        menu.addAction(tr("Fade In"), this, [this]() {
            // Placeholder -- would apply a fade-in to the selection
        });
        menu.addAction(tr("Fade Out"), this, [this]() {
            // Placeholder -- would apply a fade-out to the selection
        });
        menu.addSeparator();
        menu.addAction(tr("Loop Selection"), this, [this]() {
            if (m_timeline) {
                m_timeline->setLoopStart(m_selectionStart);
                m_timeline->setLoopEnd(m_selectionEnd);
                m_timeline->setLoopEnabled(true);
                update();
            }
        });
        menu.addAction(tr("Zoom to Selection"), this, [this]() {
            if (m_selectionEnd > m_selectionStart) {
                int64_t range = m_selectionEnd - m_selectionStart;
                int64_t padding = range / 20;
                m_scroll = qMax(int64_t(0), m_selectionStart - padding);
                m_zoom = static_cast<float>(width()) / static_cast<float>(range + padding * 2);
                m_zoom = qBound(kMinZoom, m_zoom, kMaxZoom);
                update();
            }
        });
        menu.addSeparator();
        menu.addAction(tr("Clear Selection"), this, [this]() {
            clearSelection();
        });
        menu.exec(event->globalPos());
        return;
    }

    // ════════════════════════════════════════════════════════════════════
    // CLIP right-click menu (when clips are selected)
    // ════════════════════════════════════════════════════════════════════
    if (hasClipSelection) {
        auto* actCut = menu.addAction(tr("Cut"));
        actCut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
        connect(actCut, &QAction::triggered, this, [this]{ cutSelectedClips(); });

        auto* actCopy = menu.addAction(tr("Copy"));
        actCopy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
        connect(actCopy, &QAction::triggered, this, [this]{ copySelectedClips(); });

        auto* actDelete = menu.addAction(tr("Delete"));
        actDelete->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(actDelete, &QAction::triggered, this, [this]{ deleteSelectedClips(); });

        menu.addSeparator();

        auto* actSplit = menu.addAction(tr("Split at Playhead"));
        actSplit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
        connect(actSplit, &QAction::triggered, this, [this]{ splitAtPlayhead(); });

        menu.addSeparator();

        menu.addAction(tr("Normalize..."), this, [this]{ normalizeSelectedClips(); });

        menu.addAction(tr("Fade In"), this, [this]() {
            // Apply default fade-in to selected clips
            if (!m_timeline) return;
            int sr = m_timeline->sampleRate();
            int trackCount = m_timeline->trackCount();
            for (int t = 0; t < trackCount; ++t) {
                auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
                if (!audioTrack) continue;
                for (int c : std::as_const(m_selectedClips)) {
                    Clip* clip = audioTrack->clip(c);
                    if (clip) clip->setFadeIn(sr / 4);  // 0.25 sec default
                }
            }
            update();
        });
        menu.addAction(tr("Fade Out"), this, [this]() {
            if (!m_timeline) return;
            int sr = m_timeline->sampleRate();
            int trackCount = m_timeline->trackCount();
            for (int t = 0; t < trackCount; ++t) {
                auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
                if (!audioTrack) continue;
                for (int c : std::as_const(m_selectedClips)) {
                    Clip* clip = audioTrack->clip(c);
                    if (clip) clip->setFadeOut(sr / 4);  // 0.25 sec default
                }
            }
            update();
        });

        menu.addSeparator();

        menu.addAction(tr("Time Stretch / Pitch Shift..."), this, [this] {
            auto* dialog = new TimeStretchDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &QDialog::accepted, this, [this, dialog] {
                Q_UNUSED(dialog);
                update();
            });
            dialog->show();
        });

        menu.addAction(tr("Reverse"), this, [this]() {
            // Placeholder -- would reverse the clip audio
        });

        menu.addSeparator();

        menu.addAction(tr("Bounce to New Track"), this, [this]() {
            // Placeholder -- would bounce selected clips to a new track
        });

        menu.addAction(tr("Open in Editor Studio"), this, [this]() {
            // Placeholder -- would launch dawcast-editor-studio with this clip
        });

        menu.addSeparator();

        menu.addAction(tr("Properties..."), this, [this]() {
            // Placeholder -- would show clip properties dialog
        });

        menu.exec(event->globalPos());
        return;
    }

    // ════════════════════════════════════════════════════════════════════
    // EMPTY TRACK AREA right-click menu (no selection)
    // ════════════════════════════════════════════════════════════════════
    if (m_timeline) {
        menu.addAction(tr("Add Audio Track"), this, [this]() {
            if (m_timeline) m_timeline->addAudioTrack();
            update();
        });
        menu.addAction(tr("Add Video Track"), this, [this]() {
            if (m_timeline) m_timeline->addVideoTrack();
            update();
        });
        menu.addAction(tr("Add MIDI Track"), this, [this]() {
            if (m_timeline) m_timeline->addMidiTrack();
            update();
        });
        menu.addSeparator();

        menu.addAction(tr("Paste"), this, [this]{ pasteClips(); });
        menu.addSeparator();

        menu.addAction(tr("Split at Playhead"), this, [this]{ splitAtPlayhead(); });
        menu.addSeparator();
    }

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

void TimelineWidget::drawWaveform(QPainter& p, Clip* clip, const QColor& baseColor,
                                  int clipX, int clipY, int clipW, int clipH,
                                  float verticalZoom)
{
    auto* cache = WaveformCache::instance();

    // Check if waveform is cached WITHOUT triggering a decode
    // (never do I/O or heavy work inside paintEvent)
    if (!cache->hasWaveform(clip->sourcePath())) {
        // Request async decode (no-op if already pending)
        cache->requestWaveform(clip->sourcePath());

        // Draw a simple "Loading..." placeholder
        p.setPen(QColor(180, 180, 180, 120));
        QFont loadFont = font();
        loadFont.setPointSize(8);
        p.setFont(loadFont);
        p.drawText(clipX + clipW / 2 - 25, clipY + clipH / 2 + 4,
                   tr("Loading..."));
        return;
    }

    const WaveformData* waveform = cache->getWaveform(clip->sourcePath());
    if (!waveform) return;  // race condition guard

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

    // Cap pixel iteration to visible area to prevent huge loops on zoomed-in clips
    int visibleDrawW = qMin(drawW, width() + 100);
    for (int px = 0; px < visibleDrawW; ++px) {
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

        // Apply per-track vertical zoom (visual amplification only)
        maxPeak *= verticalZoom;
        maxRms  *= verticalZoom;

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

void TimelineWidget::probeStreams(const QString& filePath, bool& hasAudio, bool& hasVideo)
{
    hasAudio = false;
    hasVideo = false;

#ifdef HAVE_AVFORMAT
    AVFormatContext* fmtCtx = nullptr;
    QByteArray pathUtf8 = filePath.toUtf8();
    if (avformat_open_input(&fmtCtx, pathUtf8.constData(), nullptr, nullptr) < 0)
        return;

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return;
    }

    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            hasAudio = true;
        else if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            hasVideo = true;
    }

    avformat_close_input(&fmtCtx);
#else
    // Without FFmpeg, infer from file extension
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    QStringList vidExts = supportedVideoExtensions();
    QStringList audExts = supportedAudioExtensions();

    if (vidExts.contains(ext)) {
        hasVideo = true;
        hasAudio = true;  // Most video files have audio
    } else if (audExts.contains(ext)) {
        hasAudio = true;
    }
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

        // Probe the file to determine if it has audio, video, or both
        bool hasAudio = false;
        bool hasVideo = false;
        probeStreams(filePath, hasAudio, hasVideo);

        // Fallback: if probing didn't detect streams, use extension
        if (!hasAudio && !hasVideo) {
            if (isVideoFile(filePath)) {
                hasVideo = true;
                hasAudio = true;  // assume video files have audio
            } else {
                hasAudio = true;
            }
        }

        // Probe file duration
        int64_t durationSamples = probeDuration(filePath);

        // ── A/V Split: when a video file has both streams, create
        //    a VideoTrack + AudioTrack with linked clips ──
        if (hasVideo && hasAudio) {
            // Create or find video track
            VideoTrack* videoTrack = nullptr;
            int videoTrackIdx = dropTrackIdx;

            if (videoTrackIdx >= 0) {
                videoTrack = qobject_cast<VideoTrack*>(m_timeline->track(videoTrackIdx));
            }
            if (!videoTrack) {
                m_timeline->addVideoTrack();
                videoTrackIdx = m_timeline->trackCount() - 1;
                videoTrack = qobject_cast<VideoTrack*>(m_timeline->track(videoTrackIdx));
            }

            // Create a video clip
            if (videoTrack) {
                auto* vClip = new Clip(videoTrack);
                vClip->setSourcePath(filePath);
                vClip->setSourceIn(0);
                vClip->setSourceOut(durationSamples);
                vClip->setTimelinePosition(cursorPos);
                videoTrack->addClip(vClip);
            }

            // Create a companion audio track for the audio stream
            AudioTrack* audioTrack = m_timeline->addAudioTrack();
            if (audioTrack) {
                QFileInfo fi(filePath);
                audioTrack->setName(fi.baseName() + QStringLiteral(" (Audio)"));

                auto* aClip = new Clip(audioTrack);
                aClip->setSourcePath(filePath);  // FFmpeg decodes audio from video
                aClip->setSourceIn(0);
                aClip->setSourceOut(durationSamples);
                aClip->setTimelinePosition(cursorPos);
                audioTrack->addClip(aClip);

                // Request waveform decode for the audio portion
                WaveformCache::instance()->requestWaveform(filePath);
            }

        } else if (hasVideo) {
            // Video-only: create just a video track
            VideoTrack* videoTrack = nullptr;
            int targetIdx = dropTrackIdx;
            if (targetIdx >= 0) {
                videoTrack = qobject_cast<VideoTrack*>(m_timeline->track(targetIdx));
            }
            if (!videoTrack) {
                m_timeline->addVideoTrack();
                targetIdx = m_timeline->trackCount() - 1;
                videoTrack = qobject_cast<VideoTrack*>(m_timeline->track(targetIdx));
            }
            if (videoTrack) {
                auto* clip = new Clip(videoTrack);
                clip->setSourcePath(filePath);
                clip->setSourceIn(0);
                clip->setSourceOut(durationSamples);
                clip->setTimelinePosition(cursorPos);
                videoTrack->addClip(clip);
            }

        } else {
            // Audio-only: create just an audio track (existing behavior)
            AudioTrack* audioTrack = nullptr;
            int targetIdx = dropTrackIdx;
            if (targetIdx >= 0) {
                audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(targetIdx));
            }
            if (!audioTrack) {
                m_timeline->addAudioTrack();
                targetIdx = m_timeline->trackCount() - 1;
                audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(targetIdx));
            }
            if (audioTrack) {
                auto* clip = new Clip(audioTrack);
                clip->setSourcePath(filePath);
                clip->setSourceIn(0);
                clip->setSourceOut(durationSamples);
                clip->setTimelinePosition(cursorPos);
                audioTrack->addClip(clip);

                WaveformCache::instance()->requestWaveform(filePath);
            }
        }

        // Advance cursor for the next dropped file so they don't overlap
        cursorPos += durationSamples;
    }

    // If this is the first content on the timeline (single track, single clip),
    // snap the clip to position 0 and reset the playhead so the user hears
    // audio immediately on pressing Play.
    if (m_timeline->trackCount() == 1) {
        QObject* firstTrack = m_timeline->track(0);
        auto* at = qobject_cast<AudioTrack*>(firstTrack);
        if (at && at->clipCount() == 1) {
            Clip* firstClip = at->clip(0);
            if (firstClip) {
                firstClip->setTimelinePosition(0);
                m_timeline->setPlayhead(0);
            }
        }
    }

    event->acceptProposedAction();
    update();
}

// ── Snap-to-Grid ─────────────────────────────────────────────────────────────

void TimelineWidget::setSnapMode(SnapMode mode)
{
    if (m_snapMode == mode) return;
    m_snapMode = mode;
    emit snapModeChanged(mode);
    update();
}

int64_t TimelineWidget::snapPosition(int64_t raw) const
{
    if (m_snapMode == SnapOff || !m_timeline)
        return raw;

    int sampleRate = m_timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;
    double bpm = m_timeline->tempo();
    if (bpm <= 0.0) bpm = 120.0;

    int64_t gridInterval = 0;

    switch (m_snapMode) {
    case SnapOff:
        return raw;
    case SnapBeat:
        // One beat = 60/bpm seconds
        gridInterval = static_cast<int64_t>((60.0 / bpm) * sampleRate);
        break;
    case SnapBar:
        // Assume 4/4 time: 1 bar = 4 beats
        gridInterval = static_cast<int64_t>((60.0 / bpm) * 4.0 * sampleRate);
        break;
    case SnapSecond:
        gridInterval = sampleRate;
        break;
    case SnapHalfSecond:
        gridInterval = sampleRate / 2;
        break;
    case SnapFrame:
        // 30 fps video frame grid
        gridInterval = sampleRate / 30;
        break;
    }

    if (gridInterval <= 0)
        return raw;

    // Snap to nearest grid line
    int64_t lower = (raw / gridInterval) * gridInterval;
    int64_t upper = lower + gridInterval;
    return (raw - lower <= upper - raw) ? lower : upper;
}

void TimelineWidget::drawSnapGrid(QPainter& painter, int viewWidth)
{
    if (m_snapMode == SnapOff || !m_timeline)
        return;

    int sampleRate = m_timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;
    double bpm = m_timeline->tempo();
    if (bpm <= 0.0) bpm = 120.0;

    int64_t gridInterval = 0;
    switch (m_snapMode) {
    case SnapOff:     return;
    case SnapBeat:    gridInterval = static_cast<int64_t>((60.0 / bpm) * sampleRate); break;
    case SnapBar:     gridInterval = static_cast<int64_t>((60.0 / bpm) * 4.0 * sampleRate); break;
    case SnapSecond:  gridInterval = sampleRate; break;
    case SnapHalfSecond: gridInterval = sampleRate / 2; break;
    case SnapFrame:   gridInterval = sampleRate / 30; break;
    }

    if (gridInterval <= 0) return;

    int trackCount = m_timeline->trackCount();
    int totalH = kRulerHeight + trackCount * kTrackHeight;
    if (totalH < kRulerHeight) totalH = height();

    // Subtle dotted grid lines
    QPen gridPen(QColor(100, 100, 120, 50), 1, Qt::DotLine);
    painter.setPen(gridPen);

    // First visible grid line
    int64_t firstSample = (m_scroll / gridInterval) * gridInterval;
    if (firstSample < m_scroll) firstSample += gridInterval;

    for (int64_t sample = firstSample; ; sample += gridInterval) {
        int px = sampleToPixel(sample, m_scroll, m_zoom);
        if (px > viewWidth) break;
        if (px >= 0) {
            painter.drawLine(px, kRulerHeight, px, totalH);
        }
    }
}

// ── Clipboard Operations ─────────────────────────────────────────────────────

void TimelineWidget::copySelectedClips()
{
    m_clipboard.clear();

    if (!m_timeline || m_selectedClips.isEmpty())
        return;

    // Gather clip data from all tracks; find the earliest position
    int64_t earliest = INT64_MAX;

    // For now, selected clips are indices within the first track that has them.
    // We scan all tracks to find clips whose index matches the selection list.
    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
        if (!audioTrack) continue;

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            if (!m_selectedClips.contains(c)) continue;
            Clip* clip = audioTrack->clip(c);
            if (!clip) continue;

            if (clip->timelinePosition() < earliest)
                earliest = clip->timelinePosition();
        }
    }

    // Build clipboard entries with positions relative to the earliest clip
    for (int t = 0; t < trackCount; ++t) {
        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
        if (!audioTrack) continue;

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            if (!m_selectedClips.contains(c)) continue;
            Clip* clip = audioTrack->clip(c);
            if (!clip) continue;

            ClipData cd;
            cd.sourcePath = clip->sourcePath();
            cd.sourceIn   = clip->sourceIn();
            cd.sourceOut  = clip->sourceOut();
            cd.gain       = clip->gain();
            cd.fadeIn     = clip->fadeIn();
            cd.fadeOut    = clip->fadeOut();
            cd.trackIndex = t;
            cd.relativePosition = clip->timelinePosition() - earliest;
            m_clipboard.append(cd);
        }
    }
}

void TimelineWidget::cutSelectedClips()
{
    copySelectedClips();
    deleteSelectedClips();
}

void TimelineWidget::pasteClips()
{
    if (!m_timeline || m_clipboard.isEmpty())
        return;

    int64_t playhead = m_timeline->playhead();
    int trackCount = m_timeline->trackCount();

    // In ripple mode, calculate total paste duration to shift clips first
    if (m_rippleMode) {
        // Find the max extent of the pasted content per track
        QHash<int, int64_t> trackPasteDurations;
        for (const ClipData& cd : std::as_const(m_clipboard)) {
            int64_t clipDur = cd.sourceOut - cd.sourceIn;
            int64_t clipEnd = cd.relativePosition + clipDur;
            if (!trackPasteDurations.contains(cd.trackIndex)
                || clipEnd > trackPasteDurations[cd.trackIndex]) {
                trackPasteDurations[cd.trackIndex] = clipEnd;
            }
        }

        for (auto it = trackPasteDurations.begin(); it != trackPasteDurations.end(); ++it) {
            int trackIdx = it.key();
            if (trackIdx < 0 || trackIdx >= trackCount) continue;
            auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(trackIdx));
            if (!audioTrack) continue;
            rippleShift(audioTrack, playhead, it.value());
        }
    }

    for (const ClipData& cd : std::as_const(m_clipboard)) {
        int targetTrack = cd.trackIndex;
        if (targetTrack < 0 || targetTrack >= trackCount) {
            // Create a new audio track if needed
            m_timeline->addAudioTrack();
            targetTrack = m_timeline->trackCount() - 1;
            trackCount = m_timeline->trackCount();
        }

        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(targetTrack));
        if (!audioTrack) continue;

        auto* clip = new Clip(audioTrack);
        clip->setSourcePath(cd.sourcePath);
        clip->setSourceIn(cd.sourceIn);
        clip->setSourceOut(cd.sourceOut);
        clip->setGain(cd.gain);
        clip->setFadeIn(cd.fadeIn);
        clip->setFadeOut(cd.fadeOut);

        int64_t pastePos = playhead + cd.relativePosition;
        if (m_snapMode != SnapOff)
            pastePos = snapPosition(pastePos);
        clip->setTimelinePosition(pastePos);

        audioTrack->addClip(clip);
    }

    update();
}

void TimelineWidget::deleteSelectedClips()
{
    if (!m_timeline || m_selectedClips.isEmpty())
        return;

    // Delete selected clips from all audio tracks
    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
        if (!audioTrack) continue;

        // Collect indices and ripple info before removal
        QList<int> toRemove;
        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            if (m_selectedClips.contains(c))
                toRemove.append(c);
        }

        if (toRemove.isEmpty()) continue;

        // For ripple mode: find the earliest deleted clip position and total gap
        int64_t rippleFrom = INT64_MAX;
        int64_t rippleDelta = 0;
        if (m_rippleMode) {
            for (int idx : toRemove) {
                Clip* clip = audioTrack->clip(idx);
                if (!clip) continue;
                if (clip->timelinePosition() < rippleFrom)
                    rippleFrom = clip->timelinePosition();
                rippleDelta += clip->duration();
            }
        }

        // Remove in reverse order to keep indices valid
        std::sort(toRemove.begin(), toRemove.end(), std::greater<int>());
        for (int idx : toRemove) {
            audioTrack->removeClip(idx);
        }

        // Ripple: shift subsequent clips left to fill the gap
        if (m_rippleMode && rippleFrom < INT64_MAX && rippleDelta > 0) {
            rippleShift(audioTrack, rippleFrom, -rippleDelta);
        }
    }

    m_selectedClips.clear();
    update();
}

// ── Zoom Operations ──────────────────────────────────────────────────────────

void TimelineWidget::zoomIn()
{
    setZoom(m_zoom * kZoomFactor);
}

void TimelineWidget::zoomOut()
{
    setZoom(m_zoom / kZoomFactor);
}

void TimelineWidget::zoomToFit()
{
    if (m_timeline && m_timeline->duration() > 0) {
        m_scroll = 0;
        m_zoom = static_cast<float>(width()) / static_cast<float>(m_timeline->duration());
        m_zoom = qBound(kMinZoom, m_zoom, kMaxZoom);
        update();
    }
}

void TimelineWidget::zoomToSelection()
{
    if (!m_timeline || m_selectedClips.isEmpty())
        return;

    // Find the time range spanned by all selected clips
    int64_t minPos = INT64_MAX;
    int64_t maxEnd = 0;

    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
        if (!audioTrack) continue;

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            if (!m_selectedClips.contains(c)) continue;
            Clip* clip = audioTrack->clip(c);
            if (!clip) continue;

            if (clip->timelinePosition() < minPos)
                minPos = clip->timelinePosition();
            if (clip->endPosition() > maxEnd)
                maxEnd = clip->endPosition();
        }
    }

    if (minPos >= maxEnd) return;

    // Add some padding (5% on each side)
    int64_t range = maxEnd - minPos;
    int64_t padding = range / 20;
    minPos = qMax(int64_t(0), minPos - padding);
    maxEnd += padding;

    m_scroll = minPos;
    m_zoom = static_cast<float>(width()) / static_cast<float>(maxEnd - minPos);
    m_zoom = qBound(kMinZoom, m_zoom, kMaxZoom);
    update();
}

// ── Clip Gain Envelope Drawing ───────────────────────────────────────────────

void TimelineWidget::drawGainEnvelope(QPainter& painter, Clip* clip,
                                      int clipX, int clipY, int clipW, int clipH)
{
    const auto& env = clip->gainEnvelope();
    if (env.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    int64_t clipDuration = clip->duration();
    if (clipDuration <= 0) { painter.restore(); return; }

    // Map a dB value to a Y pixel within the clip rect.
    // +12 dB = top (clipY), -60 dB = bottom (clipY + clipH), 0 dB ~ center
    auto dbToY = [&](float db) -> int {
        float clamped = std::clamp(db, kGainEnvelopeMinDb, kGainEnvelopeMaxDb);
        float frac = (clamped - kGainEnvelopeMinDb)
                   / (kGainEnvelopeMaxDb - kGainEnvelopeMinDb);
        return clipY + clipH - static_cast<int>(frac * clipH);
    };

    // Map a sample offset to an X pixel
    auto offsetToX = [&](int64_t offset) -> int {
        double frac = static_cast<double>(offset) / clipDuration;
        return clipX + static_cast<int>(frac * clipW);
    };

    // 0 dB reference line Y
    int zeroDbY = dbToY(0.0f);

    // Build the envelope polyline and fill path
    QPainterPath curvePath;
    QPainterPath fillPath;

    bool first = true;
    for (const auto& pt : env) {
        int px = offsetToX(pt.offsetSamples);
        int py = dbToY(pt.gainDb);
        if (first) {
            curvePath.moveTo(px, py);
            fillPath.moveTo(px, zeroDbY);
            fillPath.lineTo(px, py);
            first = false;
        } else {
            curvePath.lineTo(px, py);
            fillPath.lineTo(px, py);
        }
    }

    // Close fill path back to 0 dB line
    if (!first) {
        int lastX = offsetToX(env.last().offsetSamples);
        fillPath.lineTo(lastX, zeroDbY);
        fillPath.closeSubpath();

        // Semi-transparent yellow fill between curve and 0 dB line
        painter.setBrush(kGainEnvelopeFill);
        painter.setPen(Qt::NoPen);
        painter.drawPath(fillPath);

        // Draw the curve line
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(kGainEnvelopeColor, 2.0));
        painter.drawPath(curvePath);
    }

    // Draw 0 dB reference line (thin dotted)
    painter.setPen(QPen(QColor(255, 210, 0, 80), 1, Qt::DotLine));
    painter.drawLine(clipX, zeroDbY, clipX + clipW, zeroDbY);

    // Draw diamond handles at each gain point
    for (const auto& pt : env) {
        int px = offsetToX(pt.offsetSamples);
        int py = dbToY(pt.gainDb);

        if (px < clipX - kGainPointSize || px > clipX + clipW + kGainPointSize)
            continue;

        // Diamond shape (rotated square)
        int half = kGainPointSize / 2;
        QPolygon diamond;
        diamond << QPoint(px, py - half)
                << QPoint(px + half, py)
                << QPoint(px, py + half)
                << QPoint(px - half, py);

        painter.setBrush(kGainEnvelopeColor);
        painter.setPen(QPen(kGainEnvelopeColor.darker(130), 1));
        painter.drawPolygon(diamond);
    }

    painter.restore();
}

int TimelineWidget::hitTestGainPoint(Clip* clip, int clipX, int clipW,
                                     int clipY, int clipH, int x, int y) const
{
    const auto& env = clip->gainEnvelope();
    if (env.isEmpty()) return -1;

    int64_t clipDuration = clip->duration();
    if (clipDuration <= 0) return -1;

    auto dbToY = [&](float db) -> int {
        float clamped = std::clamp(db, kGainEnvelopeMinDb, kGainEnvelopeMaxDb);
        float frac = (clamped - kGainEnvelopeMinDb)
                   / (kGainEnvelopeMaxDb - kGainEnvelopeMinDb);
        return clipY + clipH - static_cast<int>(frac * clipH);
    };

    auto offsetToX = [&](int64_t offset) -> int {
        double frac = static_cast<double>(offset) / clipDuration;
        return clipX + static_cast<int>(frac * clipW);
    };

    for (int i = 0; i < env.size(); ++i) {
        int px = offsetToX(env[i].offsetSamples);
        int py = dbToY(env[i].gainDb);
        int dx = x - px;
        int dy = y - py;
        if (dx * dx + dy * dy <= kGainPointHitRadius * kGainPointHitRadius)
            return i;
    }

    return -1;
}

// ── Normalize Selected Clips ────────────────────────────────────────────────

void TimelineWidget::normalizeSelectedClips()
{
    if (!m_timeline || m_selectedClips.isEmpty())
        return;

    // Ask the user for a target level
    bool ok = false;
    double targetDb = QInputDialog::getDouble(
        this, tr("Normalize"), tr("Normalize to (dBFS):"),
        -0.3, -60.0, 0.0, 1, &ok);
    if (!ok) return;

    auto* cache = WaveformCache::instance();

    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
        if (!audioTrack) continue;

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            if (!m_selectedClips.contains(c)) continue;
            Clip* clip = audioTrack->clip(c);
            if (!clip) continue;

            // Get peak from WaveformCache data
            const WaveformData* wf = cache->getWaveform(clip->sourcePath());
            if (!wf || wf->peaks.empty()) continue;

            // Find the peak across all blocks within this clip's source range
            int blockSize = wf->blockSize;
            int64_t srcIn = clip->sourceIn();
            int64_t srcOut = clip->sourceOut();
            int blk0 = static_cast<int>(srcIn / blockSize);
            int blk1 = static_cast<int>(srcOut / blockSize);
            blk0 = std::clamp(blk0, 0, static_cast<int>(wf->peaks.size()) - 1);
            blk1 = std::clamp(blk1, 0, static_cast<int>(wf->peaks.size()) - 1);

            float peakVal = 0.0f;
            for (int b = blk0; b <= blk1; ++b) {
                if (wf->peaks[static_cast<size_t>(b)] > peakVal)
                    peakVal = wf->peaks[static_cast<size_t>(b)];
            }

            if (peakVal <= 0.0f) continue;  // silent clip

            // Calculate gain adjustment: targetDb - current peak dB
            float peakDb = 20.0f * std::log10(peakVal);
            float gainAdjust = static_cast<float>(targetDb) - peakDb;

            // Apply as linear gain multiplier
            float currentGain = clip->gain();
            float currentGainDb = 20.0f * std::log10(std::max(currentGain, 1e-10f));
            float newGainDb = currentGainDb + gainAdjust;
            float newGain = std::pow(10.0f, newGainDb / 20.0f);
            clip->setGain(newGain);
        }
    }

    update();
}

// ── Per-Track Vertical Zoom Helper ──────────────────────────────────────────

float TimelineWidget::verticalZoomForTrack(int trackIndex) const
{
    return m_trackVerticalZoom.value(trackIndex, 1.0f);
}

// ── Freeze Indicator Drawing ────────────────────────────────────────────────

void TimelineWidget::drawFreezeIndicator(QPainter& painter, AudioTrack* track, int yTop)
{
    if (!track) return;

    // Check if the DSP chain exists and is bypassed (frozen state from TrackBouncer)
    // Use the const accessor to avoid lazily creating a DspChain in draw code
    const DspChain* chain = static_cast<const AudioTrack*>(track)->effectChain();
    if (!chain || !chain->isBypassed()) return;

    painter.save();

    // Draw a slight blue tint over the frozen track lane
    QColor frozenTint(120, 180, 255, 20);
    painter.fillRect(0, yTop, width(), kTrackHeight, frozenTint);

    // Draw snowflake icon in the top-right corner of the track lane
    QFont freezeFont = font();
    freezeFont.setPointSize(14);
    painter.setFont(freezeFont);
    painter.setPen(kFreezeColor);
    // Snowflake character U+2744
    painter.drawText(width() - 24, yTop + 18, QString::fromUtf8("\xe2\x9d\x84"));

    painter.restore();
}

// ── Split at Playhead (Cmd+E) ───────────────────────────────────────────────

void TimelineWidget::splitAtPlayhead()
{
    if (!m_timeline) return;

    int64_t playhead = m_timeline->playhead();
    if (playhead <= 0) return;

    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        auto* audioTrack = qobject_cast<AudioTrack*>(m_timeline->track(t));
        if (!audioTrack) continue;

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            Clip* clip = audioTrack->clip(c);
            if (!clip) continue;

            // Check if the playhead falls within this clip (exclusive of edges)
            if (playhead > clip->timelinePosition() && playhead < clip->endPosition()) {
                auto* cmd = new SplitClipCommand(clip, playhead);
                cmd->redo();  // Execute immediately (no UndoManager wired at widget level)
            }
        }
    }

    update();
}

// ── Ripple Editing Mode ─────────────────────────────────────────────────────

void TimelineWidget::setRippleMode(bool enabled)
{
    if (m_rippleMode != enabled) {
        m_rippleMode = enabled;
        emit rippleModeChanged(enabled);
        update();
    }
}

void TimelineWidget::rippleShift(AudioTrack* track, int64_t fromPosition, int64_t delta)
{
    if (!track || delta == 0) return;

    for (int c = 0; c < track->clipCount(); ++c) {
        Clip* clip = track->clip(c);
        if (!clip) continue;

        if (clip->timelinePosition() >= fromPosition) {
            int64_t newPos = clip->timelinePosition() + delta;
            if (newPos < 0) newPos = 0;
            clip->setTimelinePosition(newPos);
        }
    }
}

void TimelineWidget::drawRippleIndicator(QPainter& painter, int viewWidth)
{
    if (!m_rippleMode) return;

    painter.save();

    // Draw a small "R" badge in the top-left corner below the ruler
    QFont badgeFont = font();
    badgeFont.setPointSize(9);
    badgeFont.setBold(true);
    painter.setFont(badgeFont);

    QRect badge(4, kRulerHeight + 4, 18, 16);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(200, 120, 30, 200));
    painter.drawRoundedRect(badge, 3, 3);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(badge, Qt::AlignCenter, QStringLiteral("R"));

    painter.restore();
}

// ── Time Region Selection ──────────────────────────────────────────────────

void TimelineWidget::setSelection(int64_t start, int64_t end)
{
    if (start > end) std::swap(start, end);
    m_selectionStart = qMax(int64_t(0), start);
    m_selectionEnd   = end;
    emit selectionChanged(m_selectionStart, m_selectionEnd);
    update();
}

void TimelineWidget::clearSelection()
{
    m_selectionStart = 0;
    m_selectionEnd   = 0;
    m_selectingRegion = false;
    emit selectionChanged(0, 0);
    update();
}

void TimelineWidget::drawSelection(QPainter& painter, int viewWidth, int viewHeight)
{
    if (m_selectionEnd <= m_selectionStart) return;

    int selX1 = sampleToPixel(m_selectionStart, m_scroll, m_zoom);
    int selX2 = sampleToPixel(m_selectionEnd, m_scroll, m_zoom);
    if (selX2 < 0 || selX1 > viewWidth) return;

    selX1 = qMax(0, selX1);
    selX2 = qMin(viewWidth, selX2);
    int selW = selX2 - selX1;
    if (selW <= 0) return;

    // Ruler highlight (lighter)
    painter.fillRect(selX1, 0, selW, kRulerHeight, kTimeSelectionRuler);

    // Track area highlight (semi-transparent blue)
    painter.fillRect(selX1, kRulerHeight, selW, viewHeight - kRulerHeight,
                     kTimeSelectionFill);

    // Left and right border lines
    painter.setPen(QPen(kTimeSelectionBorder, 1));
    painter.drawLine(selX1, 0, selX1, viewHeight);
    painter.drawLine(selX2, 0, selX2, viewHeight);
}

} // namespace dawcast::widgets
