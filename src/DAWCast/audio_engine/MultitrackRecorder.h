// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QFile>
#include <atomic>
#include <cstdint>

namespace dawcast {

class AudioTrack;
class AudioEngine;
class Timeline;
class Clip;

/// Records audio input into temporary WAV files on armed timeline tracks.
///
/// Thread safety:
///   - processInputBlock() is called from the PortAudio audio thread.
///   - startRecording / stopRecording are called from the GUI thread.
///   - m_recording is shared via std::atomic<bool>.
class MultitrackRecorder : public QObject
{
    Q_OBJECT

public:
    explicit MultitrackRecorder(QObject* parent = nullptr);
    ~MultitrackRecorder() override;

    /// A single recording target: one armed track, one input channel pair,
    /// one temporary WAV file.
    struct RecordTarget {
        AudioTrack* track        = nullptr;  ///< Timeline track to record to
        int         inputChannel = -1;       ///< Input channel (0-based, -1 = stereo pair)
        QString     tempFilePath;            ///< Temporary WAV file path
    };

    void addTarget(const RecordTarget& target);
    void clearTargets();

    void setTimeline(Timeline* timeline);
    void setAudioEngine(AudioEngine* engine);

    /// Start recording on all configured targets.
    /// Opens temp WAV files and sets m_recording = true.
    void startRecording();

    /// Stop recording, finalize WAV files, and create Clips on each track.
    void stopRecording();

    [[nodiscard]] bool isRecording() const;

    /// Called from the audio thread -- writes input samples to temp files.
    /// @param input      Interleaved float32 input buffer from PortAudio
    /// @param frames     Number of frames in the buffer
    /// @param inputChannels  Number of interleaved channels in the input buffer
    void processInputBlock(const float* input, int frames, int inputChannels);

    /// Set the current playhead position so punch-in/out can be evaluated.
    /// Called from the audio thread (PlaybackEngine::processBlock).
    void setCurrentPosition(int64_t samples) { m_currentPosition = samples; }

    /// Number of active recording targets (meter rows).
    [[nodiscard]] int targetCount() const { return m_targets.size(); }

    /// Lock-free read of the last-seen peak for target `i`. Safe to call
    /// from the GUI thread every frame — values decay naturally as the
    /// audio thread overwrites them each block.
    [[nodiscard]] float peakL(int i) const {
        return (i >= 0 && i < m_peaks.size())
            ? m_peaks[i]->peakL.load(std::memory_order_acquire) : 0.0f;
    }
    [[nodiscard]] float peakR(int i) const {
        return (i >= 0 && i < m_peaks.size())
            ? m_peaks[i]->peakR.load(std::memory_order_acquire) : 0.0f;
    }

signals:
    void recordingStarted();
    void recordingStopped();
    /// Emitted periodically with peak level for a recording target.
    void levelUpdate(int trackIndex, float peakL, float peakR);

private:
    /// Write a WAV file header with placeholder data size.
    void writeWavHeader(QFile* file, int channels, int sampleRate);

    /// Finalize a WAV file header with the correct data size.
    void finalizeWavHeader(QFile* file);

    /// Create a Clip from a completed recording and add it to the track.
    void createClipFromRecording(const RecordTarget& target, int64_t durationSamples);

    QList<RecordTarget> m_targets;
    Timeline*           m_timeline = nullptr;
    AudioEngine*        m_engine   = nullptr;

    std::atomic<bool> m_recording{false};
    int64_t           m_recordStartPosition = 0;
    int64_t           m_samplesRecorded     = 0;
    int64_t           m_currentPosition     = 0;  ///< Set from audio thread for punch I/O

    /// One open QFile per target, in the same order as m_targets.
    QList<QFile*> m_tempFiles;

    /// Per-target peak accumulators (written on audio thread, read on GUI).
    struct PeakData {
        std::atomic<float> peakL{0.0f};
        std::atomic<float> peakR{0.0f};
    };
    QList<PeakData*> m_peaks;
};

} // namespace dawcast
