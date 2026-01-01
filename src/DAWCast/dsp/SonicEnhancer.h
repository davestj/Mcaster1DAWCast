// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"
#include "dsp/Biquad.h"

#include <array>
#include <vector>

namespace dawcast {

// 3-band phase-corrective exciter (BBE Sonic Maximizer style).
// Uses Linkwitz-Riley crossover to split into low, mid, and high bands,
// then applies independent phase-aligned delay/boost per band.
// Harmonic exciter on the high band via soft clipping for added presence.

class SonicEnhancer : public IEffectUnit
{
public:
    enum Param
    {
        LowContour = 0,    // Low-frequency boost / contour  (0..100)
        ProcessAmount,      // Mid/high harmonic process      (0..100)
        Presence,           // High-frequency presence         (0..100)
        ParamCount
    };

    SonicEnhancer();
    ~SonicEnhancer() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    void recalcFilters();

    // Soft-clip function for harmonic generation (tanh approximation)
    static inline float softClip(float x)
    {
        // Fast tanh approximation — Pade form, no allocation
        if (x > 3.0f)  return 1.0f;
        if (x < -3.0f) return -1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float m_lowContour    = 50.0f;
    float m_processAmount = 50.0f;
    float m_presence      = 50.0f;

    float m_sampleRate = 48000.0f;

    // Linkwitz-Riley crossover points
    static constexpr float LowCrossover  = 250.0f;   // Hz
    static constexpr float HighCrossover  = 3500.0f;  // Hz

    // LR4 = two cascaded 2nd-order Butterworth filters
    static constexpr int MaxChannels = 16;

    // Low/high split (LP + HP at LowCrossover)
    std::array<Biquad, MaxChannels> m_lp1a, m_lp1b;  // 2x LP for LR4 low
    std::array<Biquad, MaxChannels> m_hp1a, m_hp1b;  // 2x HP for LR4 high-mid+high

    // Mid/high split (LP + HP at HighCrossover — on the upper output)
    std::array<Biquad, MaxChannels> m_lp2a, m_lp2b;  // 2x LP for LR4 mid
    std::array<Biquad, MaxChannels> m_hp2a, m_hp2b;  // 2x HP for LR4 high

    // Phase-corrective delay lines for band time-alignment
    // Low band is delayed most (group delay compensation for higher crossover latency)
    // Mid band delayed by a smaller amount; high band has zero delay.
    static constexpr int LowDelaySamples = 16;   // ~0.33ms at 48kHz
    static constexpr int MidDelaySamples = 8;    // ~0.17ms at 48kHz
    std::array<std::array<float, LowDelaySamples>, MaxChannels> m_lowDelay{};
    std::array<std::array<float, MidDelaySamples>, MaxChannels> m_midDelay{};
    std::array<int, MaxChannels> m_lowDelayPos{};
    std::array<int, MaxChannels> m_midDelayPos{};
};

} // namespace dawcast
