// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PlaybackEngine.h"
#include "AudioEngine.h"
#include "AudioMixer.h"
#include "AudioClipReader.h"
#include "BusRouter.h"
#include "Metronome.h"
#include "MultitrackRecorder.h"
#include "../broadcast/RTMPStreamer.h"
#include "../core/AudioBuffer.h"
#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/Clip.h"
#include "../dsp/DspChain.h"

#include <QDebug>
#include <QThreadPool>
#include <algorithm>
#include <cstring>

namespace dawcast {

// Position poll interval -- 30 ms gives smooth playhead motion without
// overwhelming the GUI event loop.
static constexpr int kPositionPollMs = 30;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

PlaybackEngine::PlaybackEngine(QObject* parent)
    : QObject(parent)
{
    m_metronome = new Metronome(this);

    m_positionTimer = new QTimer(this);
    m_positionTimer->setTimerType(Qt::PreciseTimer);
    connect(m_positionTimer, &QTimer::timeout,
            this, &PlaybackEngine::onPositionTimer);
}

PlaybackEngine::~PlaybackEngine()
{
    stop();

    for (auto& tp : m_tracks) {
        for (auto* reader : tp.readers) {
            delete reader;
        }
    }
    m_tracks.clear();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void PlaybackEngine::setTimeline(Timeline* timeline)
{
    if (m_timeline == timeline) return;

    if (m_playing.load(std::memory_order_acquire)) stop();

    m_timeline = timeline;

    if (m_timeline) {
        connect(m_timeline, &Timeline::trackAdded,
                this, [this]() { m_needsRebuild = true; });
        connect(m_timeline, &Timeline::trackRemoved,
                this, [this]() { m_needsRebuild = true; });
    }

    m_needsRebuild = true;
    rebuildReaders();
}

void PlaybackEngine::setAudioEngine(AudioEngine* engine)
{
    m_audioEngine = engine;
    if (engine) {
        m_mixer = engine->mixer();
    }
}

void PlaybackEngine::setRecorder(MultitrackRecorder* recorder)
{
    m_recorder = recorder;
}

void PlaybackEngine::setRTMPStreamer(RTMPStreamer* streamer)
{
    m_rtmpStreamer = streamer;
}

void PlaybackEngine::setBusRouter(BusRouter* router)
{
    m_busRouter = router;
}

// ---------------------------------------------------------------------------
// Transport controls (GUI thread)
// ---------------------------------------------------------------------------

void PlaybackEngine::play()
{
    if (!m_timeline || !m_audioEngine) {
        qWarning() << "PlaybackEngine::play: no timeline or audio engine set";
        return;
    }

    // Ensure the audio engine is running
    if (!m_audioEngine->isRunning()) {
        if (!m_audioEngine->start()) {
            qWarning() << "PlaybackEngine::play: failed to start audio engine";
            return;
        }
    }

    // If readers need rebuilding (clips changed while stopped), do it
    // in a worker thread so file decoding doesn't block the GUI.
    if (m_needsRebuild) {
        m_needsRebuild = false;
        QThreadPool::globalInstance()->start([this]() {
            // rebuildReaders decodes files — runs off the GUI thread.
            // It only touches m_tracks (not accessed by audio thread
            // while m_playing is false) and AudioClipReader (not a QObject).
            rebuildReaders();
            // Finish on the GUI thread: sync mixer strips (QObject methods)
            // and start playback.
            QMetaObject::invokeMethod(this, [this]() {
                syncMixerStrips();
                m_playing.store(true, std::memory_order_release);
                m_positionTimer->start(kPositionPollMs);
                emit playbackStarted();
            }, Qt::QueuedConnection);
        });
    } else {
        // Readers are current — start immediately
        rebuildReaders();
        syncMixerStrips();
        m_playing.store(true, std::memory_order_release);
        m_positionTimer->start(kPositionPollMs);
        emit playbackStarted();
    }
}

void PlaybackEngine::pause()
{
    m_playing.store(false, std::memory_order_release);
    m_positionTimer->stop();

    emit playbackStopped();
}

void PlaybackEngine::stop()
{
    const bool wasPlaying = m_playing.load(std::memory_order_acquire);
    m_playing.store(false, std::memory_order_release);
    m_positionTimer->stop();

    if (wasPlaying) {
        // First stop: halt playback but keep playhead where it is
        m_stoppedAtPosition = true;
    } else if (m_stoppedAtPosition) {
        // Second stop (already stopped): return to zero
        m_stoppedAtPosition = false;
        m_playheadPos.store(0, std::memory_order_release);
        if (m_timeline) {
            m_timeline->setPlayhead(0);
        }
        emit positionChanged(0);
    }

    emit playbackStopped();
}

void PlaybackEngine::seekTo(int64_t samplePosition)
{
    m_playheadPos.store(std::max(int64_t(0), samplePosition),
                        std::memory_order_release);
}

bool PlaybackEngine::isPlaying() const
{
    return m_playing.load(std::memory_order_acquire);
}

int64_t PlaybackEngine::currentPosition() const
{
    return m_playheadPos.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// Audio thread -- processBlock (RT-safe)
// ---------------------------------------------------------------------------

void PlaybackEngine::processBlock(int frames, int channels,
                                  const float* input, int inputChannels)
{
    // Forward input to the multitrack recorder if it is recording.
    // This must happen regardless of play state so punch-in works.
    if (m_recorder && input && inputChannels > 0) {
        // Provide current playhead position for punch-in/out evaluation
        m_recorder->setCurrentPosition(m_playheadPos.load(std::memory_order_acquire));
        m_recorder->processInputBlock(input, frames, inputChannels);
    }

    // If not playing, leave everything silent -- the mixer strips have
    // no input buffers set, so mixer->process() will output silence.
    if (!m_playing.load(std::memory_order_acquire)) {
        // Clear all mixer strip inputs
        for (auto& tp : m_tracks) {
            if (tp.mixerStrip >= 0 && m_mixer) {
                m_mixer->setStripBuffer(tp.mixerStrip, nullptr);
            }
        }
        return;
    }

    const int64_t pos = m_playheadPos.load(std::memory_order_acquire);

    // Ensure per-track buffers are allocated for this block size.
    // This only reallocates on the very first call or if buffer size changes.
    ensureTrackBuffers(frames, channels);

    // Clear all bus buffers for this block
    if (m_busRouter) {
        m_busRouter->clearAll(frames, channels);
    }

    const int totalSamples = frames * channels;

    // Process each audio track: read clips, apply DSP, feed mixer strip
    for (auto& tp : m_tracks) {
        if (tp.mixerStrip < 0 || !m_mixer) continue;

        // Zero this track's scratch buffer
        std::memset(tp.buffer.data(), 0,
                    static_cast<size_t>(totalSamples) * sizeof(float));

        // Accumulate all overlapping clips into the track buffer
        bool hasAudio = false;
        for (auto* reader : tp.readers) {
            if (!reader || !reader->isOpen()) continue;

            Clip* clip = reader->clip();
            if (!clip) continue;

            // Quick overlap test: [pos, pos+frames) vs [clipStart, clipEnd)
            int64_t clipStart = clip->timelinePosition();
            int64_t clipEnd   = clip->endPosition();
            if (pos + frames <= clipStart || pos >= clipEnd) continue;

            // readSamples adds (not overwrites) into the buffer
            reader->readSamples(tp.buffer.data(), pos, frames, channels);
            hasAudio = true;
        }

        if (!hasAudio) {
            m_mixer->setStripBuffer(tp.mixerStrip, nullptr);
            continue;
        }

        // Apply the track's DspChain if present and has effects.
        // DspChain::process is RT-safe (no alloc, no locks).
        // Use the const accessor to avoid lazy allocation on the audio thread.
        if (tp.audioTrack) {
            const auto* constTrack =
                static_cast<const AudioTrack*>(tp.audioTrack);
            const DspChain* chain = constTrack->effectChain();
            if (chain && chain->effectCount() > 0) {
                // process() modifies the float* buffer in-place and is RT-safe.
                const_cast<DspChain*>(chain)->process(
                    tp.buffer.data(), frames, channels);
            }
        }

        // Route through bus system if available
        if (m_busRouter) {
            m_busRouter->routeTrack(static_cast<int>(&tp - m_tracks.data()),
                                    tp.buffer.data(), frames, channels);
        }

        // Set up the AudioBuffer struct pointing to this track's data
        tp.audioBuf.data       = tp.buffer.data();
        tp.audioBuf.frames     = frames;
        tp.audioBuf.channels   = channels;
        tp.audioBuf.sampleRate = m_audioEngine ? m_audioEngine->sampleRate() : 48000;

        // Point the mixer strip to this track's buffer
        m_mixer->setStripBuffer(tp.mixerStrip, &tp.audioBuf);
    }

    // ── Bus routing: process all buses after track audio is routed ──
    if (m_busRouter) {
        m_busRouter->processAll(frames, channels);
    }

    // ── Metronome: mix click track into the first active strip buffer ──
    // The metronome is mixed after all tracks are read and DSP-processed
    // but before the mixer sums to master output. We pick the first strip
    // that has audio, or if none, the first strip that exists.
    if (m_metronome && m_metronome->isEnabled() && !m_tracks.empty()) {
        int targetStrip = -1;
        for (size_t i = 0; i < m_tracks.size(); ++i) {
            if (m_tracks[i].audioBuf.data != nullptr) {
                targetStrip = static_cast<int>(i);
                break;
            }
        }
        if (targetStrip < 0) targetStrip = 0;

        auto& mtp = m_tracks[static_cast<size_t>(targetStrip)];
        if (mtp.buffer.size() >= static_cast<size_t>(totalSamples)) {
            if (mtp.audioBuf.data == nullptr) {
                std::memset(mtp.buffer.data(), 0,
                            static_cast<size_t>(totalSamples) * sizeof(float));
                mtp.audioBuf.data       = mtp.buffer.data();
                mtp.audioBuf.frames     = frames;
                mtp.audioBuf.channels   = channels;
                mtp.audioBuf.sampleRate =
                    m_audioEngine ? m_audioEngine->sampleRate() : 48000;
                if (mtp.mixerStrip >= 0 && m_mixer) {
                    m_mixer->setStripBuffer(mtp.mixerStrip, &mtp.audioBuf);
                }
            }
            int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
            m_metronome->generateClick(mtp.buffer.data(), frames, channels,
                                       sr, pos);
        }
    }

    // Advance the playhead. If loop is enabled and we've passed loopEnd,
    // wrap back to loopStart atomically on the audio thread.
    int64_t newPos = pos + frames;
    if (m_timeline) {
        bool loopEnabled = m_timeline->loopEnabled();
        int64_t loopStart = m_timeline->loopStart();
        int64_t loopEnd   = m_timeline->loopEnd();
        if (loopEnabled && loopEnd > loopStart && newPos >= loopEnd) {
            newPos = loopStart + (newPos - loopEnd);
        }
    }
    m_playheadPos.store(newPos, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// GUI thread -- position polling
// ---------------------------------------------------------------------------

void PlaybackEngine::onPositionTimer()
{
    int64_t pos = m_playheadPos.load(std::memory_order_acquire);

    if (m_timeline) {
        // Check if we've passed the end of the timeline
        int64_t dur = m_timeline->duration();
        if (dur > 0 && pos >= dur) {
            stop();
            return;
        }
        m_timeline->setPlayhead(pos);
    }

    emit positionChanged(pos);
}

// ---------------------------------------------------------------------------
// Reader management (GUI thread only)
// ---------------------------------------------------------------------------

void PlaybackEngine::rebuildReaders()
{
    // Clean up existing readers
    for (auto& tp : m_tracks) {
        for (auto* reader : tp.readers) {
            delete reader;
        }
    }
    m_tracks.clear();

    if (!m_timeline) return;

    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        QObject* trackObj = m_timeline->track(t);
        auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
        if (!audioTrack) continue;  // Skip video tracks

        TrackPlayback tp;
        tp.audioTrack = audioTrack;
        tp.mixerStrip = -1;  // Assigned in syncMixerStrips

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            Clip* clip = audioTrack->clip(c);
            if (!clip || clip->sourcePath().isEmpty()) continue;

            auto* reader = new AudioClipReader(clip);
            if (reader->open()) {
                tp.readers.push_back(reader);
            } else {
                qWarning() << "PlaybackEngine: failed to open reader for"
                           << clip->sourcePath();
                delete reader;
            }
        }

        m_tracks.push_back(std::move(tp));
    }

    // Force buffer reallocation on next processBlock
    m_allocFrames   = 0;
    m_allocChannels = 0;

    qDebug() << "PlaybackEngine::rebuildReaders:"
             << m_tracks.size() << "audio tracks prepared";
}

void PlaybackEngine::syncMixerStrips()
{
    if (!m_mixer || !m_timeline) return;

    for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i) {
        auto& tp = m_tracks[static_cast<size_t>(i)];

        // Add mixer strips as needed
        while (m_mixer->stripCount() <= i) {
            m_mixer->addStrip();
        }

        tp.mixerStrip = i;

        // Sync mixer strip with the track's current settings
        if (tp.audioTrack) {
            m_mixer->setStripVolume(i, tp.audioTrack->volumeDb());
            m_mixer->setStripPan(i,    tp.audioTrack->pan());
            m_mixer->setStripMuted(i,  tp.audioTrack->isMuted());
            m_mixer->setStripSolo(i,   tp.audioTrack->isSolo());
        }
    }
}

void PlaybackEngine::ensureTrackBuffers(int frames, int channels)
{
    if (frames == m_allocFrames && channels == m_allocChannels) return;

    const auto totalSamples = static_cast<size_t>(frames * channels);

    for (auto& tp : m_tracks) {
        tp.buffer.resize(totalSamples, 0.0f);
        // Update the AudioBuffer pointer in case the vector reallocated
        tp.audioBuf.data     = tp.buffer.data();
        tp.audioBuf.frames   = frames;
        tp.audioBuf.channels = channels;
    }

    m_allocFrames   = frames;
    m_allocChannels = channels;
}

} // namespace dawcast
