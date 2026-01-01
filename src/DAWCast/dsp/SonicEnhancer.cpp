// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SonicEnhancer.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

SonicEnhancer::SonicEnhancer()
{
    recalcFilters();
    // Zero-initialize delay lines
    for (int c = 0; c < MaxChannels; ++c) {
        m_lowDelay[c].fill(0.0f);
        m_midDelay[c].fill(0.0f);
        m_lowDelayPos[c] = 0;
        m_midDelayPos[c] = 0;
    }
}

SonicEnhancer::~SonicEnhancer() = default;

void SonicEnhancer::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const int ch = std::min(channels, MaxChannels);

    // Convert 0..100 parameters to gain multipliers
    const float lowGain  = 1.0f + (m_lowContour / 100.0f) * 0.5f;    // up to +6 dB
    const float highGain = 1.0f + (m_presence   / 100.0f) * 0.5f;    // up to +6 dB

    // Harmonic exciter drive: processAmount controls the amount of soft-clipping harmonics
    // added to the high band. At 0 = no harmonics, at 100 = full saturation blend.
    const float exciterDrive  = 1.0f + (m_processAmount / 100.0f) * 3.0f; // 1..4x drive
    const float exciterMix    = m_processAmount / 100.0f;                  // 0..1 blend

    for (int f = 0; f < frames; ++f) {
        for (int c = 0; c < ch; ++c) {
            float input = buffer[f * channels + c];

            // --- Linkwitz-Riley 4th-order crossover ---

            // LR4 low-pass: cascaded pair
            float low = m_lp1a[c].process(input);
            low = m_lp1b[c].process(low);

            // LR4 high-pass (mid + high)
            float midHigh = m_hp1a[c].process(input);
            midHigh = m_hp1b[c].process(midHigh);

            // Split mid/high via second crossover
            float mid = m_lp2a[c].process(midHigh);
            mid = m_lp2b[c].process(mid);

            float high = m_hp2a[c].process(midHigh);
            high = m_hp2b[c].process(high);

            // --- Time-alignment delay lines ---
            // Low band gets the most delay (crossover introduces group delay
            // that is greater in the high-pass path; compensate by delaying
            // the low-pass output).

            // Low band delay
            int& ldp = m_lowDelayPos[c];
            float delayedLow = m_lowDelay[c][ldp];
            m_lowDelay[c][ldp] = low;
            ldp = (ldp + 1) % LowDelaySamples;

            // Mid band delay
            int& mdp = m_midDelayPos[c];
            float delayedMid = m_midDelay[c][mdp];
            m_midDelay[c][mdp] = mid;
            mdp = (mdp + 1) % MidDelaySamples;

            // High band: no delay (reference)

            // --- Harmonic exciter on high band ---
            // Soft-clip the high band to generate even/odd harmonics,
            // then blend the saturated signal back with the clean high band.
            float saturated = softClip(high * exciterDrive);
            float excitedHigh = high + (saturated - high) * exciterMix;

            // --- Per-band gain and summation ---
            buffer[f * channels + c] = delayedLow  * lowGain
                                     + delayedMid
                                     + excitedHigh * highGain;
        }
    }
}

void SonicEnhancer::setParameter(int id, float value)
{
    switch (id) {
    case LowContour:    m_lowContour    = std::clamp(value, 0.0f, 100.0f); break;
    case ProcessAmount: m_processAmount = std::clamp(value, 0.0f, 100.0f); break;
    case Presence:      m_presence      = std::clamp(value, 0.0f, 100.0f); break;
    default: break;
    }
}

float SonicEnhancer::parameter(int id) const
{
    switch (id) {
    case LowContour:    return m_lowContour;
    case ProcessAmount: return m_processAmount;
    case Presence:      return m_presence;
    default:            return 0.0f;
    }
}

QString SonicEnhancer::name() const
{
    return QStringLiteral("Sonic Enhancer");
}

int SonicEnhancer::parameterCount() const
{
    return ParamCount;
}

void SonicEnhancer::recalcFilters()
{
    // Butterworth Q for LR4 crossover (two cascaded 2nd-order = 4th-order LR)
    constexpr float bwQ = 0.7071f; // 1/sqrt(2)

    auto lpLow  = Biquad::lowpass(LowCrossover,  bwQ, m_sampleRate);
    auto hpLow  = Biquad::highpass(LowCrossover,  bwQ, m_sampleRate);
    auto lpHigh = Biquad::lowpass(HighCrossover,  bwQ, m_sampleRate);
    auto hpHigh = Biquad::highpass(HighCrossover, bwQ, m_sampleRate);

    for (int c = 0; c < MaxChannels; ++c) {
        m_lp1a[c].setCoeffs(lpLow);  m_lp1a[c].reset();
        m_lp1b[c].setCoeffs(lpLow);  m_lp1b[c].reset();
        m_hp1a[c].setCoeffs(hpLow);  m_hp1a[c].reset();
        m_hp1b[c].setCoeffs(hpLow);  m_hp1b[c].reset();
        m_lp2a[c].setCoeffs(lpHigh); m_lp2a[c].reset();
        m_lp2b[c].setCoeffs(lpHigh); m_lp2b[c].reset();
        m_hp2a[c].setCoeffs(hpHigh); m_hp2a[c].reset();
        m_hp2b[c].setCoeffs(hpHigh); m_hp2b[c].reset();
    }
}

} // namespace dawcast
