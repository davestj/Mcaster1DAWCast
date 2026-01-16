// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TimeStretch.h"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace dawcast {

// ── Construction ───────────────────────────────────────────────────────────────

TimeStretch::TimeStretch(int sampleRate)
    : m_sampleRate(sampleRate)
    , m_fft(kFFTSize)
{
    // Pre-compute Hann window
    m_window.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i
                                                / (kFFTSize - 1)));
    }

    // Allocate working buffers
    m_fftReal.resize(kFFTSize);
    m_fftImag.resize(kFFTSize);
    m_lastPhase.resize(kNumBins, 0.0f);
    m_phaseAccum.resize(kNumBins, 0.0f);
    m_frameBuffer.resize(kFFTSize);
}

TimeStretch::~TimeStretch() = default;

// ── Public interface ───────────────────────────────────────────────────────────

AudioBuffer TimeStretch::stretch(const AudioBuffer& input, float ratio)
{
    if (!input.data || input.frames <= 0 || input.channels <= 0 || ratio <= 0.0f) {
        AudioBuffer out{};
        return out;
    }

    int channels = input.channels;

    // Process each channel independently through the phase vocoder
    std::vector<std::vector<float>> channelResults(channels);
    int maxFrames = 0;

    for (int c = 0; c < channels; ++c) {
        // De-interleave channel
        std::vector<float> mono(input.frames);
        for (int i = 0; i < input.frames; ++i) {
            mono[i] = input.data[i * channels + c];
        }

        channelResults[c] = stretchMono(mono.data(), input.frames, ratio);
        maxFrames = std::max(maxFrames, static_cast<int>(channelResults[c].size()));
    }

    // Interleave results
    AudioBuffer out{};
    out.frames = maxFrames;
    out.channels = channels;
    out.sampleRate = input.sampleRate;
    out.data = new float[out.sampleCount()];
    std::memset(out.data, 0, static_cast<size_t>(out.sampleCount()) * sizeof(float));

    for (int c = 0; c < channels; ++c) {
        int chFrames = static_cast<int>(channelResults[c].size());
        for (int i = 0; i < chFrames; ++i) {
            out.data[i * channels + c] = channelResults[c][i];
        }
    }

    return out;
}

AudioBuffer TimeStretch::pitchShift(const AudioBuffer& input, float semitones)
{
    if (!input.data || input.frames <= 0 || input.channels <= 0) {
        AudioBuffer out{};
        return out;
    }

    // Pitch ratio: positive semitones = higher pitch = shorter when stretched
    float pitchRatio = std::pow(2.0f, semitones / 12.0f);

    // Step 1: Time-stretch by 1/pitchRatio (so the resampling brings it back
    //         to original length but at the new pitch)
    AudioBuffer stretched = stretch(input, 1.0f / pitchRatio);

    if (!stretched.data || stretched.frames <= 0) {
        return stretched;
    }

    int channels = stretched.channels;

    // Step 2: Resample by pitchRatio to restore original duration
    std::vector<std::vector<float>> channelResults(channels);
    int targetFrames = input.frames;  // want same length as original

    for (int c = 0; c < channels; ++c) {
        // De-interleave
        std::vector<float> mono(stretched.frames);
        for (int i = 0; i < stretched.frames; ++i) {
            mono[i] = stretched.data[i * channels + c];
        }

        channelResults[c] = resampleLinear(mono.data(), stretched.frames, pitchRatio);

        // Trim or pad to target length
        channelResults[c].resize(targetFrames, 0.0f);
    }

    // Clean up stretched intermediate
    delete[] stretched.data;

    // Interleave
    AudioBuffer out{};
    out.frames = targetFrames;
    out.channels = channels;
    out.sampleRate = input.sampleRate;
    out.data = new float[out.sampleCount()];

    for (int c = 0; c < channels; ++c) {
        for (int i = 0; i < targetFrames; ++i) {
            out.data[i * channels + c] = channelResults[c][i];
        }
    }

    return out;
}

// ── Phase vocoder core ─────────────────────────────────────────────────────────

