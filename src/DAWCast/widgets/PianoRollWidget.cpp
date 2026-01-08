// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PianoRollWidget.h"
#include "MidiClip.h"
#include "MidiEvent.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>

#include <cmath>
#include <algorithm>

namespace dawcast::widgets {

namespace {
const QColor kBackground(36, 36, 42);
const QColor kGridLine(60, 60, 66);
const QColor kBeatLine(80, 80, 90);
const QColor kBarLine(110, 110, 120);
const QColor kWhiteKeyBg(240, 240, 240);
const QColor kBlackKeyBg(40, 40, 40);
const QColor kWhiteKeyLane(42, 42, 48);
const QColor kBlackKeyLane(36, 36, 42);
const QColor kNoteColor(90, 160, 230);
const QColor kNoteSelected(255, 200, 60);
const QColor kVelocityBar(90, 160, 230, 180);
const QColor kPianoKeyBorder(100, 100, 100);

const char* kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

constexpr int kResizeHandleWidth = 6;
} // anonymous namespace

PianoRollWidget::PianoRollWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 300);
}

PianoRollWidget::~PianoRollWidget() = default;

void PianoRollWidget::setMidiClip(MidiClip* clip)
{
    m_clip = clip;
    update();
}

void PianoRollWidget::setZoom(float hZoom, float vZoom)
{
    m_hZoom = qBound(0.01f, hZoom, 10.0f);
    m_vZoom = qBound(0.5f, vZoom, 4.0f);
    update();
}

bool PianoRollWidget::isBlackKey(int note)
{
    int n = note % 12;
    return (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
}

int PianoRollWidget::noteToY(int note) const
{
    // Note 127 at top, note 0 at bottom
    int rowH = static_cast<int>(kNoteHeight * m_vZoom);
    return (kTotalNotes - 1 - note) * rowH;
}

int PianoRollWidget::yToNote(int y) const
{
    int rowH = static_cast<int>(kNoteHeight * m_vZoom);
    if (rowH <= 0) rowH = 1;
    int note = kTotalNotes - 1 - (y / rowH);
    return qBound(0, note, 127);
}

int PianoRollWidget::tickToX(int64_t tick, int gridLeft) const
{
    return gridLeft + static_cast<int>(tick * m_hZoom);
}

int64_t PianoRollWidget::xToTick(int x, int gridLeft) const
{
    if (m_hZoom <= 0.0f) return 0;
    return static_cast<int64_t>((x - gridLeft) / m_hZoom);
}

// ── Paint ──────────────────────────────────────────────────────────────────

void PianoRollWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), kBackground);

    int gridLeft  = kPianoKeyWidth;
    int gridWidth = width() - kPianoKeyWidth;

    drawGrid(p, gridLeft, gridWidth);
    drawNotes(p, gridLeft, gridWidth);
    drawPianoKeys(p);
    drawVelocityLane(p, gridLeft, gridWidth);
}

void PianoRollWidget::drawPianoKeys(QPainter& p)
{
    int rowH = static_cast<int>(kNoteHeight * m_vZoom);
    QFont keyFont = font();
    keyFont.setPointSize(7);
    p.setFont(keyFont);

    for (int note = 0; note < kTotalNotes; ++note) {
        int y = noteToY(note);
        bool black = isBlackKey(note);

        // Key rectangle
        QColor keyColor = black ? kBlackKeyBg : kWhiteKeyBg;
        p.fillRect(0, y, kPianoKeyWidth, rowH, keyColor);

        // Border
        p.setPen(kPianoKeyBorder);
        p.drawLine(0, y + rowH, kPianoKeyWidth, y + rowH);

        // Label on each C
        if (note % 12 == 0) {
            int octave = (note / 12) - 1;
            QString label = QStringLiteral("C%1").arg(octave);
            QColor textColor = black ? Qt::white : Qt::black;
            p.setPen(textColor);
            p.drawText(4, y + rowH - 2, label);
        }
    }

    // Right border of piano
    p.setPen(QColor(80, 80, 90));
    p.drawLine(kPianoKeyWidth - 1, 0, kPianoKeyWidth - 1, height());
}

