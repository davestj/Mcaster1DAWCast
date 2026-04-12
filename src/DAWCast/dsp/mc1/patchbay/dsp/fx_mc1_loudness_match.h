/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_loudness_match.h — MC1 Loudness Match
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Per-track EBU R128-style loudness matcher. Tracks short-term LUFS
 * against a target value and applies smooth makeup gain so all hosts
 * sit at the same perceived volume.
 *
 *   - K-weighting filter (high-shelf + RLB high-pass) approximates EBU R128
 *   - Sliding 400 ms RMS window for short-term LUFS
 *   - Smooth gain follower (slow attack/release so it doesn't pump)
 *   - Optional ceiling protection
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace mc1dsp {

class FxLoudnessMatch : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ParamId {
        ParamTarget = 0,    // 0..1 → -30..-10 LUFS
        ParamRange,         // 0..1 → max gain change in dB
        ParamSpeed,         // 0..1 → response time
        ParamCeiling,       // 0..1 → -3..0 dBFS
        ParamMix,           // 0..1
        kParamCount
    };

    FxLoudnessMatch()
    {
        m_params[ParamTarget]  = 0.7f;   // ~-16 LUFS (Spotify podcast standard)
        m_params[ParamRange]   = 0.5f;
        m_params[ParamSpeed]   = 0.4f;
        m_params[ParamCeiling] = 0.6f;
        m_params[ParamMix]     = 1.0f;
    }

    const char* name()    const override { return "MC1 Loudness Match"; }
    const char* id()      const override { return "mc1.podcast.loudness_match"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        // 400 ms sliding window
        int winLen = static_cast<int>(0.4 * sr);
        m_window.assign(winLen, 0.0f);
        m_windowIdx = 0;
        m_windowSum = 0.0f;

        // K-weighting state
        m_kHpState1 = m_kHpState2 = 0.0f;
        m_kHsState1 = m_kHsState2 = 0.0f;
        m_currentGain = 1.0f;

        recompute();
    }

    void reset() override
    {
        std::fill(m_window.begin(), m_window.end(), 0.0f);
        m_windowSum = 0.0f;
        m_windowIdx = 0;
        m_kHpState1 = m_kHpState2 = 0.0f;
        m_kHsState1 = m_kHsState2 = 0.0f;
        m_currentGain = 1.0f;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamTarget:  return "Target";
            case ParamRange:   return "Range";
            case ParamSpeed:   return "Speed";
            case ParamCeiling: return "Ceiling";
            case ParamMix:     return "Mix";
            default:           return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamTarget:  return "LUFS";
            case ParamRange:   return "dB";
            case ParamCeiling: return "dBFS";
            default:           return "%";
        }
    }

    float paramValue(int idx) const override
    {
        return (idx >= 0 && idx < kParamCount) ? m_params[idx] : 0.0f;
    }

    void setParamValue(int idx, float v) override
    {
        if (idx < 0 || idx >= kParamCount) return;
        m_params[idx] = std::max(0.0f, std::min(1.0f, v));
        recompute();
    }

    std::string paramDisplayValue(int idx) const override
    {
        char buf[32];
        switch (idx) {
            case ParamTarget:
                std::snprintf(buf, sizeof(buf), "%.1f LUFS", -30.0f + m_params[idx] * 20.0f);
                return buf;
            case ParamRange:
                std::snprintf(buf, sizeof(buf), "±%.0f dB", m_params[idx] * 18.0f);
                return buf;
            case ParamSpeed:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamCeiling:
                std::snprintf(buf, sizeof(buf), "%.1f dBFS", -3.0f + m_params[idx] * 3.0f);
                return buf;
            case ParamMix:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
        return "";
    }

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0 || m_window.empty()) return;

        const float targetLUFS  = -30.0f + m_params[ParamTarget] * 20.0f;
        const float maxRangeDb  = m_params[ParamRange] * 18.0f;
        const float ceilingLin  = std::pow(10.0f, (-3.0f + m_params[ParamCeiling] * 3.0f) / 20.0f);
        const float gainCoef    = m_gainCoef;
        const float mix         = m_params[ParamMix];
        const int   winLen      = static_cast<int>(m_window.size());

        const float minGainLin = std::pow(10.0f, -maxRangeDb / 20.0f);
        const float maxGainLin = std::pow(10.0f,  maxRangeDb / 20.0f);

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            float mono = 0.5f * (inL + inR);

            // K-weighting filter (simplified — high shelf + high pass)
            // Pre-filter: 38 Hz HPF
            float hp = mono - m_kHpState1;
            m_kHpState1 += 0.005f * hp;
            float kw = mono - m_kHpState1;
            // High-shelf at 1.5 kHz, +4 dB
            m_kHsState1 = m_kHsState1 * 0.85f + kw * 0.15f;
            kw = kw + (kw - m_kHsState1) * 0.4f;

            // Sliding RMS
            float square = kw * kw;
            m_windowSum -= m_window[m_windowIdx];
            m_window[m_windowIdx] = square;
            m_windowSum += square;
            if (++m_windowIdx >= winLen) m_windowIdx = 0;

            float meanSq = m_windowSum / static_cast<float>(winLen);
            float lufs   = -0.691f + 10.0f * std::log10(std::max(1e-12f, meanSq));

            // Compute target gain in dB then convert to linear
            float gainDb = targetLUFS - lufs;
            if (gainDb > maxRangeDb)  gainDb = maxRangeDb;
            if (gainDb < -maxRangeDb) gainDb = -maxRangeDb;
            float targetGain = std::pow(10.0f, gainDb / 20.0f);
            targetGain = std::max(minGainLin, std::min(maxGainLin, targetGain));

            // Smooth toward the target
            m_currentGain += (targetGain - m_currentGain) * gainCoef;

            float wetL = inL * m_currentGain;
            float wetR = inR * m_currentGain;

            // Ceiling protection
            if (wetL >  ceilingLin) wetL =  ceilingLin;
            if (wetL < -ceilingLin) wetL = -ceilingLin;
            if (wetR >  ceilingLin) wetR =  ceilingLin;
            if (wetR < -ceilingLin) wetR = -ceilingLin;

            pcm[f * channels + 0] = inL * (1.0f - mix) + wetL * mix;
            if (channels > 1)
                pcm[f * channels + 1] = inR * (1.0f - mix) + wetR * mix;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        // Slower speed = smaller coefficient (more smoothing)
        float speedNorm = m_params[ParamSpeed];
        // Convert 0..1 → 50..2 ms time constant
        float tcMs = 50.0f - speedNorm * 48.0f;
        m_gainCoef = 1.0f - std::exp(-1.0f / (tcMs * 0.001f * static_cast<float>(sampleRate_)));
    }

    float m_params[kParamCount] = {};
    std::vector<float> m_window;
    float m_windowSum = 0.0f;
    int   m_windowIdx = 0;

    float m_kHpState1 = 0.0f, m_kHpState2 = 0.0f;
    float m_kHsState1 = 0.0f, m_kHsState2 = 0.0f;

    float m_currentGain = 1.0f;
    float m_gainCoef    = 0.001f;
};

} // namespace mc1dsp
