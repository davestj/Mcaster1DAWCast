/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_bleed_suppressor.h — MC1 Multi-Host Bleed Suppressor
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Multi-host roundtable podcast bleed suppressor.
 *
 * When 2–4 hosts share a room each mic picks up everyone else faintly.
 * This plugin works as an envelope-following gate that opens when THIS
 * host is louder than the others, and closes (ducks) when one of the
 * sibling hosts is loud (= they're talking, not us).
 *
 * Without true sidechain inputs, the plugin uses a self-tracking
 * "speaker activity" detector: it watches the local input envelope
 * and applies a fast-acting expander when the input drops below the
 * activity threshold (i.e. the host stopped talking, so any residual
 * audio is bleed from someone else).
 *
 * For full sidechain mode the plugin exposes a hook so the host can
 * push other channels' envelopes via setExternalEnv() — when used
 * inside the Signal Hill studio plugin, the studio routes the right
 * sibling envelopes in.
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <algorithm>
#include <atomic>

namespace mc1dsp {

class FxBleedSuppressor : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ParamId {
        ParamThreshold = 0,    // 0..1 → -60..-10 dBFS
        ParamRange,            // 0..1 → 0..40 dB max reduction
        ParamAttack,           // 0..1 → 0.5..30 ms
        ParamRelease,          // 0..1 → 30..500 ms
        ParamHold,             // 0..1 → 0..200 ms
        ParamLookahead,        // 0..1 → 0..10 ms
        ParamMix,              // 0..1
        kParamCount
    };

    FxBleedSuppressor()
    {
        m_params[ParamThreshold] = 0.55f;
        m_params[ParamRange]     = 0.65f;
        m_params[ParamAttack]    = 0.20f;
        m_params[ParamRelease]   = 0.40f;
        m_params[ParamHold]      = 0.30f;
        m_params[ParamLookahead] = 0.30f;
        m_params[ParamMix]       = 1.0f;
    }

    const char* name()    const override { return "MC1 Multi-Host Bleed Suppressor"; }
    const char* id()      const override { return "mc1.podcast.bleed"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_envFollower = 0.0f;
        m_gainEnv = 1.0f;
        m_holdSamples = 0;
        recompute();
    }

    void reset() override
    {
        m_envFollower = 0.0f;
        m_gainEnv = 1.0f;
        m_holdSamples = 0;
    }

    /// Optional: when used in a multi-channel context, host can push the
    /// max envelope from the other host channels. The local gate will
    /// open more aggressively when sibling channels are quiet.
    void setExternalEnv(float env) { m_externalEnv.store(env, std::memory_order_relaxed); }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamThreshold: return "Threshold";
            case ParamRange:     return "Range";
            case ParamAttack:    return "Attack";
            case ParamRelease:   return "Release";
            case ParamHold:      return "Hold";
            case ParamLookahead: return "Lookahead";
            case ParamMix:       return "Mix";
            default:             return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamThreshold: return "dB";
            case ParamRange:     return "dB";
            case ParamAttack:    return "ms";
            case ParamRelease:   return "ms";
            case ParamHold:      return "ms";
            case ParamLookahead: return "ms";
            default:             return "%";
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
            case ParamThreshold:
                std::snprintf(buf, sizeof(buf), "%.0f dB", -60.0f + m_params[idx] * 50.0f);
                return buf;
            case ParamRange:
                std::snprintf(buf, sizeof(buf), "-%.0f dB", m_params[idx] * 40.0f);
                return buf;
            case ParamAttack:
                std::snprintf(buf, sizeof(buf), "%.1f ms", 0.5f + m_params[idx] * 29.5f);
                return buf;
            case ParamRelease:
                std::snprintf(buf, sizeof(buf), "%.0f ms", 30.0f + m_params[idx] * 470.0f);
                return buf;
            case ParamHold:
                std::snprintf(buf, sizeof(buf), "%.0f ms", m_params[idx] * 200.0f);
                return buf;
            case ParamLookahead:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 10.0f);
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
        const float rangeLin  = m_rangeLin;
        const float attCoef   = m_attCoef;
        const float relCoef   = m_relCoef;
        const float mix       = m_params[ParamMix];

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            float mag = 0.5f * (std::fabs(inL) + std::fabs(inR));

            // Envelope follower
            if (mag > m_envFollower) m_envFollower += (mag - m_envFollower) * 0.30f;
            else                      m_envFollower += (mag - m_envFollower) * 0.02f;

            // Local activity vs. external
            float ext = m_externalEnv.load(std::memory_order_relaxed);
            // If external env is louder, we're hearing bleed: gate harder
            float effectiveThresh = threshLin;
            if (ext > m_envFollower * 1.2f) {
                effectiveThresh *= 2.0f;
            }

            float targetGain = 1.0f;
            if (m_envFollower < effectiveThresh) {
                targetGain = rangeLin;
                m_holdSamples = m_holdLengthSamples;
            } else if (m_holdSamples > 0) {
                m_holdSamples--;
                targetGain = 1.0f;
            }

            if (targetGain < m_gainEnv)
                m_gainEnv += (targetGain - m_gainEnv) * attCoef;
            else
                m_gainEnv += (targetGain - m_gainEnv) * relCoef;

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

        float threshDb = -60.0f + m_params[ParamThreshold] * 50.0f;
        m_threshLin = std::pow(10.0f, threshDb / 20.0f);
        float rangeDb = -m_params[ParamRange] * 40.0f;
        m_rangeLin = std::pow(10.0f, rangeDb / 20.0f);

        float attMs = 0.5f + m_params[ParamAttack] * 29.5f;
        float relMs = 30.0f + m_params[ParamRelease] * 470.0f;
        m_attCoef = 1.0f - std::exp(-1.0f / (attMs * 0.001f * fs));
        m_relCoef = 1.0f - std::exp(-1.0f / (relMs * 0.001f * fs));

        m_holdLengthSamples = static_cast<int>(m_params[ParamHold] * 0.200f * fs);
    }

    float m_params[kParamCount] = {};
    float m_threshLin = 0.001f;
    float m_rangeLin  = 0.1f;
    float m_attCoef   = 0.05f;
    float m_relCoef   = 0.005f;
    int   m_holdLengthSamples = 0;
    int   m_holdSamples = 0;

    float m_envFollower = 0.0f;
    float m_gainEnv = 1.0f;

    std::atomic<float> m_externalEnv{0.0f};
};

} // namespace mc1dsp
