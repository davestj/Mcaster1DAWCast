// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace dawcast {

class AudioTrack;
class Timeline;

/// Renders all effects on a track in-place, creating a new audio file
/// and replacing the track's clips with a single bounced clip.
///
/// "Bounce" renders the full track (clips + DSP chain) to a WAV file.
/// "Freeze" does the same but also bypasses the DSP chain (keeping it
/// for later unfreeze).
///
/// This runs on a worker thread via QThread or QtConcurrent.
class TrackBouncer : public QObject
{
    Q_OBJECT

public:
    explicit TrackBouncer(QObject* parent = nullptr);
    ~TrackBouncer() override;

    /// Bounce (render) all clips + effects on the track to a single WAV file.
    /// @param track     The audio track to bounce
    /// @param timeline  The project timeline (for sample rate, duration context)
    /// @param outputDir Directory where the bounced WAV will be saved
    void bounceTrack(AudioTrack* track, Timeline* timeline, const QString& outputDir);

    /// Freeze: same as bounce, but also bypasses the DspChain afterwards.
    /// The original effect chain is preserved so it can be unfrozen later.
    void freezeTrack(AudioTrack* track, Timeline* timeline, const QString& outputDir);

    /// Unfreeze: re-enable the DSP chain on a previously frozen track.
    /// Does NOT remove the bounced clip — the user decides whether to
    /// undo that manually.
    static void unfreezeTrack(AudioTrack* track);

signals:
    /// Emitted periodically during the bounce to indicate progress.
    void progress(int percent);

    /// Emitted when the bounce completes successfully.
    void finished(const QString& bouncedFilePath);

    /// Emitted if the bounce fails.
    void error(const QString& message);
};

} // namespace dawcast
