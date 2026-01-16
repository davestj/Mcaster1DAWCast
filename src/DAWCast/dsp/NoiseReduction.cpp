// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "NoiseReduction.h"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace dawcast {

// ── Construction ───────────────────────────────────────────────────────────────

NoiseReduction::NoiseReduction()
    : m_fft(kFFTSize)
{
    buildWindow();

    // Allocate buffers
    m_inputRing.resize(kFFTSize, 0.0f);
    m_outputAccum.resize(kFFTSize * 2, 0.0f);  // extra room for overlap

    m_fftReal.resize(kFFTSize);
    m_fftImag.resize(kFFTSize);
    m_fftMag.resize(kNumBins);
    m_fftPhase.resize(kNumBins);
    m_fftFrame.resize(kFFTSize);

    m_prevMag.resize(kNumBins, 0.0f);

    m_noiseProfile.resize(kNumBins, 0.0f);
    m_captureAccum.resize(kNumBins, 0.0f);

    // Start needing a full window before first FFT
    m_samplesUntilNextHop = kFFTSize;
    m_primed = false;
}

NoiseReduction::~NoiseReduction() = default;

void NoiseReduction::buildWindow()
{
    m_window.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i) {
        // Hann window: 0.5 * (1 - cos(2*pi*n / (N-1)))
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i
                                                / (kFFTSize - 1)));
    }
}

// ── Noise profile capture ──────────────────────────────────────────────────────

void NoiseReduction::startNoiseCapture()
{
    std::fill(m_captureAccum.begin(), m_captureAccum.end(), 0.0f);
    m_captureFrames = 0;
    m_capturing.store(true);
}

void NoiseReduction::stopNoiseCapture()
{
    m_capturing.store(false);

    if (m_captureFrames > 0) {
        float invFrames = 1.0f / static_cast<float>(m_captureFrames);
        for (int i = 0; i < kNumBins; ++i) {
            m_noiseProfile[i] = m_captureAccum[i] * invFrames;
        }
        m_hasProfile = true;
    }
}

bool NoiseReduction::hasNoiseProfile() const
{
    return m_hasProfile;
}

// ── Process (IEffectUnit) ──────────────────────────────────────────────────────

void NoiseReduction::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    for (int f = 0; f < frames; ++f) {
        // Sum to mono for analysis
        float mono = 0.0f;
        for (int c = 0; c < channels; ++c) {
            mono += buffer[f * channels + c];
        }
        mono /= static_cast<float>(channels);

        // Write into circular input buffer
        m_inputRing[m_inputWritePos] = mono;
        m_inputWritePos = (m_inputWritePos + 1) % kFFTSize;

        --m_samplesUntilNextHop;

        if (m_samplesUntilNextHop <= 0) {
            m_primed = true;
            processFrame();
            m_samplesUntilNextHop = kHopSize;
        }

        // Read from the output accumulator
        float processed = mono;  // pass-through until primed
        if (m_primed) {
            processed = m_outputAccum[m_outputReadPos];
            m_outputAccum[m_outputReadPos] = 0.0f;  // clear after reading
            m_outputReadPos = (m_outputReadPos + 1) % static_cast<int>(m_outputAccum.size());
        }

        // Apply residual mix: 0 = denoised, 1 = noise only
        float denoised = processed;
        float noise = mono - processed;
        float output = denoised * (1.0f - m_residualMix) + noise * m_residualMix;

        // Write back to all channels (same processing applied uniformly)
        for (int c = 0; c < channels; ++c) {
            float dry = buffer[f * channels + c];
            float chMono = dry;  // original channel signal

            // Scale the output relative to the channel's contribution
            if (std::fabs(mono) > 1e-10f) {
                buffer[f * channels + c] = output * (chMono / mono);
            } else {
                buffer[f * channels + c] = 0.0f;
            }
        }
    }
}

// ── Process one FFT frame ──────────────────────────────────────────────────────

