// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QList>

namespace dawcast {
class MidiClip;
struct MidiEvent;
}

namespace dawcast::widgets {

class PianoRollWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PianoRollWidget(QWidget* parent = nullptr);
    ~PianoRollWidget() override;

    void setMidiClip(MidiClip* clip);
    void setZoom(float hZoom, float vZoom);
    void setTicksPerBeat(int tpb) { m_ticksPerBeat = tpb; }

signals:
    void noteAdded(int note, int64_t tick, int64_t duration);
    void noteDeleted(int index);
    void noteMoved(int index, int note, int64_t tick);
    void noteResized(int index, int64_t newDuration);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // Drawing helpers
    void drawPianoKeys(class QPainter& painter);
    void drawGrid(class QPainter& painter, int gridLeft, int gridWidth);
    void drawNotes(class QPainter& painter, int gridLeft, int gridWidth);
    void drawVelocityLane(class QPainter& painter, int gridLeft, int gridWidth);

    // Coordinate conversion
    int  noteToY(int note) const;
    int  yToNote(int y) const;
    int  tickToX(int64_t tick, int gridLeft) const;
    int64_t xToTick(int x, int gridLeft) const;

    static bool isBlackKey(int note);

    MidiClip* m_clip = nullptr;

    float m_hZoom = 0.5f;   // pixels per tick
    float m_vZoom = 1.0f;   // vertical scale factor

    int m_ticksPerBeat = 480;

    // Layout constants
    static constexpr int kPianoKeyWidth   = 60;
    static constexpr int kNoteHeight      = 12;
    static constexpr int kVelocityLaneH   = 60;
    static constexpr int kTotalNotes      = 128;

    // Mouse interaction state
    enum DragMode { None, AddNote, MoveNote, ResizeNote };
    DragMode m_dragMode    = None;
    int      m_dragNoteIdx = -1;
    QPoint   m_dragStart;
    int64_t  m_dragStartTick = 0;
    int      m_dragStartNote = 0;
};

} // namespace dawcast::widgets
