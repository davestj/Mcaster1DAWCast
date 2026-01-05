// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimelineCommands.h"
#include "Clip.h"
#include "AudioTrack.h"
#include "Timeline.h"

namespace dawcast {

// ─── MoveClipCommand ────────────────────────────────────────────────

MoveClipCommand::MoveClipCommand(Clip* clip, int64_t oldPosition, int64_t newPosition,
                                 QUndoCommand* parent)
    : QUndoCommand("Move Clip", parent)
    , m_clip(clip)
    , m_oldPosition(oldPosition)
    , m_newPosition(newPosition)
{
}

void MoveClipCommand::undo()
{
    if (m_clip) {
        m_clip->setTimelinePosition(m_oldPosition);
    }
}

void MoveClipCommand::redo()
{
    if (m_clip) {
        m_clip->setTimelinePosition(m_newPosition);
    }
}

// ─── SplitClipCommand ───────────────────────────────────────────────

SplitClipCommand::SplitClipCommand(Clip* clip, int64_t splitPosition,
                                   QUndoCommand* parent)
    : QUndoCommand("Split Clip", parent)
    , m_originalClip(clip)
    , m_splitPosition(splitPosition)
    , m_originalSourceOut(clip ? clip->sourceOut() : 0)
{
    // Resolve the parent track so we can add/remove the new clip
    if (m_originalClip) {
        m_track = qobject_cast<AudioTrack*>(m_originalClip->parent());
    }
}

void SplitClipCommand::undo()
{
    if (!m_originalClip) return;

    // Restore original clip's source out to merge back
    m_originalClip->setSourceOut(m_originalSourceOut);

    // Remove the right-side clip from the track
    if (m_newClip && m_track) {
        for (int i = 0; i < m_track->clipCount(); ++i) {
            if (m_track->clip(i) == m_newClip) {
                m_track->removeClip(i);
                break;
            }
        }
    }
    delete m_newClip;
    m_newClip = nullptr;
}

void SplitClipCommand::redo()
{
    if (!m_originalClip) return;

    // Calculate split point relative to source media
    int64_t splitInSource = m_originalClip->sourceIn() +
        (m_splitPosition - m_originalClip->timelinePosition());

    // Trim original clip's right edge to the split point
    m_originalClip->setSourceOut(splitInSource);

    // Create new clip for the right portion
    m_newClip = new Clip();
    m_newClip->setSourcePath(m_originalClip->sourcePath());
    m_newClip->setSourceIn(splitInSource);
    m_newClip->setSourceOut(m_originalSourceOut);
    m_newClip->setTimelinePosition(m_splitPosition);
    m_newClip->setGain(m_originalClip->gain());
    m_newClip->setFadeIn(0);
    m_newClip->setFadeOut(m_originalClip->fadeOut());

    // Zero out the original clip's fade-out since it now ends at the split
    m_originalClip->setFadeOut(0);

    // Add the new clip to the same track
    if (m_track) {
        m_track->addClip(m_newClip);
    }
}

// ─── TrimClipCommand ────────────────────────────────────────────────

TrimClipCommand::TrimClipCommand(Clip* clip, Edge edge, int64_t oldBound, int64_t newBound,
                                 QUndoCommand* parent)
    : QUndoCommand("Trim Clip", parent)
    , m_clip(clip)
    , m_edge(edge)
    , m_oldBound(oldBound)
    , m_newBound(newBound)
{
}

void TrimClipCommand::undo()
{
    if (!m_clip) return;
    switch (m_edge) {
    case Edge::Left:
        m_clip->setSourceIn(m_oldBound);
        break;
    case Edge::Right:
        m_clip->setSourceOut(m_oldBound);
        break;
    }
}

void TrimClipCommand::redo()
{
    if (!m_clip) return;
    switch (m_edge) {
    case Edge::Left:
        m_clip->setSourceIn(m_newBound);
        break;
    case Edge::Right:
        m_clip->setSourceOut(m_newBound);
        break;
    }
}

// ─── DeleteClipCommand ──────────────────────────────────────────────

DeleteClipCommand::DeleteClipCommand(AudioTrack* track, int clipIndex,
                                     QUndoCommand* parent)
    : QUndoCommand("Delete Clip", parent)
    , m_track(track)
    , m_clipIndex(clipIndex)
{
}

void DeleteClipCommand::undo()
{
    if (!m_track || !m_clip) return;
    // Re-add the clip to the track
    m_track->addClip(m_clip);
    m_ownsClip = false;
}

void DeleteClipCommand::redo()
{
    if (!m_track) return;
    if (m_clipIndex < 0 || m_clipIndex >= m_track->clipCount()) return;

    // Store clip reference before removing so undo can re-add it
    m_clip = m_track->clip(m_clipIndex);
    m_track->removeClip(m_clipIndex);
    m_ownsClip = true;
}

// ─── AddTrackCommand ────────────────────────────────────────────────

AddTrackCommand::AddTrackCommand(Timeline* timeline, TrackType type,
                                 QUndoCommand* parent)
    : QUndoCommand("Add Track", parent)
    , m_timeline(timeline)
    , m_type(type)
{
}

void AddTrackCommand::undo()
{
    if (!m_timeline || m_trackIndex < 0) return;
    m_timeline->removeTrack(m_trackIndex);
}

void AddTrackCommand::redo()
{
    if (!m_timeline) return;
    switch (m_type) {
    case TrackType::Audio:
        m_timeline->addAudioTrack();
        break;
    case TrackType::Video:
        m_timeline->addVideoTrack();
        break;
    }
    m_trackIndex = m_timeline->trackCount() - 1;
}

} // namespace dawcast
