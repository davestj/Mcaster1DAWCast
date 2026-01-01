// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"
#include "dsp/Biquad.h"

#include <array>
#include <vector>

namespace dawcast {

// EBU R128 / ITU-R BS.1770 loudness measurement with makeup gain.
// Implements K-weighting (pre-filter high shelf + RLB high-pass) and
// 400ms block-based gated integrated loudness measurement.

class Normalizer : public IEffectUnit
{
public:
    enum Standard : int
    {
        EBU_R128    = 0,
        ITU_BS1770  = 1
    };

    enum Param
    {
        TargetLufs = 0,
        MeasureStandard,
        ParamCount
    };

    Normalizer();
    ~Normalizer() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

    // Query current measurement
    float currentLufs() const { return m_currentLufs; }

private:
    void initKWeighting();

    float m_targetLufs = -23.0f;   // EBU R128 default
    Standard m_standard = EBU_R128;

    float m_sampleRate   = 48000.0f;
    float m_currentLufs  = -120.0f;
    float m_makeupGainDb = 0.0f;

    // Smoothed gain to avoid sudden jumps
    float m_smoothedGainLin = 1.0f;
    float m_gainSmoothCoeff = 0.0f;  // ~100ms smoothing

    // --- K-weighting filters (ITU-R BS.1770-4) ---
    // Stage 1: Pre-filter (high shelf, +4 dB at 1681 Hz)
    // Stage 2: RLB weighting (high-pass, ~38 Hz, revised low-frequency B-curve)
    static constexpr int MaxChannels = 16;
    std::array<Biquad, MaxChannels> m_preFilter;   // stage 1
    std::array<Biquad, MaxChannels> m_rlbFilter;   // stage 2

    // --- 400ms block-based gated loudness ---
    // We accumulate mean-square per 400ms block, then perform gating.
    int   m_blockSize      = 0;     // samples per 400ms block
    int   m_blockPos       = 0;     // current position within block
    double m_blockSumSq    = 0.0;   // running sum of squares for current block
    int    m_blockChannels = 2;     // channels in current block

    // Ring buffer of block loudness values for gated measurement
    static constexpr int MaxBlocks = 750;  // ~5 minutes of 400ms blocks
    std::array<double, MaxBlocks> m_blockLoudness{};  // mean-square per block
    int m_blockWriteIdx = 0;
    int m_blockCount    = 0;

    // Integrated (gated) loudness computed after each block
    double m_integratedLoudness = -120.0;
};

} // namespace dawcast
