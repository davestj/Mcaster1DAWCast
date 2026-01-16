// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/AudioBuffer.h"
#include "FFT.h"

#include <vector>

namespace dawcast {

/// Phase vocoder-based time stretch and pitch shift.
///
/// Time stretch changes duration without changing pitch.
/// Pitch shift changes pitch without changing duration.
///
/// Uses a 2048-point FFT with 75% overlap for analysis, with proper
/// phase accumulation to maintain phase coherence across frames.

class TimeStretch
{
public:
    explicit TimeStretch(int sampleRate = 48000);
    ~TimeStretch();

    /// Time stretch: change duration without changing pitch.
    /// ratio: 0.5 = half speed (2x duration), 2.0 = double speed (0.5x duration)
    /// Returns a newly allocated AudioBuffer; caller owns the data pointer.
    AudioBuffer stretch(const AudioBuffer& input, float ratio);

    /// Pitch shift: change pitch without changing duration.
    /// semitones: -12 to +12 (negative = lower, positive = higher)
    /// Returns a newly allocated AudioBuffer; caller owns the data pointer.
    AudioBuffer pitchShift(const AudioBuffer& input, float semitones);

private:
    static constexpr int kFFTSize = 2048;
    static constexpr int kHopSize = 512;   // 75% overlap (analysis hop)
    static constexpr int kNumBins = kFFTSize / 2 + 1;

    /// Phase vocoder core: stretch a single-channel signal.
    /// Returns the stretched samples in a vector.
    std::vector<float> stretchMono(const float* input, int inputFrames, float ratio);

    /// Simple linear interpolation resampler for pitch shift.
    std::vector<float> resampleLinear(const float* input, int inputFrames, float ratio);

    int m_sampleRate;
    FFT m_fft;

    // Hann window (pre-computed)
    std::vector<float> m_window;

    // Working buffers for the phase vocoder
    std::vector<float> m_fftReal;
    std::vector<float> m_fftImag;
    std::vector<float> m_lastPhase;     // phase from previous analysis frame
    std::vector<float> m_phaseAccum;    // accumulated synthesis phase
    std::vector<float> m_frameBuffer;   // windowed time-domain frame
};

} // namespace dawcast