void PianoRollWidget::drawGrid(QPainter& p, int gridLeft, int gridWidth)
{
    int rowH = static_cast<int>(kNoteHeight * m_vZoom);

    // Horizontal lanes (one per note)
    for (int note = 0; note < kTotalNotes; ++note) {
        int y = noteToY(note);
        QColor laneBg = isBlackKey(note) ? kBlackKeyLane : kWhiteKeyLane;
        p.fillRect(gridLeft, y, gridWidth, rowH, laneBg);

        p.setPen(QPen(kGridLine, 1));
        p.drawLine(gridLeft, y + rowH, gridLeft + gridWidth, y + rowH);
    }

    if (!m_clip) return;

    // Vertical grid lines at beat/bar boundaries
    int64_t endTick = m_clip->durationTicks();
    if (endTick <= 0) endTick = m_ticksPerBeat * 16; // show 16 beats by default

    int totalHeight = kTotalNotes * rowH;

    for (int64_t tick = 0; tick <= endTick; tick += m_ticksPerBeat) {
        int x = tickToX(tick, gridLeft);
        if (x < gridLeft || x > gridLeft + gridWidth) continue;

        bool isBar = (tick % (m_ticksPerBeat * 4)) == 0;
        p.setPen(QPen(isBar ? kBarLine : kBeatLine, isBar ? 1 : 1, isBar ? Qt::SolidLine : Qt::DotLine));
        p.drawLine(x, 0, x, totalHeight);
    }
}

void PianoRollWidget::drawNotes(QPainter& p, int gridLeft, int /*gridWidth*/)
{
    if (!m_clip) return;

    int rowH = static_cast<int>(kNoteHeight * m_vZoom);
    const auto& events = m_clip->events();

    for (int i = 0; i < events.size(); ++i) {
        const MidiEvent& ev = events[i];
        if (ev.type != MidiEvent::NoteOn) continue;

        int x = tickToX(ev.tick, gridLeft);
        int y = noteToY(ev.note);
        int w = static_cast<int>(ev.durationTicks * m_hZoom);
        if (w < 2) w = 2;

        // Velocity maps to alpha: 0 = dim, 127 = bright
        QColor noteCol = kNoteColor;
        int alpha = 100 + (ev.velocity * 155 / 127);
        noteCol.setAlpha(alpha);

        p.setBrush(noteCol);
        p.setPen(QPen(noteCol.lighter(130), 1));
        p.drawRoundedRect(x, y + 1, w, rowH - 2, 2, 2);

        // Resize handle indicator (right edge)
        if (w > kResizeHandleWidth * 2) {
            p.setPen(QPen(noteCol.lighter(160), 1));
            p.drawLine(x + w - 2, y + 3, x + w - 2, y + rowH - 4);
        }
    }
}

void PianoRollWidget::drawVelocityLane(QPainter& p, int gridLeft, int /*gridWidth*/)
{
    if (!m_clip) return;

    int velTop = height() - kVelocityLaneH;

    // Separator line
    p.setPen(QPen(QColor(80, 80, 90), 1));
    p.drawLine(gridLeft, velTop, width(), velTop);

    // Background
    p.fillRect(gridLeft, velTop, width() - gridLeft, kVelocityLaneH, QColor(30, 30, 36));

    const auto& events = m_clip->events();
    for (const MidiEvent& ev : events) {
        if (ev.type != MidiEvent::NoteOn) continue;

        int x = tickToX(ev.tick, gridLeft);
        int barH = static_cast<int>(ev.velocity / 127.0f * (kVelocityLaneH - 4));
        int barY = velTop + kVelocityLaneH - barH - 2;

        p.fillRect(x, barY, qMax(3, static_cast<int>(ev.durationTicks * m_hZoom / 4)), barH, kVelocityBar);
    }
}

// ── Mouse Interaction ──────────────────────────────────────────────────────

void PianoRollWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_clip) {
        QWidget::mousePressEvent(event);
        return;
    }

    int gridLeft = kPianoKeyWidth;
    int rowH = static_cast<int>(kNoteHeight * m_vZoom);

    // Only handle clicks in the grid area (not piano keys or velocity lane)
    if (event->pos().x() < gridLeft || event->pos().y() > height() - kVelocityLaneH) {
        QWidget::mousePressEvent(event);
        return;
    }

    int64_t clickTick = xToTick(event->pos().x(), gridLeft);
    int clickNote = yToNote(event->pos().y());

    // Right-click: delete note under cursor
    if (event->button() == Qt::RightButton) {
        const auto& events = m_clip->events();
        for (int i = events.size() - 1; i >= 0; --i) {
            const MidiEvent& ev = events[i];
            if (ev.type != MidiEvent::NoteOn) continue;
            if (ev.note == clickNote && clickTick >= ev.tick && clickTick < ev.tick + ev.durationTicks) {
                m_clip->removeEvent(i);
                emit noteDeleted(i);
                update();
                return;
            }
        }
        QWidget::mousePressEvent(event);
        return;
    }

    // Left-click: check if we hit an existing note
    if (event->button() == Qt::LeftButton) {
        const auto& events = m_clip->events();
        for (int i = 0; i < events.size(); ++i) {
            const MidiEvent& ev = events[i];
            if (ev.type != MidiEvent::NoteOn) continue;
            if (ev.note != clickNote) continue;
            if (clickTick < ev.tick || clickTick >= ev.tick + ev.durationTicks) continue;

            // Hit a note -- check if we're near the right edge (resize)
            int noteRightX = tickToX(ev.tick + ev.durationTicks, gridLeft);
            if (std::abs(event->pos().x() - noteRightX) <= kResizeHandleWidth) {
                m_dragMode = ResizeNote;
            } else {
                m_dragMode = MoveNote;
            }
            m_dragNoteIdx = i;
            m_dragStart = event->pos();
            m_dragStartTick = ev.tick;
            m_dragStartNote = ev.note;
            return;
        }

        // No note hit -- add a new note
        // Snap to beat grid
        int64_t snapTick = (clickTick / m_ticksPerBeat) * m_ticksPerBeat;

        MidiEvent newEv;
        newEv.type = MidiEvent::NoteOn;
        newEv.tick = snapTick;
        newEv.note = static_cast<uint8_t>(clickNote);
        newEv.velocity = 100;
        newEv.channel = 0;
        newEv.durationTicks = m_ticksPerBeat; // 1 beat

        m_clip->addEvent(newEv);
        emit noteAdded(clickNote, snapTick, m_ticksPerBeat);
        update();
    }
}

void PianoRollWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_clip || m_dragMode == None) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int gridLeft = kPianoKeyWidth;

    if (m_dragMode == MoveNote && m_dragNoteIdx >= 0) {
        int64_t deltaTick = xToTick(event->pos().x(), gridLeft) - xToTick(m_dragStart.x(), gridLeft);
        int deltaNote = yToNote(event->pos().y()) - yToNote(m_dragStart.y());

        // We need to modify the event -- remove and re-add
        auto events = m_clip->events();
        if (m_dragNoteIdx < events.size()) {
            MidiEvent ev = events[m_dragNoteIdx];
            m_clip->removeEvent(m_dragNoteIdx);

            ev.tick = qMax(int64_t(0), m_dragStartTick + deltaTick);
            ev.note = static_cast<uint8_t>(qBound(0, m_dragStartNote + deltaNote, 127));

            m_clip->addEvent(ev);
            // The index may have changed due to sorted insertion
            const auto& newEvents = m_clip->events();
            for (int i = 0; i < newEvents.size(); ++i) {
                if (newEvents[i].tick == ev.tick && newEvents[i].note == ev.note) {
                    m_dragNoteIdx = i;
                    break;
                }
            }
        }
        update();
    } else if (m_dragMode == ResizeNote && m_dragNoteIdx >= 0) {
        int64_t newEndTick = xToTick(event->pos().x(), gridLeft);
        auto events = m_clip->events();
        if (m_dragNoteIdx < events.size()) {
            MidiEvent ev = events[m_dragNoteIdx];
            int64_t newDuration = newEndTick - ev.tick;
            if (newDuration < m_ticksPerBeat / 4) newDuration = m_ticksPerBeat / 4; // minimum 1/16 note

            m_clip->removeEvent(m_dragNoteIdx);
            ev.durationTicks = newDuration;
            m_clip->addEvent(ev);

            // Find new index
            const auto& newEvents = m_clip->events();
            for (int i = 0; i < newEvents.size(); ++i) {
                if (newEvents[i].tick == ev.tick && newEvents[i].note == ev.note) {
                    m_dragNoteIdx = i;
                    break;
                }
            }
            emit noteResized(m_dragNoteIdx, newDuration);
        }
        update();
    }
}

void PianoRollWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragMode == MoveNote && m_dragNoteIdx >= 0 && m_clip) {
        const auto& events = m_clip->events();
        if (m_dragNoteIdx < events.size()) {
            const MidiEvent& ev = events[m_dragNoteIdx];
            emit noteMoved(m_dragNoteIdx, ev.note, ev.tick);
        }
    }

    m_dragMode = None;
    m_dragNoteIdx = -1;
    QWidget::mouseReleaseEvent(event);
}

} // namespace dawcast::widgets
