// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QUndoCommand>
#include <QString>
#include <cstdint>

namespace dawcast {

class Clip;
class Timeline;
class AudioTrack;

// ─── MoveClipCommand ────────────────────────────────────────────────

class MoveClipCommand : public QUndoCommand
{
public:
    MoveClipCommand(Clip* clip, int64_t oldPosition, int64_t newPosition,
                    QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Clip*   m_clip;
    int64_t m_oldPosition;
    int64_t m_newPosition;
};

// ─── SplitClipCommand ───────────────────────────────────────────────

class SplitClipCommand : public QUndoCommand
{
public:
    SplitClipCommand(Clip* clip, int64_t splitPosition,
                     QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Clip*       m_originalClip;
    Clip*       m_newClip = nullptr;
    AudioTrack* m_track = nullptr;
    int64_t     m_splitPosition;
    int64_t     m_originalSourceOut;
};

// ─── TrimClipCommand ────────────────────────────────────────────────

class TrimClipCommand : public QUndoCommand
{
public:
    enum class Edge { Left, Right };

    TrimClipCommand(Clip* clip, Edge edge, int64_t oldBound, int64_t newBound,
                    QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Clip*   m_clip;
    Edge    m_edge;
    int64_t m_oldBound;
    int64_t m_newBound;
};

// ─── DeleteClipCommand ──────────────────────────────────────────────

class DeleteClipCommand : public QUndoCommand
{
public:
    DeleteClipCommand(AudioTrack* track, int clipIndex,
                      QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    AudioTrack* m_track;
    int         m_clipIndex;
    Clip*       m_clip = nullptr;
    bool        m_ownsClip = false;
};

// ─── AddTrackCommand ────────────────────────────────────────────────

class AddTrackCommand : public QUndoCommand
{
public:
    enum class TrackType { Audio, Video };

    AddTrackCommand(Timeline* timeline, TrackType type,
                    QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Timeline* m_timeline;
    TrackType m_type;
    int       m_trackIndex = -1;
};

} // namespace dawcast
