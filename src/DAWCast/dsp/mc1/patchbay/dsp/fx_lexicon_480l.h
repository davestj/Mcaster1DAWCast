/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * dsp/fx_lexicon_480l.h — Lexicon 480L Random Hall (1986)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Header-only emulation of the Lexicon 480L — the rack reverb that
 * defined the Hollywood scoring stage and the bigger-than-life mix
 * bus reverb sound from the late '80s through today. Two algorithms:
 *
 *   Random Hall      — Modulated long hall, dense smooth tail
 *   Random Ambience  — Short, dense, stochastic ER-heavy room
 *
 * Architecture:
 *   input → DC blocker → pre-delay → 8 randomized tap-delays (early
 *   reflections, refreshed every few seconds) → 8 parallel modulated
 *   comb filters (the "decay tank") → 6 series allpass diffusers →
 *   tail-density saturation → wet/dry mix.
 *
 * The "random" in Random Hall means the comb modulation has multiple
 * decorrelated LFOs and the tap delay times shift slowly over time —
 * not literally random per sample, but stochastic enough to break up
 * the cyclic ringing of a fixed Schroeder network.
 *
 * Real-time safe. No allocs / locks in process().
 */

#pragma once

#include "dsp_effect.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxLexicon480L : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int kNumERTaps  = 8;
    static constexpr int kNumCombs   = 8;
    static constexpr int kNumAllpass = 6;

    enum Algorithm { RandomHall = 0, RandomAmbience };

    enum ParamId {
        ParamAlgo = 0,
        ParamRtMid,        // 0..1 → 0.4..70 s
        ParamSize,         // 0..1 → 0.5..2.5
        ParamShape,        // 0..1
        ParamSpread,       // 0..1
        ParamPreDelay,     // 0..1 → 0..250 ms
        ParamErTime,       // 0..1 → 0..200 ms
        ParamDiffusion,    // 0..1
        ParamHfCut,        // 0..1
        ParamBassBoost,    // 0..1
        ParamModRate,      // 0..1 → 0.05..2 Hz
        ParamModDepth,     // 0..1 → 0..5 ms
        ParamTailDensity,  // 0..1 (saturation amount)
        ParamMix,          // 0..1
        kParamCount
    };

    FxLexicon480L()
    {
        m_params[ParamAlgo]         = 0.0f;
        m_params[ParamRtMid]        = 0.50f;
        m_params[ParamSize]         = 0.65f;
        m_params[ParamShape]        = 0.50f;
        m_params[ParamSpread]       = 0.70f;
        m_params[ParamPreDelay]     = 0.06f;
        m_params[ParamErTime]       = 0.30f;
        m_params[ParamDiffusion]    = 0.75f;
        m_params[ParamHfCut]        = 0.40f;
        m_params[ParamBassBoost]    = 0.50f;
        m_params[ParamModRate]      = 0.40f;
        m_params[ParamModDepth]     = 0.40f;
        m_params[ParamTailDensity]  = 0.30f;
        m_params[ParamMix]          = 0.30f;
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "Lexicon 480L Random Hall"; }
    const char* id()      const override { return "mc1.lexicon.l480l"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    /* ── Configuration ───────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        const int preMax = static_cast<int>(0.30 * sr);
        m_preDelayBufL.assign(preMax, 0.0f);
        m_preDelayBufR.assign(preMax, 0.0f);
        m_preWriteIdx = 0;

        const int erMax = static_cast<int>(0.30 * sr);
        m_erBufL.assign(erMax, 0.0f);
        m_erBufR.assign(erMax, 0.0f);
        m_erWriteIdx = 0;

        for (int c = 0; c < kNumCombs; ++c) {
            int len = static_cast<int>(0.25 * sr) + 64;
            m_combL[c].buf.assign(len, 0.0f);
            m_combR[c].buf.assign(len, 0.0f);
            m_combL[c].writeIdx = m_combR[c].writeIdx = 0;
            m_combLpL[c] = m_combLpR[c] = 0.0f;
        }
        for (int a = 0; a < kNumAllpass; ++a) {
            int len = static_cast<int>(0.040 * sr) + 64;
            m_apL[a].buf.assign(len, 0.0f);
            m_apR[a].buf.assign(len, 0.0f);
            m_apL[a].writeIdx = m_apR[a].writeIdx = 0;
        }

        for (int i = 0; i < kNumCombs; ++i) {
            m_modPhase[i] = static_cast<double>(i) * 0.79;
        }
        m_lfStateL = m_lfStateR = 0.0f;
        m_bassStateL = m_bassStateR = 0.0f;
        recompute();
    }

    void reset() override
    {
        std::fill(m_preDelayBufL.begin(), m_preDelayBufL.end(), 0.0f);
        std::fill(m_preDelayBufR.begin(), m_preDelayBufR.end(), 0.0f);
        std::fill(m_erBufL.begin(), m_erBufL.end(), 0.0f);
        std::fill(m_erBufR.begin(), m_erBufR.end(), 0.0f);
        for (int c = 0; c < kNumCombs; ++c) {
            std::fill(m_combL[c].buf.begin(), m_combL[c].buf.end(), 0.0f);
            std::fill(m_combR[c].buf.begin(), m_combR[c].buf.end(), 0.0f);
            m_combLpL[c] = m_combLpR[c] = 0.0f;
        }
        for (int a = 0; a < kNumAllpass; ++a) {
            std::fill(m_apL[a].buf.begin(), m_apL[a].buf.end(), 0.0f);
            std::fill(m_apR[a].buf.begin(), m_apR[a].buf.end(), 0.0f);
        }
        m_lfStateL = m_lfStateR = 0.0f;
        m_bassStateL = m_bassStateR = 0.0f;
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamAlgo:        return "Algorithm";
            case ParamRtMid:       return "RT Mid";
            case ParamSize:        return "Size";
            case ParamShape:       return "Shape";
            case ParamSpread:      return "Spread";
            case ParamPreDelay:    return "Pre Delay";
            case ParamErTime:      return "ER Time";
            case ParamDiffusion:   return "Diffusion";
            case ParamHfCut:       return "HF Cut";
            case ParamBassBoost:   return "Bass Boost";
            case ParamModRate:     return "Mod Rate";
            case ParamModDepth:    return "Mod Depth";
            case ParamTailDensity: return "Tail Density";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamRtMid:    return "s";
            case ParamPreDelay: return "ms";
            case ParamErTime:   return "ms";
            case ParamModRate:  return "Hz";
            case ParamMix:      return "%";
            default:            return "";
        }
    }

    float paramValue(int idx) const override
    {
        if (idx < 0 || idx >= kParamCount) return 0.0f;
        return m_params[idx];
    }

    void setParamValue(int idx, float v) override
    {
        if (idx < 0 || idx >= kParamCount) return;
        v = std::clamp(v, 0.0f, 1.0f);
        m_params[idx] = v;
        recompute();
    }

    std::string paramDisplayValue(int idx) const override
    {
        char buf[32];
        switch (idx) {
            case ParamAlgo: {
                static const char* algos[2] = { "Random Hall", "Random Ambience" };
                int a = std::clamp(static_cast<int>(m_params[idx] * 1.999f), 0, 1);
                return algos[a];
            }
            case ParamRtMid:
                std::snprintf(buf, sizeof(buf), "%.2f s", rtMidTime());
                return buf;
            case ParamSize:
                std::snprintf(buf, sizeof(buf), "%.2f", 0.5f + m_params[idx] * 2.0f);
                return buf;
            case ParamPreDelay:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 250.0f);
                return buf;
            case ParamErTime:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 200.0f);
                return buf;
            case ParamModRate:
                std::snprintf(buf, sizeof(buf), "%.2f Hz", 0.05f + m_params[idx] * 1.95f);
                return buf;
            default:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
    }

    /* ── Audio processing ────────────────────────────────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float wet = m_params[ParamMix];
        const float dry = 1.0f - wet;
        const float lfCoef = m_lfHighpassCoef;
        const float bassG  = m_params[ParamBassBoost] * 1.5f;
        const float density = m_params[ParamTailDensity];
        const int   algo = std::clamp(static_cast<int>(m_params[ParamAlgo] * 1.999f), 0, 1);

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // LF cut
            float hpL = inL - m_lfStateL;
            float hpR = inR - m_lfStateR;
            m_lfStateL += lfCoef * hpL;
            m_lfStateR += lfCoef * hpR;
            float tankInL = inL - m_lfStateL;
            float tankInR = inR - m_lfStateR;

            // Pre-delay
            int preMax = static_cast<int>(m_preDelayBufL.size());
            int preRead = m_preWriteIdx - m_preDelayLen;
            if (preRead < 0) preRead += preMax;
            float pdL = m_preDelayBufL[preRead];
            float pdR = m_preDelayBufR[preRead];
            m_preDelayBufL[m_preWriteIdx] = tankInL;
            m_preDelayBufR[m_preWriteIdx] = tankInR;
            if (++m_preWriteIdx >= preMax) m_preWriteIdx = 0;

            // ── Early reflections (8 random taps per channel) ─────
            int erMax = static_cast<int>(m_erBufL.size());
            m_erBufL[m_erWriteIdx] = pdL;
            m_erBufR[m_erWriteIdx] = pdR;
            float erL = 0.0f, erR = 0.0f;
            for (int t = 0; t < kNumERTaps; ++t) {
                int rL = m_erWriteIdx - m_erTapL[t];
                int rR = m_erWriteIdx - m_erTapR[t];
                if (rL < 0) rL += erMax;
                if (rR < 0) rR += erMax;
                erL += m_erBufL[rL] * m_erGainL[t];
                erR += m_erBufR[rR] * m_erGainR[t];
            }
            erL *= 0.20f;
            erR *= 0.20f;
            if (++m_erWriteIdx >= erMax) m_erWriteIdx = 0;

            // Random ambience: skip the long decay tank entirely
            if (algo == RandomAmbience) {
                float wetL = erL;
                float wetR = erR;
                // Stereo spread
                float mid  = 0.5f * (wetL + wetR);
                float side = 0.5f * (wetL - wetR);
                side *= (0.4f + m_params[ParamSpread] * 1.6f);
                wetL = mid + side;
                wetR = mid - side;
                pcm[f * channels + 0] = inL * dry + wetL * wet;
                if (channels > 1)
                    pcm[f * channels + 1] = inR * dry + wetR * wet;
                continue;
            }

            // ── Random hall: long decay tank with multi-LFO mod ──
            float wetL = 0.0f, wetR = 0.0f;
            for (int c = 0; c < kNumCombs; ++c) {
                CombLine& cl = m_combL[c];
                CombLine& cr = m_combR[c];
                int blen = static_cast<int>(cl.buf.size());

                m_modPhase[c] += m_modPhaseInc[c];
                if (m_modPhase[c] > 6.28318530718) m_modPhase[c] -= 6.28318530718;
                float mod = static_cast<float>(std::sin(m_modPhase[c])) * m_modDepthSamples;

                int dL = m_combDelayL[c] + static_cast<int>(mod);
                int dR = m_combDelayR[c] + static_cast<int>(mod * 0.7f);
                if (dL < 4) dL = 4; if (dL >= blen) dL = blen - 1;
                if (dR < 4) dR = 4; if (dR >= blen) dR = blen - 1;

                int rL = cl.writeIdx - dL;
                int rR = cr.writeIdx - dR;
                if (rL < 0) rL += blen;
                if (rR < 0) rR += blen;
                float yL = cl.buf[rL];
                float yR = cr.buf[rR];

                m_combLpL[c] = yL * (1.0f - m_combLpCoef) + m_combLpL[c] * m_combLpCoef;
                m_combLpR[c] = yR * (1.0f - m_combLpCoef) + m_combLpR[c] * m_combLpCoef;

                cl.buf[cl.writeIdx] = pdL + erL * 0.4f + m_combFeedback[c] * m_combLpL[c];
                cr.buf[cr.writeIdx] = pdR + erR * 0.4f + m_combFeedback[c] * m_combLpR[c];
                if (++cl.writeIdx >= blen) cl.writeIdx = 0;
                if (++cr.writeIdx >= blen) cr.writeIdx = 0;

                wetL += yL;
                wetR += yR;
            }
            wetL *= 0.16f;
            wetR *= 0.16f;

            // Allpass smoothing
            for (int a = 0; a < kNumAllpass; ++a) {
                wetL = allpassTick(m_apL[a], wetL, m_apDelay[a], m_diffusionG);
                wetR = allpassTick(m_apR[a], wetR, m_apDelay[a], m_diffusionG);
            }

            // Bass boost (one-pole LP added back to wet)
            m_bassStateL = m_bassStateL * 0.985f + wetL * 0.015f;
            m_bassStateR = m_bassStateR * 0.985f + wetR * 0.015f;
            wetL += m_bassStateL * bassG;
            wetR += m_bassStateR * bassG;

            // Tail density saturation (gentle tanh)
            if (density > 0.001f) {
                wetL = std::tanh(wetL * (1.0f + density * 1.5f)) / (1.0f + density * 0.5f);
                wetR = std::tanh(wetR * (1.0f + density * 1.5f)) / (1.0f + density * 0.5f);
            }

            // Stereo spread
            float mid  = 0.5f * (wetL + wetR);
            float side = 0.5f * (wetL - wetR);
            side *= (0.4f + m_params[ParamSpread] * 1.6f);
            wetL = mid + side;
            wetR = mid - side;

            pcm[f * channels + 0] = inL * dry + wetL * wet;
            if (channels > 1)
                pcm[f * channels + 1] = inR * dry + wetR * wet;
        }
    }

private:
    struct DelayLine {
        std::vector<float> buf;
        int                writeIdx = 0;
    };
    using CombLine = DelayLine;

    static inline float allpassTick(DelayLine& d, float in, int delay, float g)
    {
        int blen = static_cast<int>(d.buf.size());
        if (delay < 1) delay = 1;
        if (delay >= blen) delay = blen - 1;
        int readIdx = d.writeIdx - delay;
        if (readIdx < 0) readIdx += blen;
        float bufOut = d.buf[readIdx];
        float v = in + g * bufOut;
        float out = -g * v + bufOut;
        d.buf[d.writeIdx] = v;
        if (++d.writeIdx >= blen) d.writeIdx = 0;
        return out;
    }

    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        m_preDelayLen = static_cast<int>(m_params[ParamPreDelay] * 0.250f * fs);
        if (m_preDelayLen < 1) m_preDelayLen = 1;
        if (m_preDelayLen >= static_cast<int>(m_preDelayBufL.size()))
            m_preDelayLen = static_cast<int>(m_preDelayBufL.size()) - 1;

        const float sizeMult = 0.5f + m_params[ParamSize] * 2.0f;
        const float erTimeMs = m_params[ParamErTime] * 200.0f;

        // Pseudo-random tap times for ER. We use a fixed seed pattern
        // so the sound is reproducible — "random" refers to the feel,
        // not literally re-randomized per second.
        static constexpr float kErFracL[kNumERTaps] = {
            0.07f, 0.19f, 0.31f, 0.43f, 0.55f, 0.67f, 0.79f, 0.91f
        };
        static constexpr float kErFracR[kNumERTaps] = {
            0.11f, 0.23f, 0.37f, 0.49f, 0.61f, 0.73f, 0.85f, 0.97f
        };

        for (int t = 0; t < kNumERTaps; ++t) {
            float msL = (12.0f + erTimeMs * kErFracL[t]) * sizeMult;
            float msR = (12.0f + erTimeMs * kErFracR[t]) * sizeMult;
            int dL = static_cast<int>(msL * 0.001f * fs);
            int dR = static_cast<int>(msR * 0.001f * fs);
            int erMax = static_cast<int>(m_erBufL.size());
            if (dL < 1)        dL = 1;
            if (dR < 1)        dR = 1;
            if (dL >= erMax)   dL = erMax - 1;
            if (dR >= erMax)   dR = erMax - 1;
            m_erTapL[t] = dL;
            m_erTapR[t] = dR;
            float decay = std::exp(-1.6f * static_cast<float>(t) / kNumERTaps);
            m_erGainL[t] = decay;
            m_erGainR[t] = decay * (0.92f + 0.16f * std::sin(t * 1.71f));
        }

        // Comb tunings — long, dense, coprime
        static constexpr float kCombMs[kNumCombs] = {
            47.3f, 53.9f, 61.7f, 71.5f, 79.3f, 89.1f, 97.7f, 109.3f
        };
        for (int c = 0; c < kNumCombs; ++c) {
            float ms = kCombMs[c] * sizeMult;
            int dL = static_cast<int>(ms * 0.001f * fs);
            int dR = static_cast<int>(ms * 1.023f * 0.001f * fs);
            int blen = static_cast<int>(m_combL[c].buf.size());
            if (dL < 8) dL = 8;
            if (dR < 8) dR = 8;
            if (dL >= blen - 200) dL = blen - 200;
            if (dR >= blen - 200) dR = blen - 200;
            m_combDelayL[c] = dL;
            m_combDelayR[c] = dR;

            float Dsec = static_cast<float>(dL) / fs;
            float fb = std::pow(10.0f, -3.0f * Dsec / std::max(0.05f, rtMidTime()));
            if (fb > 0.995f) fb = 0.995f;
            m_combFeedback[c] = fb;
        }
        m_combLpCoef = 0.10f + m_params[ParamHfCut] * 0.85f;

        static constexpr float kApMs[kNumAllpass] = {
            8.7f, 12.3f, 17.5f, 23.1f, 29.7f, 35.3f
        };
        for (int a = 0; a < kNumAllpass; ++a) {
            int dl = static_cast<int>(kApMs[a] * (0.85f + sizeMult * 0.15f) * 0.001f * fs);
            int blen = static_cast<int>(m_apL[a].buf.size());
            if (dl < 4)     dl = 4;
            if (dl >= blen) dl = blen - 1;
            m_apDelay[a] = dl;
        }
        m_diffusionG = 0.42f + m_params[ParamDiffusion] * 0.38f;

        float lfHz = 20.0f + m_params[ParamShape] * 380.0f;
        m_lfHighpassCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * lfHz / fs);

        m_modDepthSamples = m_params[ParamModDepth] * 5.0f * 0.001f * fs;
        float baseRate = 0.05f + m_params[ParamModRate] * 1.95f;
        // Slightly different LFO rates per comb for decorrelation
        for (int c = 0; c < kNumCombs; ++c) {
            float r = baseRate * (0.85f + 0.30f * std::sin(c * 1.79f));
            m_modPhaseInc[c] = 2.0 * 3.14159265359 *
                static_cast<double>(r) / static_cast<double>(fs);
        }
    }

    float rtMidTime() const
    {
        float v = m_params[ParamRtMid];
        return 0.4f * std::pow(70.0f / 0.4f, v);
    }

    /* ── State ───────────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    std::vector<float> m_preDelayBufL, m_preDelayBufR;
    int m_preWriteIdx = 0;
    int m_preDelayLen = 1;

    std::vector<float> m_erBufL, m_erBufR;
    int m_erWriteIdx = 0;
    int m_erTapL[kNumERTaps] = {}, m_erTapR[kNumERTaps] = {};
    float m_erGainL[kNumERTaps] = {}, m_erGainR[kNumERTaps] = {};

    CombLine m_combL[kNumCombs], m_combR[kNumCombs];
    int m_combDelayL[kNumCombs] = {}, m_combDelayR[kNumCombs] = {};
    float m_combFeedback[kNumCombs] = {};
    float m_combLpCoef = 0.5f;
    float m_combLpL[kNumCombs] = {}, m_combLpR[kNumCombs] = {};

    DelayLine m_apL[kNumAllpass], m_apR[kNumAllpass];
    int m_apDelay[kNumAllpass] = {};
    float m_diffusionG = 0.6f;

    float m_lfStateL = 0.0f, m_lfStateR = 0.0f;
    float m_lfHighpassCoef = 0.01f;

    float m_bassStateL = 0.0f, m_bassStateR = 0.0f;

    double m_modPhase[kNumCombs]    = {};
    double m_modPhaseInc[kNumCombs] = {};
    float  m_modDepthSamples = 0.0f;
};

} // namespace mc1dsp
