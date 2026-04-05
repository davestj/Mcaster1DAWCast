// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QTimer>
#include <atomic>
#include <vector>
#include <cstdint>

#include "../core/AudioBuffer.h"

namespace dawcast {

class Timeline;
class AudioEngine;
class AudioMixer;
class AudioTrack;
class AudioClipReader;
class BusRouter;
class Clip;
class Metronome;
class MultitrackRecorder;
class RTMPStreamer;

/// Orchestrates real-time audio playback from the Timeline through
/// the AudioMixer to the AudioEngine (PortAudio) output.
///
/// Thread safety:
///   - processBlock() runs on the PortAudio audio thread (RT-safe).
///   - play/pause/stop/seekTo are called from the GUI thread.
///   - Playhead position is shared via std::atomic<int64_t>.
///   - A QTimer polls the atomic and emits positionChanged on the GUI thread.
class PlaybackEngine : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackEngine(QObject* parent = nullptr);
    ~PlaybackEngine() override;

    void setTimeline(Timeline* timeline);
    void setAudioEngine(AudioEngine* engine);

    /// Set the multitrack recorder that receives input during recording.
    void setRecorder(MultitrackRecorder* recorder);
    [[nodiscard]] MultitrackRecorder* recorder() const { return m_recorder; }

    /// Set the RTMP streamer to receive mixed audio output for live streaming.
    /// When set and streaming, processBlock() feeds the mixed output to the
    /// streamer via its lock-free pushAudioFrame() method.
    void setRTMPStreamer(RTMPStreamer* streamer);
    [[nodiscard]] RTMPStreamer* rtmpStreamer() const { return m_rtmpStreamer; }

    /// Accessor for the built-in metronome / click track.
    [[nodiscard]] Metronome* metronome() const { return m_metronome; }

    /// Set the bus router for bus-based audio routing.
    void setBusRouter(BusRouter* router);
    [[nodiscard]] BusRouter* busRouter() const { return m_busRouter; }

    void play();
    void pause();
    void stop();
    void seekTo(int64_t samplePosition);

    [[nodiscard]] bool    isPlaying() const;
    [[nodiscard]] int64_t currentPosition() const;  // in samples

signals:
    void positionChanged(int64_t samples);
    void playbackStarted();
    void playbackStopped();

public:
    /// Called from AudioEngine::processCallback on the audio thread.
    /// Populates per-track buffers from decoded clip data, applies DSP
    /// chains, sets mixer strip input pointers, then advances the
    /// playhead. The caller (AudioEngine) calls mixer->process()
    /// immediately after this returns.
    ///
    /// @param input         Raw PortAudio input buffer (may be nullptr)
    /// @param inputChannels Number of interleaved channels in input buffer
    ///
    /// RT-safe: no allocations, no locks, no Qt signals.
    void processBlock(int frames, int channels,
                      const float* input = nullptr, int inputChannels = 0);

private slots:
    void onPositionTimer();

private:
    /// Rebuild the AudioClipReader cache when the timeline changes.
    /// Called on the GUI thread only.
    void rebuildReaders();

    /// Ensure the mixer has the correct number of strips for the timeline.
    void syncMixerStrips();

    /// Pre-allocate per-track audio buffers for the current buffer size.
    void ensureTrackBuffers(int frames, int channels);

    Timeline*            m_timeline      = nullptr;
    AudioEngine*         m_audioEngine   = nullptr;
    AudioMixer*          m_mixer         = nullptr;
    BusRouter*           m_busRouter     = nullptr;
    Metronome*           m_metronome     = nullptr;
    MultitrackRecorder*  m_recorder      = nullptr;
    RTMPStreamer*         m_rtmpStreamer  = nullptr;

    // Playhead state -- shared between audio thread and GUI thread
    std::atomic<int64_t> m_playheadPos{0};
    std::atomic<bool>    m_playing{false};

    // GUI-thread timer to poll position and emit signal
    QTimer* m_positionTimer = nullptr;

    // Pre-decoded clip readers (one per Clip on audio tracks).
    // Rebuilt when timeline structure changes. Read on audio thread,
    // modified only on GUI thread while playback is stopped.
    struct TrackPlayback {
        int                            mixerStrip = -1;
        AudioTrack*                    audioTrack = nullptr;
        std::vector<AudioClipReader*>  readers;
        std::vector<float>             buffer;      // per-track audio scratch
        AudioBuffer                    audioBuf{};  // points into buffer[]
    };
    std::vector<TrackPlayback> m_tracks;

    // Current buffer dimensions (to detect when realloc is needed)
    int m_allocFrames   = 0;
    int m_allocChannels = 0;

    // True when the timeline structure has changed and readers need rebuilding
    // before the next play(). Set by track add/remove signals, cleared in play().
    bool m_needsRebuild = true;

    // Tracks whether we are in the "stopped at position" state for
    // double-stop-to-rewind behavior.
    bool m_stoppedAtPosition = false;
};

} // namespace dawcast
