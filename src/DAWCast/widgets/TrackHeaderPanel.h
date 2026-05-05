// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QList>
#include <QSet>

class QVBoxLayout;
class QScrollArea;
class QScrollBar;

namespace dawcast { class Timeline; }
namespace dawcast { class TrackGroup; }
namespace dawcast { class AudioMixer; }

namespace dawcast::widgets {

class TrackHeaderWidget;

/// Panel that sits to the left of the TimelineWidget, containing
/// a "TRACKS" title bar with an Add (+) button, and a vertically
/// scrollable stack of TrackHeaderWidget instances (one per track).
///
/// Vertical scroll is synchronised externally with the TimelineWidget
/// so headers and waveform lanes stay aligned.
class TrackHeaderPanel : public QWidget {
    Q_OBJECT

public:
    explicit TrackHeaderPanel(QWidget* parent = nullptr);
    ~TrackHeaderPanel() override;

    /// Binds the panel to a Timeline model.  Rebuilds the header stack
    /// whenever tracks are added or removed.
    void setTimeline(dawcast::Timeline* timeline);

    /// Optional: bind the live audio mixer so per-track volume/pan/mute/solo
    /// updates propagate to the running DSP path, not just the stored
    /// AudioTrack field.
    void setAudioMixer(dawcast::AudioMixer* mixer) { m_audioMixer = mixer; }

    /// Returns the vertical scroll bar so the caller can synchronise it
    /// with the TimelineWidget's scroll position.
    QScrollBar* verticalScrollBar() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// Ruler-height offset that headers use to line up with the timeline ruler.
    static constexpr int kRulerHeight = 28;
    /// Height of each track row (must match TimelineWidget::kBaseTrackHeight).
    static constexpr int kTrackHeight = 108;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

signals:
    void addTrackRequested();
    void eqRequested(int trackIndex);
    void settingsRequested(int trackIndex);
    void createGroupRequested();
    void groupCollapseToggled(int groupIndex, bool collapsed);
    void duplicateTrackRequested(int trackIndex);
    void deleteTrackRequested(int trackIndex);
    void trackMoveRequested(int fromIndex, int toIndex);
    /// Emitted when the user single-clicks a track header. The effects rack
    /// and other context panels use this to focus on the clicked track.
    void trackSelected(int trackIndex);

    // ── Per-track transport (forwarded from TrackHeaderWidget) ─────
    void trackPlayClicked(int trackIndex);
    void trackStopClicked(int trackIndex);
    void trackRewindClicked(int trackIndex);
    void trackFastForwardClicked(int trackIndex);
    void trackPauseClicked(int trackIndex);
    void trackRecordClicked(int trackIndex);

    /// Files dropped on a specific track header (or on empty area = -1).
    void filesDroppedOnTrack(int trackIndex, const QStringList& filePaths);

public slots:
    /// Rebuild the list of track headers from the current Timeline model.
    void rebuildHeaders();

private:
    int headerIndexAtPos(const QPoint& pos) const;

    dawcast::Timeline*   m_timeline   = nullptr;
    dawcast::AudioMixer* m_audioMixer = nullptr;
    QScrollArea*   m_scrollArea   = nullptr;
    QWidget*       m_headerStack  = nullptr;
    QVBoxLayout*   m_stackLayout  = nullptr;
    QList<TrackHeaderWidget*> m_headers;
    QList<QWidget*>           m_groupWidgets;

    // Track reorder drag state
    bool    m_dragReorder      = false;
    int     m_dragFromIndex    = -1;
    QPoint  m_dragStartPos;
};

} // namespace dawcast::widgets
