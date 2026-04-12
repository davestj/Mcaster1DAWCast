/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_stinger_bed.h — MC1 Stinger Bed
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Auto-ducking music bed for podcast / radio production.
 *
 * Designed to sit on a music-bed track that runs underneath a host
 * voice track. The plugin uses an internal envelope follower on the
 * bed signal but receives the voice signal envelope from the DAW
 * host (via setExternalEnv()) so it can duck the bed in real time
 * when the voice is active.
 *
 * Two modes:
 *   Voice-Over  — duck depth scales with voice envelope (gentler)
 *   Hard Duck   — voice over threshold = full duck, voice off = full bed
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <algorithm>
#include <atomic>

namespace mc1dsp {

class FxStingerBed : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum DuckMode { VoiceOver = 0, HardDuck };

    enum ParamId {
        ParamMode = 0,        // 0..1 → VoiceOver / HardDuck
        ParamDepth,           // 0..1 → 0..30 dB duck
        ParamThreshold,       // 0..1 → -50..-10 dBFS voice activity threshold
        ParamAttack,          // 0..1 → 5..200 ms duck attack
        ParamRelease,         // 0..1 → 100..2000 ms recovery
        ParamFadeIn,          // 0..1 → 0..3 s bed fade-in time
        ParamFadeOut,         // 0..1 → 0..3 s bed fade-out time
        ParamBedLevel,        // 0..1 → -24..0 dB master bed level
        kParamCount
    };

    FxStingerBed()
    {
        m_params[ParamMode]      = 0.0f;    // VoiceOver
        m_params[ParamDepth]     = 0.55f;
        m_params[ParamThreshold] = 0.55f;
        m_params[ParamAttack]    = 0.20f;
        m_params[ParamRelease]   = 0.40f;
        m_params[ParamFadeIn]    = 0.30f;
        m_params[ParamFadeOut]   = 0.30f;
        m_params[ParamBedLevel]  = 0.667f;  // 0 dB
    }

    const char* name()    const override { return "MC1 Stinger Bed"; }
    const char* id()      const override { return "mc1.podcast.stinger"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_currentDuck = 1.0f;
        m_voiceEnv = 0.0f;
        recompute();
    }

    void reset() override
    {
        m_currentDuck = 1.0f;
        m_voiceEnv = 0.0f;
    }

    /// External voice activity envelope (0..1). The host or another
    /// MC1 plugin pushes this in once per buffer.
    void setExternalEnv(float env) { m_externalEnv.store(env, std::memory_order_relaxed); }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamMode:      return "Mode";
            case ParamDepth:     return "Duck Depth";
            case ParamThreshold: return "Threshold";
            case ParamAttack:    return "Attack";
            case ParamRelease:   return "Release";
            case ParamFadeIn:    return "Fade In";
            case ParamFadeOut:   return "Fade Out";
            case ParamBedLevel:  return "Bed Level";
            default:             return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamDepth:     return "dB";
            case ParamThreshold: return "dB";
            case ParamAttack:    return "ms";
            case ParamRelease:   return "ms";
            case ParamFadeIn:    return "s";
            case ParamFadeOut:   return "s";
            case ParamBedLevel:  return "dB";
            default:             return "";
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
            case ParamMode:
                return (m_params[idx] >= 0.5f) ? "Hard Duck" : "Voice Over";
            case ParamDepth:
                std::snprintf(buf, sizeof(buf), "-%.0f dB", m_params[idx] * 30.0f);
                return buf;
            case ParamThreshold:
                std::snprintf(buf, sizeof(buf), "%.0f dB", -50.0f + m_params[idx] * 40.0f);
                return buf;
            case ParamAttack:
                std::snprintf(buf, sizeof(buf), "%.0f ms", 5.0f + m_params[idx] * 195.0f);
                return buf;
            case ParamRelease:
                std::snprintf(buf, sizeof(buf), "%.0f ms", 100.0f + m_params[idx] * 1900.0f);
                return buf;
            case ParamFadeIn:
            case ParamFadeOut:
                std::snprintf(buf, sizeof(buf), "%.1f s", m_params[idx] * 3.0f);
                return buf;
            case ParamBedLevel:
                std::snprintf(buf, sizeof(buf), "%+.1f dB", -24.0f + m_params[idx] * 24.0f);
                return buf;
        }
        return "";
    }

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const bool  hardDuck = m_params[ParamMode] >= 0.5f;
        const float threshLin = m_threshLin;
        const float duckMin   = m_duckMin;
        const float bedLevel  = m_bedLevel;
        const float attCoef   = m_attCoef;
        const float relCoef   = m_relCoef;

        // Get voice envelope from external sidechain
        float ext = m_externalEnv.load(std::memory_order_relaxed);
        // Smooth it
        if (ext > m_voiceEnv) m_voiceEnv += (ext - m_voiceEnv) * 0.20f;
        else                  m_voiceEnv += (ext - m_voiceEnv) * 0.05f;

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // Compute target duck level
            float target = 1.0f;
            if (hardDuck) {
                target = (m_voiceEnv > threshLin) ? duckMin : 1.0f;
            } else {
                // Voice-over: continuous ducking proportional to voice envelope
                if (m_voiceEnv > threshLin) {
                    float over = (m_voiceEnv - threshLin) / (1.0f - threshLin + 1e-6f);
                    if (over > 1.0f) over = 1.0f;
                    target = 1.0f - over * (1.0f - duckMin);
                }
            }

            if (target < m_currentDuck) m_currentDuck += (target - m_currentDuck) * attCoef;
            else                         m_currentDuck += (target - m_currentDuck) * relCoef;

            float gain = m_currentDuck * bedLevel;
            pcm[f * channels + 0] = inL * gain;
            if (channels > 1)
                pcm[f * channels + 1] = inR * gain;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        float threshDb = -50.0f + m_params[ParamThreshold] * 40.0f;
        m_threshLin = std::pow(10.0f, threshDb / 20.0f);

        float duckDb = -m_params[ParamDepth] * 30.0f;
        m_duckMin = std::pow(10.0f, duckDb / 20.0f);

        float bedDb = -24.0f + m_params[ParamBedLevel] * 24.0f;
        m_bedLevel = std::pow(10.0f, bedDb / 20.0f);

        float attMs = 5.0f + m_params[ParamAttack] * 195.0f;
        float relMs = 100.0f + m_params[ParamRelease] * 1900.0f;
        m_attCoef = 1.0f - std::exp(-1.0f / (attMs * 0.001f * fs));
        m_relCoef = 1.0f - std::exp(-1.0f / (relMs * 0.001f * fs));
    }

    float m_params[kParamCount] = {};
    float m_threshLin = 0.001f;
    float m_duckMin   = 0.1f;
    float m_bedLevel  = 1.0f;
    float m_attCoef   = 0.05f;
    float m_relCoef   = 0.005f;
    float m_currentDuck = 1.0f;

    std::atomic<float> m_externalEnv{0.0f};
    float m_voiceEnv = 0.0f;
};

} // namespace mc1dsp
