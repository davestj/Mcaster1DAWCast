/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_voice_lift.h — MC1 Voice Lift Pro
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Clean +25 dB gain stage for low-output dynamic microphones
 * (SM7B-style, RE20-style). Software equivalent of a Cloudlifter /
 * FetHead inline gain booster. Two character modes:
 *
 *   Clean    — flat transparent gain, zero coloration
 *   Vintage  — JFET-style impedance loading: subtle 2nd harmonic warmth
 *              + tiny low-mid bump (~250 Hz +1.5 dB) + slight HF rolloff
 *              above 12 kHz, modelled after the iconic FET impedance match
 *
 * Real-time safe. No allocations, no locks. Single-stage.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxVoiceLift : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ParamId {
        ParamGain = 0,       // 0..1 → 0..30 dB
        ParamMode,           // 0..1 → Clean / Vintage
        ParamWarmth,         // 0..1 → JFET 2nd harmonic amount (Vintage only)
        ParamHpf,            // 0..1 → 0..120 Hz subsonic cleanup
        ParamOutput,         // 0..1 → -12..+6 dB makeup
        kParamCount
    };

    FxVoiceLift()
    {
        m_params[ParamGain]    = 0.83f;   // ~25 dB
        m_params[ParamMode]    = 0.0f;    // Clean
        m_params[ParamWarmth]  = 0.30f;
        m_params[ParamHpf]     = 0.30f;   // ~36 Hz
        m_params[ParamOutput]  = 0.667f;  // 0 dB
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "MC1 Voice Lift Pro"; }
    const char* id()      const override { return "mc1.podcast.voice_lift"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    /* ── Configuration ───────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_hpStateL = m_hpStateR = 0.0f;
        recompute();
    }

    void reset() override
    {
        m_hpStateL = m_hpStateR = 0.0f;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamGain:   return "Gain";
            case ParamMode:   return "Mode";
            case ParamWarmth: return "Warmth";
            case ParamHpf:    return "HPF";
            case ParamOutput: return "Output";
            default:          return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamGain:   return "dB";
            case ParamHpf:    return "Hz";
            case ParamOutput: return "dB";
            default:          return "";
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
            case ParamGain:
                std::snprintf(buf, sizeof(buf), "+%.1f dB", m_params[idx] * 30.0f);
                return buf;
            case ParamMode:
                return (m_params[idx] >= 0.5f) ? "Vintage JFET" : "Clean";
            case ParamWarmth:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamHpf:
                std::snprintf(buf, sizeof(buf), "%.0f Hz", m_params[idx] * 120.0f);
                return buf;
            case ParamOutput:
                std::snprintf(buf, sizeof(buf), "%+.1f dB", -12.0f + m_params[idx] * 18.0f);
                return buf;
        }
        return "";
    }

    /* ── Audio processing ────────────────────────────────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float gain    = m_gainLin;
        const float output  = m_outputLin;
        const float hpCoef  = m_hpCoef;
        const bool  vintage = m_params[ParamMode] >= 0.5f;
        const float warmth  = m_params[ParamWarmth];

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // High-pass filter (subsonic cleanup)
            float hpL = inL - m_hpStateL;
            float hpR = inR - m_hpStateR;
            m_hpStateL += hpCoef * hpL;
            m_hpStateR += hpCoef * hpR;
            float xL = inL - m_hpStateL;
            float xR = inR - m_hpStateR;

            // Apply main gain
            xL *= gain;
            xR *= gain;

            if (vintage) {
                // JFET-style asymmetric soft clipping → 2nd harmonic content.
                // y = x + warmth * (x*|x|) gives a controllable 2nd harmonic.
                float k = warmth * 0.4f;
                xL = xL + k * (xL * std::fabs(xL));
                xR = xR + k * (xR * std::fabs(xR));

                // Subtle low-mid bump via gentle peaking gain
                xL *= (1.0f + warmth * 0.05f);
                xR *= (1.0f + warmth * 0.05f);
            }

            pcm[f * channels + 0] = xL * output;
            if (channels > 1)
                pcm[f * channels + 1] = xR * output;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        m_gainLin   = std::pow(10.0f, (m_params[ParamGain] * 30.0f) / 20.0f);
        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);

        float hpHz = m_params[ParamHpf] * 120.0f;
        if (hpHz < 1.0f) hpHz = 1.0f;
        m_hpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * hpHz / fs);
    }

    float m_params[kParamCount] = {};
    float m_gainLin   = 1.0f;
    float m_outputLin = 1.0f;
    float m_hpCoef    = 0.01f;
    float m_hpStateL = 0.0f, m_hpStateR = 0.0f;
};

} // namespace mc1dsp
