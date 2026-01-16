// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"
#include "FFT.h"

#include <vector>
#include <atomic>

namespace dawcast {

/// Spectral subtraction noise reduction effect.
///
/// Captures a noise floor profile from a section of audio, then subtracts
/// the noise spectrum from the signal in real time.  Uses a 2048-point FFT
/// with 75% overlap (hop size 512) and Hann windowing.
///
/// Parameters:
///   0 — ReductionDb   (0..40 dB) how much to reduce noise
///   1 — Sensitivity   (0..1)     how aggressively to detect noise
///   2 — Smoothing     (0..1)     temporal smoothing to avoid musical noise
///   3 — ResidualMix   (0..1)     0 = fully denoised, 1 = noise only (monitor)

class NoiseReduction : public IEffectUnit
{
public:
    enum Param
    {
        ReductionDb = 0,
        Sensitivity,
        Smoothing,
        ResidualMix,
        ParamCount
    };

    NoiseReduction();
    ~NoiseReduction() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

    // Noise profile capture
    void startNoiseCapture();
    void stopNoiseCapture();
    bool hasNoiseProfile() const;

private:
    // FFT parameters
    static constexpr int kFFTSize  = 2048;
    static constexpr int kHopSize  = 512;   // 75% overlap
    static constexpr int kNumBins  = kFFTSize / 2 + 1;

    // Build Hann window
    void buildWindow();

    // Process one FFT frame (mono)
    void processFrame();

    // User-facing parameters
    float m_reductionDb  = 20.0f;   // 0..40
    float m_sensitivity  = 0.5f;    // 0..1
    float m_smoothing    = 0.5f;    // 0..1
    float m_residualMix  = 0.0f;    // 0 = clean, 1 = noise only

    // FFT engine
    FFT m_fft;

    // Hann window
    std::vector<float> m_window;

    // Overlap-add buffers (mono, summed from multi-channel input)
    std::vector<float> m_inputRing;     // circular input buffer
    int                m_inputWritePos = 0;
    std::vector<float> m_outputAccum;   // overlap-add output accumulator
    int                m_outputReadPos = 0;
    int                m_samplesUntilNextHop = 0;

    // Working FFT buffers
    std::vector<float> m_fftReal;
    std::vector<float> m_fftImag;
    std::vector<float> m_fftMag;
    std::vector<float> m_fftPhase;
    std::vector<float> m_fftFrame;     // windowed time-domain frame

    // Temporal smoothing state
    std::vector<float> m_prevMag;

    // Noise profile
    std::vector<float> m_noiseProfile;  // average magnitude per bin
    bool               m_hasProfile = false;

    // Capture state
    std::atomic<bool>  m_capturing{false};
    std::vector<float> m_captureAccum;  // accumulator for averaging
    int                m_captureFrames = 0;

    // Latency tracking
    bool m_primed = false;  // have we filled the initial FFT window?
};

} // namespace dawcast
