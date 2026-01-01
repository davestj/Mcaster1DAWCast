// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Normalizer.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

Normalizer::Normalizer()
{
    initKWeighting();
    m_blockSize = static_cast<int>(0.4f * m_sampleRate);  // 400ms blocks
    m_gainSmoothCoeff = std::exp(-1.0f / (0.1f * m_sampleRate));  // ~100ms smoothing
}

Normalizer::~Normalizer() = default;

void Normalizer::initKWeighting()
{
    // ---------------------------------------------------------------
    // ITU-R BS.1770-4 K-weighting filter coefficients.
    //
    // Stage 1: Pre-filter (shelving) — +4 dB high shelf at ~1681 Hz
    // These are the exact coefficients from the ITU spec for 48 kHz.
    // For other sample rates, we use the RBJ high-shelf with
    // equivalent parameters.
    //
    // Stage 2: RLB (Revised Low-frequency B-curve) — high-pass ~38 Hz
    // Also given as exact coefficients for 48 kHz in the spec.
    // ---------------------------------------------------------------

    BiquadCoeffs preCoeffs;
    BiquadCoeffs rlbCoeffs;

    if (std::abs(m_sampleRate - 48000.0f) < 1.0f) {
        // Exact ITU-R BS.1770-4 coefficients for 48 kHz
        // Stage 1: Pre-filter
        preCoeffs.b0 =  1.53512485958697;
        preCoeffs.b1 = -2.69169618940638;
        preCoeffs.b2 =  1.19839281085285;
        preCoeffs.a1 = -1.69065929318241;
        preCoeffs.a2 =  0.73248077421585;

        // Stage 2: RLB high-pass
        rlbCoeffs.b0 =  1.0;
        rlbCoeffs.b1 = -2.0;
        rlbCoeffs.b2 =  1.0;
        rlbCoeffs.a1 = -1.99004745483398;
        rlbCoeffs.a2 =  0.99007225036621;
    } else {
        // Generic approximation via RBJ cookbook for non-48k rates.
        // Stage 1: High shelf, +4 dB at 1681 Hz, Q=0.7071
        preCoeffs = Biquad::highshelf(1681.0f, 0.7071f, 4.0f, m_sampleRate);

        // Stage 2: High-pass at 38 Hz, Q=0.5 (gentle roll-off)
        rlbCoeffs = Biquad::highpass(38.0f, 0.5f, m_sampleRate);
    }

    for (int c = 0; c < MaxChannels; ++c) {
        m_preFilter[c].setCoeffs(preCoeffs);
        m_preFilter[c].reset();
        m_rlbFilter[c].setCoeffs(rlbCoeffs);
        m_rlbFilter[c].reset();
    }
}

void Normalizer::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const int ch = std::min(channels, MaxChannels);
    m_blockChannels = ch;

    for (int f = 0; f < frames; ++f) {
        // --- K-weighting: filter each channel through pre-filter + RLB ---
        float frameSumSq = 0.0f;
        for (int c = 0; c < ch; ++c) {
            float s = buffer[f * channels + c];

            // Stage 1: Pre-filter (high shelf)
            float kw = m_preFilter[c].process(s);
            // Stage 2: RLB (high-pass)
            kw = m_rlbFilter[c].process(kw);

            frameSumSq += kw * kw;
        }

        // Accumulate K-weighted mean-square for the current 400ms block
        m_blockSumSq += static_cast<double>(frameSumSq) / static_cast<double>(ch);
        m_blockPos++;

        // --- End of 400ms block: compute block loudness, perform gating ---
        if (m_blockPos >= m_blockSize) {
            double blockMeanSq = m_blockSumSq / static_cast<double>(m_blockSize);

            // Store in ring buffer
            m_blockLoudness[m_blockWriteIdx] = blockMeanSq;
            m_blockWriteIdx = (m_blockWriteIdx + 1) % MaxBlocks;
            if (m_blockCount < MaxBlocks) m_blockCount++;

            // Reset accumulator
            m_blockSumSq = 0.0;
            m_blockPos = 0;

            // --- EBU R128 Gated Loudness Measurement ---
            // Pass 1: Absolute gate at -70 LUFS
            double absGateThresh = std::pow(10.0, (-70.0 + 0.691) / 10.0);
            double sumAbove = 0.0;
            int    countAbove = 0;

            for (int i = 0; i < m_blockCount; ++i) {
                if (m_blockLoudness[i] >= absGateThresh) {
                    sumAbove += m_blockLoudness[i];
                    countAbove++;
                }
            }

            if (countAbove > 0) {
                double absGateMeanSq = sumAbove / static_cast<double>(countAbove);
                double absGateLufs = -0.691 + 10.0 * std::log10(absGateMeanSq);

                // Pass 2: Relative gate at -10 LU below absolute-gated loudness
                double relGateThresh = std::pow(10.0, (absGateLufs - 10.0 + 0.691) / 10.0);
                double sumRel = 0.0;
                int    countRel = 0;

                for (int i = 0; i < m_blockCount; ++i) {
                    if (m_blockLoudness[i] >= relGateThresh) {
                        sumRel += m_blockLoudness[i];
                        countRel++;
                    }
                }

                if (countRel > 0) {
                    double gatedMeanSq = sumRel / static_cast<double>(countRel);
                    m_integratedLoudness = -0.691 + 10.0 * std::log10(gatedMeanSq);
                } else {
                    m_integratedLoudness = -120.0;
                }
            } else {
                m_integratedLoudness = -120.0;
            }

            m_currentLufs = static_cast<float>(m_integratedLoudness);

            // Compute makeup gain: target_lufs - measured_lufs
            m_makeupGainDb = m_targetLufs - m_currentLufs;
            m_makeupGainDb = std::clamp(m_makeupGainDb, -30.0f, 30.0f);
        }

        // --- Apply smoothed makeup gain ---
        float targetGainLin = std::pow(10.0f, m_makeupGainDb / 20.0f);
        m_smoothedGainLin = m_gainSmoothCoeff * m_smoothedGainLin
                          + (1.0f - m_gainSmoothCoeff) * targetGainLin;

        for (int c = 0; c < ch; ++c) {
            buffer[f * channels + c] *= m_smoothedGainLin;
        }
    }
}

void Normalizer::setParameter(int id, float value)
{
    switch (id) {
    case TargetLufs:      m_targetLufs = value; break;
    case MeasureStandard: m_standard   = static_cast<Standard>(static_cast<int>(value)); break;
    default: break;
    }
}

float Normalizer::parameter(int id) const
{
    switch (id) {
    case TargetLufs:      return m_targetLufs;
    case MeasureStandard: return static_cast<float>(m_standard);
    default:              return 0.0f;
    }
}

QString Normalizer::name() const
{
    return QStringLiteral("Loudness Normalizer");
}

int Normalizer::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
