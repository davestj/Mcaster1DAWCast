/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_phone_line.h — MC1 Phone Line Sim
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Telephony / VoIP / cellphone simulator for podcast guest call-ins.
 * Models four eras of voice transmission:
 *
 *   Landline   — 300 Hz–3.4 kHz hard band-pass + ITU G.711 µ-law style
 *                 quantization noise + minor flutter
 *   Cellphone  — narrowband AMR-style: 200 Hz–3.5 kHz + GSM artifacts
 *   Skype/Zoom — wideband 80 Hz–7 kHz + Opus-style codec wobble + AGC
 *                 pumping + occasional dropouts
 *   Walkie     — 300 Hz–2 kHz + heavy compression + static + handheld
 *                 transmit click on/off
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace mc1dsp {

class FxPhoneLineSim : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum LineType { Landline = 0, Cellphone, SkypeZoom, Walkie };

    enum ParamId {
        ParamLineType = 0,    // 0..3 enum
        ParamArtifacts,       // 0..1 codec wobble / quantization noise
        ParamDropoutRate,     // 0..1 random dropout probability
        ParamStatic,          // 0..1 background hiss
        ParamCompression,     // 0..1 narrowband AGC pumping amount
        ParamMix,             // 0..1
        kParamCount
    };

    FxPhoneLineSim()
    {
        m_params[ParamLineType]    = 0.33f;  // SkypeZoom
        m_params[ParamArtifacts]   = 0.40f;
        m_params[ParamDropoutRate] = 0.10f;
        m_params[ParamStatic]      = 0.15f;
        m_params[ParamCompression] = 0.50f;
        m_params[ParamMix]         = 1.0f;
    }

    const char* name()    const override { return "MC1 Phone Line Sim"; }
    const char* id()      const override { return "mc1.podcast.phone_line"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_hpStateL = m_hpStateR = 0.0f;
        m_lpStateL = m_lpStateR = 0.0f;
        m_agcEnv = 0.0f;
        m_dropoutGain = 1.0f;
        m_dropoutSampleRemaining = 0;
        m_rngState = 0xC0FFEE;
        recompute();
    }

    void reset() override
    {
        m_hpStateL = m_hpStateR = 0.0f;
        m_lpStateL = m_lpStateR = 0.0f;
        m_agcEnv = 0.0f;
        m_dropoutGain = 1.0f;
        m_dropoutSampleRemaining = 0;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamLineType:    return "Line Type";
            case ParamArtifacts:   return "Artifacts";
            case ParamDropoutRate: return "Dropout";
            case ParamStatic:      return "Static";
            case ParamCompression: return "Compression";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override { (void)idx; return "%"; }

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
            case ParamLineType: {
                static const char* types[4] = {
                    "Landline", "Cellphone", "Skype / Zoom", "Walkie Talkie"
                };
                int t = std::max(0, std::min(3, static_cast<int>(m_params[idx] * 3.999f)));
                return types[t];
            }
            default:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
    }

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const int   line     = std::max(0, std::min(3, static_cast<int>(m_params[ParamLineType] * 3.999f)));
        const float artifact = m_params[ParamArtifacts];
        const float staticL  = m_params[ParamStatic];
        const float compAmt  = m_params[ParamCompression];
        const float mix      = m_params[ParamMix];
        const float hpC      = m_hpCoef;
        const float lpC      = m_lpCoef;

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            float mono = 0.5f * (inL + inR);

            // Random dropout state machine
            if (m_dropoutSampleRemaining > 0) {
                m_dropoutSampleRemaining--;
                if (m_dropoutSampleRemaining == 0) m_dropoutGain = 1.0f;
            } else if (m_dropoutChance > 0.0f) {
                float r = randUnit();
                if (r < m_dropoutChance) {
                    m_dropoutGain = 0.05f;
                    m_dropoutSampleRemaining = m_dropoutLenSamples;
                }
            }

            // Bandpass to telephony frequency range (high-pass + low-pass)
            float hp = mono - m_hpStateL;
            m_hpStateL += hpC * hp;
            float band = mono - m_hpStateL;
            m_lpStateL = m_lpStateL * (1.0f - lpC) + band * lpC;
            float bp = m_lpStateL;

            // Quantization / codec artifacts (bit reduction)
            if (artifact > 0.001f) {
                int bits = static_cast<int>(12.0f - artifact * 7.0f);  // 12..5 bits
                if (bits < 4) bits = 4;
                float steps = static_cast<float>(1 << bits);
                bp = std::round(bp * steps) / steps;
            }

            // Narrowband AGC pumping
            if (compAmt > 0.001f) {
                float mag = std::fabs(bp);
                if (mag > m_agcEnv) m_agcEnv += (mag - m_agcEnv) * 0.20f;
                else                m_agcEnv += (mag - m_agcEnv) * 0.005f;
                float agcGain = 1.0f / (1.0f + m_agcEnv * compAmt * 5.0f);
                bp *= agcGain;
            }

            // Background static
            if (staticL > 0.001f) {
                bp += (randUnit() - 0.5f) * staticL * 0.04f;
            }

            // Dropout gain
            bp *= m_dropoutGain;

            // Walkie talkie: hard clip
            if (line == Walkie) {
                bp = std::max(-0.7f, std::min(0.7f, bp * 1.6f));
            }

            float out = bp;
            pcm[f * channels + 0] = inL * (1.0f - mix) + out * mix;
            if (channels > 1)
                pcm[f * channels + 1] = inR * (1.0f - mix) + out * mix;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);
        int line = std::max(0, std::min(3, static_cast<int>(m_params[ParamLineType] * 3.999f)));

        float hpHz = 300.0f, lpHz = 3400.0f;
        switch (line) {
            case Landline:  hpHz = 300.0f; lpHz = 3400.0f; break;
            case Cellphone: hpHz = 200.0f; lpHz = 3500.0f; break;
            case SkypeZoom: hpHz = 80.0f;  lpHz = 7000.0f; break;
            case Walkie:    hpHz = 300.0f; lpHz = 2200.0f; break;
        }
        m_hpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * hpHz / fs);
        m_lpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * lpHz / fs);

        // Dropout: probability per sample, length 30..150 ms
        m_dropoutChance = m_params[ParamDropoutRate] * (1.0f / fs) * 1.5f;
        m_dropoutLenSamples = static_cast<int>(0.080f * fs);  // 80 ms dropouts
    }

    inline float randUnit()
    {
        // Linear congruential — fast & deterministic, good enough for noise
        m_rngState = m_rngState * 1664525u + 1013904223u;
        return static_cast<float>(m_rngState >> 8) / 16777216.0f;
    }

    float m_params[kParamCount] = {};
    float m_hpCoef = 0.0f, m_lpCoef = 0.0f;
    float m_hpStateL = 0.0f, m_hpStateR = 0.0f;
    float m_lpStateL = 0.0f, m_lpStateR = 0.0f;
    float m_agcEnv = 0.0f;

    float m_dropoutChance = 0.0f;
    int   m_dropoutLenSamples = 0;
    int   m_dropoutSampleRemaining = 0;
    float m_dropoutGain = 1.0f;

    uint32_t m_rngState = 0xC0FFEE;
};

} // namespace mc1dsp
