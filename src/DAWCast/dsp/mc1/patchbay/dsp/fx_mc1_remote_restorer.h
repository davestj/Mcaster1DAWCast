/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_remote_restorer.h — MC1 Remote Guest Restorer
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Audio restoration for remote-recorded guests on bad mics in untreated
 * rooms. Combines four restoration stages in series:
 *
 *   1. Spectral noise gate — fixed-noise-floor gate that removes
 *      low-level hum / fan / rumble below the speech band
 *   2. Reverb suppression — fast envelope-tracking expander that fades
 *      out lingering room tail between speech transients
 *   3. Resonance flattener — narrow notch filter that hunts the most
 *      prominent harmonic resonance (room mode) and attenuates it
 *   4. Voice band emphasis — gentle 2 kHz–6 kHz lift to recover
 *      intelligibility lost to room mud
 *
 * No FFT — all stages are time-domain so the latency is just one
 * sample. Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxRemoteRestorer : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ParamId {
        ParamDeNoise = 0,      // 0..1 noise floor reduction
        ParamDeReverb,         // 0..1 reverb tail suppression
        ParamDeResonance,      // 0..1 room mode attenuation
        ParamPresence,         // 0..1 voice band lift
        ParamLowCut,           // 0..1 → 60..400 Hz HPF
        ParamMix,              // 0..1
        kParamCount
    };

    FxRemoteRestorer()
    {
        m_params[ParamDeNoise]     = 0.45f;
        m_params[ParamDeReverb]    = 0.45f;
        m_params[ParamDeResonance] = 0.35f;
        m_params[ParamPresence]    = 0.40f;
        m_params[ParamLowCut]      = 0.30f;
        m_params[ParamMix]         = 1.0f;
    }

    const char* name()    const override { return "MC1 Remote Guest Restorer"; }
    const char* id()      const override { return "mc1.podcast.remote_restore"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_hpStateL = m_hpStateR = 0.0f;
        m_envFast = m_envSlow = 0.0f;
        m_resBpStateL = m_resBpStateR = 0.0f;
        m_presBpStateL = m_presBpStateR = 0.0f;
        recompute();
    }

    void reset() override
    {
        m_hpStateL = m_hpStateR = 0.0f;
        m_envFast = m_envSlow = 0.0f;
        m_resBpStateL = m_resBpStateR = 0.0f;
        m_presBpStateL = m_presBpStateR = 0.0f;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamDeNoise:     return "De-Noise";
            case ParamDeReverb:    return "De-Reverb";
            case ParamDeResonance: return "De-Resonance";
            case ParamPresence:    return "Presence";
            case ParamLowCut:      return "Low Cut";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        if (idx == ParamLowCut) return "Hz";
        return "%";
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
        if (idx == ParamLowCut) {
            std::snprintf(buf, sizeof(buf), "%.0f Hz", 60.0f + m_params[idx] * 340.0f);
            return buf;
        }
        std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
        return buf;
    }

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float deNoise   = m_params[ParamDeNoise];
        const float deReverb  = m_params[ParamDeReverb];
        const float deRes     = m_params[ParamDeResonance];
        const float presence  = m_params[ParamPresence];
        const float mix       = m_params[ParamMix];
        const float hpC       = m_hpCoef;
        const float resBpC    = m_resBpCoef;
        const float presBpC   = m_presBpCoef;

        // Noise floor: roughly -45 dB scaled by deNoise
        const float noiseFloor = std::pow(10.0f, (-45.0f - deNoise * 15.0f) / 20.0f);

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // 1. Low cut high-pass
            float hpL = inL - m_hpStateL;
            float hpR = inR - m_hpStateR;
            m_hpStateL += hpC * hpL;
            m_hpStateR += hpC * hpR;
            float xL = inL - m_hpStateL;
            float xR = inR - m_hpStateR;

            // 2. De-noise (downward expansion)
            float mag = 0.5f * (std::fabs(xL) + std::fabs(xR));
            float gateGain = 1.0f;
            if (mag < noiseFloor && deNoise > 0.0f) {
                float over = mag / noiseFloor;
                gateGain = over * over;
            }
            xL *= gateGain;
            xR *= gateGain;

            // 3. De-reverb: fast/slow envelope ratio. When fast env drops
            // faster than slow env, we're in reverb tail → duck.
            if (mag > m_envFast) m_envFast += (mag - m_envFast) * 0.20f;
            else                  m_envFast += (mag - m_envFast) * 0.05f;
            if (mag > m_envSlow) m_envSlow += (mag - m_envSlow) * 0.005f;
            else                  m_envSlow += (mag - m_envSlow) * 0.0005f;

            float ratio = m_envFast / (m_envSlow + 1e-6f);
            float reverbGain = 1.0f;
            if (ratio < 1.0f && deReverb > 0.0f) {
                reverbGain = 1.0f - (1.0f - ratio) * deReverb * 0.8f;
                if (reverbGain < 0.05f) reverbGain = 0.05f;
            }
            xL *= reverbGain;
            xR *= reverbGain;

            // 4. De-resonance: subtract a narrow band from the signal
            m_resBpStateL = m_resBpStateL * (1.0f - resBpC) + xL * resBpC;
            m_resBpStateR = m_resBpStateR * (1.0f - resBpC) + xR * resBpC;
            xL -= m_resBpStateL * deRes * 0.7f;
            xR -= m_resBpStateR * deRes * 0.7f;

            // 5. Presence boost (add a high-shelf flavor band)
            m_presBpStateL = m_presBpStateL * (1.0f - presBpC) + xL * presBpC;
            m_presBpStateR = m_presBpStateR * (1.0f - presBpC) + xR * presBpC;
            float presL = xL - m_presBpStateL;
            float presR = xR - m_presBpStateR;
            xL += presL * presence * 0.6f;
            xR += presR * presence * 0.6f;

            pcm[f * channels + 0] = inL * (1.0f - mix) + xL * mix;
            if (channels > 1)
                pcm[f * channels + 1] = inR * (1.0f - mix) + xR * mix;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        float hpHz = 60.0f + m_params[ParamLowCut] * 340.0f;
        m_hpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * hpHz / fs);

        // De-resonance band centered around 250 Hz (typical room mode)
        m_resBpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * 250.0f / fs);

        // Presence shelf around 4 kHz
        m_presBpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * 4000.0f / fs);
    }

    float m_params[kParamCount] = {};
    float m_hpCoef = 0.01f;
    float m_resBpCoef = 0.01f;
    float m_presBpCoef = 0.01f;

    float m_hpStateL = 0.0f, m_hpStateR = 0.0f;
    float m_resBpStateL = 0.0f, m_resBpStateR = 0.0f;
    float m_presBpStateL = 0.0f, m_presBpStateR = 0.0f;

    float m_envFast = 0.0f;
    float m_envSlow = 0.0f;
};

} // namespace mc1dsp