std::vector<float> TimeStretch::stretchMono(const float* input, int inputFrames, float ratio)
{
    // Analysis hop = kHopSize (fixed)
    // Synthesis hop = kHopSize * ratio (for time stretching)
    // ratio > 1 = compress (speed up), ratio < 1 = expand (slow down)

    int analysisHop = kHopSize;
    float synthesisHop = static_cast<float>(kHopSize) * ratio;

    // Estimate output length
    int numAnalysisFrames = std::max(1, (inputFrames - kFFTSize) / analysisHop + 1);
    int outputLength = static_cast<int>(
        static_cast<float>(numAnalysisFrames) * synthesisHop + kFFTSize);
    outputLength = std::max(outputLength, kFFTSize);

    std::vector<float> output(outputLength, 0.0f);

    // Reset phase state
    std::fill(m_lastPhase.begin(), m_lastPhase.end(), 0.0f);
    std::fill(m_phaseAccum.begin(), m_phaseAccum.end(), 0.0f);

    float expectedPhaseDiff = 2.0f * static_cast<float>(M_PI)
                              * static_cast<float>(analysisHop)
                              / static_cast<float>(kFFTSize);

    float synthPos = 0.0f;

    for (int analysisPos = 0;
         analysisPos + kFFTSize <= inputFrames;
         analysisPos += analysisHop)
    {
        // Window the analysis frame
        for (int i = 0; i < kFFTSize; ++i) {
            m_frameBuffer[i] = input[analysisPos + i] * m_window[i];
        }

        // Forward FFT
        m_fft.forward(m_frameBuffer.data(), m_fftReal.data(), m_fftImag.data());

        // Process each frequency bin: compute magnitude + corrected phase
        for (int bin = 0; bin < kNumBins; ++bin) {
            float re = m_fftReal[bin];
            float im = m_fftImag[bin];
            float mag = std::sqrt(re * re + im * im);
            float phase = std::atan2(im, re);

            // Phase difference from last frame
            float phaseDiff = phase - m_lastPhase[bin];
            m_lastPhase[bin] = phase;

            // Remove expected phase advance
            phaseDiff -= static_cast<float>(bin) * expectedPhaseDiff;

            // Wrap to [-pi, pi]
            phaseDiff = std::fmod(phaseDiff + static_cast<float>(M_PI),
                                  2.0f * static_cast<float>(M_PI));
            if (phaseDiff < 0.0f) phaseDiff += 2.0f * static_cast<float>(M_PI);
            phaseDiff -= static_cast<float>(M_PI);

            // True frequency deviation
            float trueFreq = static_cast<float>(bin) * expectedPhaseDiff + phaseDiff;

            // Accumulate phase for synthesis (scaled by synthesis/analysis hop ratio)
            m_phaseAccum[bin] += trueFreq * (synthesisHop / static_cast<float>(analysisHop));

            // Reconstruct with new phase
            m_fftReal[bin] = mag * std::cos(m_phaseAccum[bin]);
            m_fftImag[bin] = mag * std::sin(m_phaseAccum[bin]);
        }

        // Mirror negative frequencies
        for (int i = 1; i < kFFTSize / 2; ++i) {
            m_fftReal[kFFTSize - i] =  m_fftReal[i];
            m_fftImag[kFFTSize - i] = -m_fftImag[i];
        }

        // Inverse FFT
        m_fft.inverse(m_fftReal.data(), m_fftImag.data(), m_frameBuffer.data());

        // Window again (synthesis) and overlap-add
        int synthStart = static_cast<int>(synthPos);
        for (int i = 0; i < kFFTSize; ++i) {
            int outIdx = synthStart + i;
            if (outIdx >= 0 && outIdx < outputLength) {
                output[outIdx] += m_frameBuffer[i] * m_window[i];
            }
        }

        synthPos += synthesisHop;
    }

    // Normalize by the overlap-add gain factor.
    // With 75% overlap and Hann window, the constant overlap-add gain
    // is approximately sum(window^2) * hopSize / FFTSize.  For Hann with
    // 75% overlap this works out to roughly 1.5.  We use a simple scaling.
    float gain = 2.0f / 3.0f;  // reciprocal of ~1.5
    for (float& s : output) {
        s *= gain;
    }

    // Trim trailing silence
    int actualLength = outputLength;
    while (actualLength > 0 && std::fabs(output[actualLength - 1]) < 1e-8f) {
        --actualLength;
    }
    output.resize(std::max(1, actualLength));

    return output;
}

// ── Linear interpolation resampler ─────────────────────────────────────────────

std::vector<float> TimeStretch::resampleLinear(const float* input, int inputFrames, float ratio)
{
    if (inputFrames <= 0 || ratio <= 0.0f) return {};

    int outputFrames = static_cast<int>(static_cast<float>(inputFrames) / ratio);
    outputFrames = std::max(1, outputFrames);

    std::vector<float> output(outputFrames);

    for (int i = 0; i < outputFrames; ++i) {
        float srcPos = static_cast<float>(i) * ratio;
        int srcIdx = static_cast<int>(srcPos);
        float frac = srcPos - static_cast<float>(srcIdx);

        if (srcIdx + 1 < inputFrames) {
            output[i] = input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac;
        } else if (srcIdx < inputFrames) {
            output[i] = input[srcIdx];
        } else {
            output[i] = 0.0f;
        }
    }

    return output;
}

} // namespace dawcast
