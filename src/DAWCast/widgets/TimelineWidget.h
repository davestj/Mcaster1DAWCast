// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <cstdint>

class QPainter;

namespace dawcast { class AudioTrack; }
namespace dawcast { class Automation; }
namespace dawcast { class Clip; }
namespace dawcast { class MidiClip; }
namespace dawcast { class MidiTrack; }
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
    void automationPointAdded(int trackIndex, const QString& param, int64_t time, float value);
    void automationPointMoved(int trackIndex, const QString& param, int pointIndex, int64_t time, float value);
    void automationPointRemoved(int trackIndex, const QString& param, int pointIndex);

private slots:
    void onWaveformReady(const QString& filePath);

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
    void drawWaveform(QPainter& painter, Clip* clip, const QColor& baseColor,
                      int clipX, int clipY, int clipW, int clipH);
    void drawMidiClip(QPainter& painter, MidiClip* clip, int trackIndex, int clipIndex, int yTop);
    void drawAutomation(QPainter& painter, AudioTrack* track, int trackIndex);

    // Automation point hit-testing
    int  hitTestAutomationPoint(int trackIndex, int x, int y) const;
    QColor automationColor(const QString& paramName) const;

    Timeline* m_timeline = nullptr;
    float           m_zoom     = 1.0f;
    int64_t         m_scroll   = 0;
    QList<int>      m_selectedClips;
    bool            m_dragging = false;
    QPoint          m_dragStart;
    QRect           m_rubberBand;

    // Automation editing state
    int             m_editingAutoTrack = -1;    // Which track's automation is being edited
    QString         m_editingAutoParam;         // Which parameter lane is active
    int             m_draggingAutoPoint = -1;   // Index of the point being dragged (-1 = none)
    bool            m_draggingAuto = false;     // Whether an automation point drag is active
};

} // namespace dawcast::widgets
