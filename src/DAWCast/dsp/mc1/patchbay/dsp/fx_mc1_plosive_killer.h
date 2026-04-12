/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_plosive_killer.h — MC1 Plosive Killer
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Transient-detection-based plosive suppressor. Catches the
 * sub-100 Hz thump from P/B/T sounds without applying a static HPF
 * that thins the voice. Algorithm:
 *
 *   1. Two parallel one-pole low-pass filters track the input
 *      envelope at the sub-band where plosives live (50–120 Hz).
 *   2. When the band level exceeds threshold, a gain envelope
 *      drops the entire signal by N dB for D milliseconds.
 *   3. Sensitivity controls the threshold; depth controls the
 *      reduction; recovery controls the release time.
 *
 * The reduction is broadband but only fires on actual plosive
 * transients, so the voice tone is preserved between hits.
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxPlosiveKiller : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ParamId {
        ParamSensitivity = 0,  // 0..1 → -50..-20 dBFS threshold
        ParamDepth,            // 0..1 → 0..18 dB reduction
        ParamSpeed,            // 0..1 → attack 0.5..10 ms
        ParamRecovery,         // 0..1 → release 30..300 ms
        ParamRange,            // 0..1 → 50..150 Hz detection center
        ParamMix,              // 0..1
        kParamCount
    };

    FxPlosiveKiller()
    {
        m_params[ParamSensitivity] = 0.55f;
        m_params[ParamDepth]       = 0.55f;
        m_params[ParamSpeed]       = 0.30f;
        m_params[ParamRecovery]    = 0.40f;
        m_params[ParamRange]       = 0.30f;
        m_params[ParamMix]         = 1.0f;
    }

    const char* name()    const override { return "MC1 Plosive Killer"; }
    const char* id()      const override { return "mc1.podcast.plosive"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_lpStateL = m_lpStateR = 0.0f;
        m_envFollower = 0.0f;
        m_gainEnv = 1.0f;
        recompute();
    }

    void reset() override
    {
        m_lpStateL = m_lpStateR = 0.0f;
        m_envFollower = 0.0f;
        m_gainEnv = 1.0f;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamSensitivity: return "Sensitivity";
            case ParamDepth:       return "Depth";
            case ParamSpeed:       return "Speed";
            case ParamRecovery:    return "Recovery";
            case ParamRange:       return "Range";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamSensitivity: return "dB";
            case ParamDepth:       return "dB";
            case ParamSpeed:       return "ms";
            case ParamRecovery:    return "ms";
            case ParamRange:       return "Hz";
            default:               return "%";
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
            case ParamSensitivity:
                std::snprintf(buf, sizeof(buf), "%.0f dB", -50.0f + m_params[idx] * 30.0f);
                return buf;
            case ParamDepth:
                std::snprintf(buf, sizeof(buf), "-%.1f dB", m_params[idx] * 18.0f);
                return buf;
            case ParamSpeed:
                std::snprintf(buf, sizeof(buf), "%.1f ms", 0.5f + m_params[idx] * 9.5f);
                return buf;
            case ParamRecovery:
                std::snprintf(buf, sizeof(buf), "%.0f ms", 30.0f + m_params[idx] * 270.0f);
                return buf;
            case ParamRange:
                std::snprintf(buf, sizeof(buf), "%.0f Hz", 50.0f + m_params[idx] * 100.0f);
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
        if (channels < 1 || frames == 0) return;

        const float threshLin = m_threshLin;
        const float reduction = m_reductionLin;
        const float attCoef   = m_attCoef;
        const float relCoef   = m_relCoef;
        const float lpCoef    = m_detectLpCoef;
        const float mix       = m_params[ParamMix];

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            float mono = 0.5f * (inL + inR);

            // Sub-band lowpass detector — only the plosive frequency range
            m_lpStateL = m_lpStateL * (1.0f - lpCoef) + mono * lpCoef;
            float band = m_lpStateL;

            // Envelope follower on the band
            float bandMag = std::fabs(band);
            if (bandMag > m_envFollower) {
                m_envFollower += (bandMag - m_envFollower) * 0.30f;
            } else {
                m_envFollower += (bandMag - m_envFollower) * 0.05f;
            }

            // Gate the broadband signal when the band envelope exceeds threshold
            float targetGain = 1.0f;
            if (m_envFollower > threshLin) {
                targetGain = reduction;
            }

            if (targetGain < m_gainEnv) {
                m_gainEnv += (targetGain - m_gainEnv) * attCoef;
            } else {
                m_gainEnv += (targetGain - m_gainEnv) * relCoef;
            }

            float wetL = inL * m_gainEnv;
            float wetR = inR * m_gainEnv;
            pcm[f * channels + 0] = inL * (1.0f - mix) + wetL * mix;
            if (channels > 1)
                pcm[f * channels + 1] = inR * (1.0f - mix) + wetR * mix;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        float threshDb = -50.0f + m_params[ParamSensitivity] * 30.0f;
        m_threshLin = std::pow(10.0f, threshDb / 20.0f);

        float reduceDb = -m_params[ParamDepth] * 18.0f;
        m_reductionLin = std::pow(10.0f, reduceDb / 20.0f);

        float attMs = 0.5f + m_params[ParamSpeed] * 9.5f;
        float relMs = 30.0f + m_params[ParamRecovery] * 270.0f;
        m_attCoef = 1.0f - std::exp(-1.0f / (attMs * 0.001f * fs));
        m_relCoef = 1.0f - std::exp(-1.0f / (relMs * 0.001f * fs));

        float rangeHz = 50.0f + m_params[ParamRange] * 100.0f;
        m_detectLpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * rangeHz / fs);
    }

    float m_params[kParamCount] = {};
    float m_threshLin    = 0.001f;
    float m_reductionLin = 0.5f;
    float m_attCoef      = 0.1f;
    float m_relCoef      = 0.01f;
    float m_detectLpCoef = 0.01f;
    float m_lpStateL = 0.0f, m_lpStateR = 0.0f;
    float m_envFollower = 0.0f;
    float m_gainEnv     = 1.0f;
};

} // namespace mc1dsp
