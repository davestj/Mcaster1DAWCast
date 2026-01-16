// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <atomic>
#include <vector>
#include <cstdint>

namespace dawcast {

/// Real-time metronome / click track generator.
///
/// The Metronome pre-generates short click sounds (sine bursts with
/// exponential decay) and mixes them into the audio output buffer
/// at beat boundaries. generateClick() is RT-safe and called from
/// the PortAudio audio thread.
class Metronome : public QObject
{
    Q_OBJECT

public:
    explicit Metronome(QObject* parent = nullptr);
    ~Metronome() override;

    void setTempo(double bpm);                               // 20-300 BPM
    void setTimeSignature(int numerator, int denominator);   // e.g. 4/4, 3/4, 6/8
    void setVolume(float db);                                // -60 to 0 dB
    void setEnabled(bool enabled);
    void setCountIn(int bars);                               // Pre-roll count-in (0 = disabled)

    [[nodiscard]] bool   isEnabled()     const;
    [[nodiscard]] double tempo()         const;
    [[nodiscard]] int    numerator()     const { return m_numerator; }
    [[nodiscard]] int    denominator()   const { return m_denominator; }
    [[nodiscard]] float  volumeDb()      const;
    [[nodiscard]] int    countInBars()   const { return m_countInBars; }

    /// Called from the audio callback (RT-safe).
    /// Mixes click samples into the output buffer at beat boundaries.
    /// @param buffer     interleaved float audio buffer
    /// @param frames     number of frames in the buffer
    /// @param channels   number of channels (interleaved)
    /// @param sampleRate audio sample rate
    /// @param playheadSamples  current playhead position in samples
    void generateClick(float* buffer, int frames, int channels,
                       int sampleRate, int64_t playheadSamples);

signals:
    /// Emitted on the audio thread at each beat boundary.
    /// Connect with Qt::QueuedConnection for GUI updates.
    void beat(int beatNumber, bool isDownbeat);

private:
    /// Pre-generate the downbeat and regular click waveforms.
    void generateClickSamples(int sampleRate);

    // Tempo and time signature (written from GUI, read from audio thread)
    std::atomic<double> m_bpm{120.0};
    int m_numerator   = 4;
    int m_denominator = 4;

    // Volume
    std::atomic<float> m_volumeLinear{0.5f};
    float              m_volumeDb = -6.0f;

    // State
    std::atomic<bool> m_enabled{false};
    int               m_countInBars = 0;

    // Pre-generated click sounds (mono, to be mixed into each channel)
    std::vector<float> m_downbeatClick;  // 880 Hz, 20 ms, exponential decay
    std::vector<float> m_beatClick;      // 440 Hz, 15 ms, exponential decay
    int                m_clickSampleRate = 0; // sample rate used to generate clicks
};

} // namespace dawcast