void NoiseReduction::processFrame()
{
    // Extract the current FFT window from the circular input buffer
    int readStart = (m_inputWritePos - kFFTSize + static_cast<int>(m_inputRing.size()))
                    % static_cast<int>(m_inputRing.size());

    for (int i = 0; i < kFFTSize; ++i) {
        int idx = (readStart + i) % static_cast<int>(m_inputRing.size());
        m_fftFrame[i] = m_inputRing[idx] * m_window[i];
    }

    // Forward FFT
    m_fft.forward(m_fftFrame.data(), m_fftReal.data(), m_fftImag.data());

    // Compute magnitude and phase for positive-frequency bins
    for (int i = 0; i < kNumBins; ++i) {
        float re = m_fftReal[i];
        float im = m_fftImag[i];
        m_fftMag[i]   = std::sqrt(re * re + im * im);
        m_fftPhase[i] = std::atan2(im, re);
    }

    // If capturing noise profile, accumulate magnitudes
    if (m_capturing.load()) {
        for (int i = 0; i < kNumBins; ++i) {
            m_captureAccum[i] += m_fftMag[i];
        }
        ++m_captureFrames;
    }

    // Spectral subtraction
    if (m_hasProfile) {
        // Convert reduction_db to linear scale factor
        float reductionLinear = std::pow(10.0f, m_reductionDb / 20.0f);

        // Residual floor to prevent musical noise (never zero out a bin completely)
        float residualFloor = 0.01f;  // -40 dB floor

        // Temporal smoothing coefficient
        float alpha = m_smoothing * 0.95f;  // map 0..1 to 0..0.95

        for (int i = 0; i < kNumBins; ++i) {
            float mag = m_fftMag[i];
            float noiseMag = m_noiseProfile[i] * m_sensitivity * reductionLinear;

            // Spectral subtraction with residual floor
            float cleanMag = std::max(mag - noiseMag, mag * residualFloor);

            // Temporal smoothing to reduce musical noise artifacts
            cleanMag = alpha * m_prevMag[i] + (1.0f - alpha) * cleanMag;
            m_prevMag[i] = cleanMag;

            m_fftMag[i] = cleanMag;
        }
    }

    // Reconstruct complex spectrum from modified magnitude + original phase
    for (int i = 0; i < kNumBins; ++i) {
        m_fftReal[i] = m_fftMag[i] * std::cos(m_fftPhase[i]);
        m_fftImag[i] = m_fftMag[i] * std::sin(m_fftPhase[i]);
    }

    // Mirror the negative frequencies (conjugate symmetry for real signal)
    for (int i = 1; i < kFFTSize / 2; ++i) {
        m_fftReal[kFFTSize - i] =  m_fftReal[i];
        m_fftImag[kFFTSize - i] = -m_fftImag[i];
    }

    // Inverse FFT
    m_fft.inverse(m_fftReal.data(), m_fftImag.data(), m_fftFrame.data());

    // Window again (synthesis window) and overlap-add into output accumulator
    // The write position into the output accumulator is derived from the
    // output read position: we write kFFTSize samples starting at the current
    // "frame start" position.
    int writeStart = m_outputReadPos;
    int accumSize = static_cast<int>(m_outputAccum.size());

    for (int i = 0; i < kFFTSize; ++i) {
        int idx = (writeStart + i) % accumSize;
        m_outputAccum[idx] += m_fftFrame[i] * m_window[i];
    }
}

// ── Parameters ─────────────────────────────────────────────────────────────────

void NoiseReduction::setParameter(int id, float value)
{
    switch (id) {
    case ReductionDb:  m_reductionDb  = std::clamp(value, 0.0f, 40.0f); break;
    case Sensitivity:  m_sensitivity  = std::clamp(value, 0.0f, 1.0f);  break;
    case Smoothing:    m_smoothing    = std::clamp(value, 0.0f, 1.0f);  break;
    case ResidualMix:  m_residualMix  = std::clamp(value, 0.0f, 1.0f);  break;
    default: break;
    }
}

float NoiseReduction::parameter(int id) const
{
    switch (id) {
    case ReductionDb:  return m_reductionDb;
    case Sensitivity:  return m_sensitivity;
    case Smoothing:    return m_smoothing;
    case ResidualMix:  return m_residualMix;
    default:           return 0.0f;
    }
}

QString NoiseReduction::name() const
{
    return QStringLiteral("Noise Reduction");
}

int NoiseReduction::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
