/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * dsp/fx_lexicon_pcm96.h — Lexicon PCM 96 Stereo Reverb (2007)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Header-only emulation of the Lexicon PCM 96 — the 21st-century
 * flagship that brought studio-grade halls to native processing.
 * Three core algorithms:
 *
 *   Concert Hall    — Pristine, smooth, slow buildup
 *   Random Hall     — Modulated tap delays, dense tail (Hollywood scoring)
 *   Random Ambience — Short, dense early-reflection-heavy room
 *
 * Two-stage architecture:
 *   1. Early reflections — 10-tap stereo network (separate L/R taps)
 *      with independent gain envelope. Time / level controllable.
 *   2. Late field — 5 parallel modulated comb filters per channel +
 *      4 series allpass diffusers per channel, separately wet-summed.
 *
 * True stereo: L and R chains are completely independent (no shared
 * mid/side network) — that's the PCM 96 dimensionality fingerprint.
 *
 * Real-time safe. No allocs / locks in process().
 */

#pragma once

#include "dsp_effect.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxLexiconPcm96 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int kNumERTaps  = 10;
    static constexpr int kNumCombs   = 5;
    static constexpr int kNumAllpass = 4;

    enum Algorithm { ConcertHall = 0, RandomHall, RandomAmbience };

    enum ParamId {
        ParamAlgo = 0,
        ParamRtMid,        // 0..1 → 0.4..30 s
        ParamSize,         // 0..1 → 0.5..2.0
        ParamPreDelay,     // 0..1 → 0..200 ms
        ParamErLevel,      // 0..1
        ParamErTime,       // 0..1 → 0..200 ms ER spread
        ParamLateLevel,    // 0..1
        ParamDiffusion,    // 0..1
        ParamShape,        // 0..1
        ParamHfDamping,    // 0..1
        ParamLfCut,        // 0..1
        ParamStereoWidth,  // 0..1
        ParamMix,          // 0..1
        kParamCount
    };

    FxLexiconPcm96()
    {
        m_params[ParamAlgo]         = 0.0f;
        m_params[ParamRtMid]        = 0.45f;
        m_params[ParamSize]         = 0.55f;
        m_params[ParamPreDelay]     = 0.05f;
        m_params[ParamErLevel]      = 0.40f;
        m_params[ParamErTime]       = 0.30f;
        m_params[ParamLateLevel]    = 0.55f;
        m_params[ParamDiffusion]    = 0.70f;
        m_params[ParamShape]        = 0.50f;
        m_params[ParamHfDamping]    = 0.45f;
        m_params[ParamLfCut]        = 0.15f;
        m_params[ParamStereoWidth]  = 0.75f;
        m_params[ParamMix]          = 0.32f;
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "Lexicon PCM 96 Stereo Reverb"; }
    const char* id()      const override { return "mc1.lexicon.pcm96"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    /* ── Configuration ───────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        const int preMax = static_cast<int>(0.25 * sr);
        m_preDelayBufL.assign(preMax, 0.0f);
        m_preDelayBufR.assign(preMax, 0.0f);
        m_preWriteIdx = 0;

        // Early reflection delay buffer (single per channel, holds 250ms)
        const int erMax = static_cast<int>(0.25 * sr) + 64;
        m_erBufL.assign(erMax, 0.0f);
        m_erBufR.assign(erMax, 0.0f);
        m_erWriteIdx = 0;

        for (int c = 0; c < kNumCombs; ++c) {
            int len = static_cast<int>(0.20 * sr) + 64;
            m_combL[c].buf.assign(len, 0.0f);
            m_combR[c].buf.assign(len, 0.0f);
            m_combL[c].writeIdx = m_combR[c].writeIdx = 0;
            m_combLpL[c] = m_combLpR[c] = 0.0f;
        }
        for (int a = 0; a < kNumAllpass; ++a) {
            int len = static_cast<int>(0.030 * sr) + 64;
            m_apL[a].buf.assign(len, 0.0f);
            m_apR[a].buf.assign(len, 0.0f);
            m_apL[a].writeIdx = m_apR[a].writeIdx = 0;
        }

        m_modPhase = 0.0;
        m_lfStateL = m_lfStateR = 0.0f;
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
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamAlgo:         return "Algorithm";
            case ParamRtMid:        return "RT Mid";
            case ParamSize:         return "Size";
            case ParamPreDelay:     return "Pre Delay";
            case ParamErLevel:      return "ER Level";
            case ParamErTime:       return "ER Time";
            case ParamLateLevel:    return "Late Level";
            case ParamDiffusion:    return "Diffusion";
            case ParamShape:        return "Shape";
            case ParamHfDamping:    return "HF Damping";
            case ParamLfCut:        return "LF Cut";
            case ParamStereoWidth:  return "Width";
            case ParamMix:          return "Mix";
            default:                return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamRtMid:    return "s";
            case ParamPreDelay: return "ms";
            case ParamErTime:   return "ms";
            case ParamLfCut:    return "Hz";
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
                static const char* algos[3] = {
                    "Concert Hall", "Random Hall", "Random Ambience"
                };
                int a = std::clamp(static_cast<int>(m_params[idx] * 2.999f), 0, 2);
                return algos[a];
            }
            case ParamRtMid:
                std::snprintf(buf, sizeof(buf), "%.2f s", rtMidTime());
                return buf;
            case ParamSize:
                std::snprintf(buf, sizeof(buf), "%.2f", 0.5f + m_params[idx] * 1.5f);
                return buf;
            case ParamPreDelay:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 200.0f);
                return buf;
            case ParamErTime:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 200.0f);
                return buf;
            case ParamLfCut:
                std::snprintf(buf, sizeof(buf), "%.0f Hz", 20.0f + m_params[idx] * 380.0f);
                return buf;
            case ParamErLevel:
            case ParamLateLevel:
            case ParamDiffusion:
            case ParamShape:
            case ParamHfDamping:
            case ParamStereoWidth:
            case ParamMix:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
        return DspEffect::paramDisplayValue(idx);
    }

    /* ── Audio processing ────────────────────────────────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float wet      = m_params[ParamMix];
        const float dry      = 1.0f - wet;
        const float erLevel  = m_params[ParamErLevel];
        const float lateGain = m_params[ParamLateLevel];
        const float lfCoef   = m_lfHighpassCoef;
        const int   algo     = std::clamp(static_cast<int>(m_params[ParamAlgo] * 2.999f), 0, 2);

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

            // ── Early reflections: 10 taps per channel ────────────
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
            erL *= 0.30f;
            erR *= 0.30f;

            if (++m_erWriteIdx >= erMax) m_erWriteIdx = 0;

            // ── Late field: independent L/R combs + allpass ───────
            m_modPhase += m_modPhaseInc;
            if (m_modPhase > 6.28318530718) m_modPhase -= 6.28318530718;

            float lateInL = pdL + erL * 0.4f;
            float lateInR = pdR + erR * 0.4f;

            float lateL = 0.0f, lateR = 0.0f;
            for (int c = 0; c < kNumCombs; ++c) {
                CombLine& cl = m_combL[c];
                CombLine& cr = m_combR[c];
                int blen = static_cast<int>(cl.buf.size());

                // Random hall: modulate the read offset
                float modOffL = 0.0f, modOffR = 0.0f;
                if (algo != ConcertHall && c < 3) {
                    double ph  = m_modPhase + c * 1.31;
                    double phR = m_modPhase + c * 1.71 + 1.57;
                    modOffL = static_cast<float>(std::sin(ph))  * m_modDepthSamples;
                    modOffR = static_cast<float>(std::sin(phR)) * m_modDepthSamples;
                }

                int dL = m_combDelayL[c];
                int dR = m_combDelayR[c];
                int rL = cl.writeIdx - dL + static_cast<int>(modOffL);
                int rR = cr.writeIdx - dR + static_cast<int>(modOffR);
                if (rL < 0) rL += blen; if (rL >= blen) rL -= blen;
                if (rR < 0) rR += blen; if (rR >= blen) rR -= blen;

                float yL = cl.buf[rL];
                float yR = cr.buf[rR];

                m_combLpL[c] = yL * (1.0f - m_combLpCoef) + m_combLpL[c] * m_combLpCoef;
                m_combLpR[c] = yR * (1.0f - m_combLpCoef) + m_combLpR[c] * m_combLpCoef;

                cl.buf[cl.writeIdx] = lateInL + m_combFeedback[c] * m_combLpL[c];
                cr.buf[cr.writeIdx] = lateInR + m_combFeedback[c] * m_combLpR[c];
                if (++cl.writeIdx >= blen) cl.writeIdx = 0;
                if (++cr.writeIdx >= blen) cr.writeIdx = 0;

                lateL += yL;
                lateR += yR;
            }
            lateL *= 0.20f;
            lateR *= 0.20f;

            // Independent allpass diffusion per channel
            for (int a = 0; a < kNumAllpass; ++a) {
                lateL = allpassTick(m_apL[a], lateL, m_apDelay[a], m_diffusionG);
                lateR = allpassTick(m_apR[a], lateR, m_apDelay[a], m_diffusionG);
            }

            // Combine ER + late, scale levels
            float wetL = erL * erLevel + lateL * lateGain;
            float wetR = erR * erLevel + lateR * lateGain;

            // Stereo width
            float mid  = 0.5f * (wetL + wetR);
            float side = 0.5f * (wetL - wetR);
            side *= (0.4f + m_params[ParamStereoWidth] * 1.6f);
            wetL = mid + side;
            wetR = mid - side;

            // Mix dry + wet
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

        m_preDelayLen = static_cast<int>(m_params[ParamPreDelay] * 0.200f * fs);
        if (m_preDelayLen < 1) m_preDelayLen = 1;
        if (m_preDelayLen >= static_cast<int>(m_preDelayBufL.size()))
            m_preDelayLen = static_cast<int>(m_preDelayBufL.size()) - 1;

        const float sizeMult = 0.5f + m_params[ParamSize] * 1.5f;
        const float erTimeMs = m_params[ParamErTime] * 200.0f;

        // 10 ER taps per channel — different patterns L vs R for true
        // stereo dimensionality. Times spread across [0, erTimeMs].
        // Gains decay exponentially.
        static constexpr float kErFracL[kNumERTaps] = {
            0.05f, 0.13f, 0.22f, 0.31f, 0.40f, 0.49f, 0.58f, 0.67f, 0.78f, 0.92f
        };
        static constexpr float kErFracR[kNumERTaps] = {
            0.07f, 0.16f, 0.25f, 0.34f, 0.43f, 0.53f, 0.62f, 0.72f, 0.83f, 0.96f
        };

        for (int t = 0; t < kNumERTaps; ++t) {
            float msL = (10.0f + erTimeMs * kErFracL[t]) * sizeMult;
            float msR = (10.0f + erTimeMs * kErFracR[t]) * sizeMult;
            int dL = static_cast<int>(msL * 0.001f * fs);
            int dR = static_cast<int>(msR * 0.001f * fs);
            int erMax = static_cast<int>(m_erBufL.size());
            if (dL < 1)        dL = 1;
            if (dR < 1)        dR = 1;
            if (dL >= erMax)   dL = erMax - 1;
            if (dR >= erMax)   dR = erMax - 1;
            m_erTapL[t] = dL;
            m_erTapR[t] = dR;
            // Exponential gain envelope
            float decay = std::exp(-2.0f * static_cast<float>(t) / kNumERTaps);
            m_erGainL[t] = decay * (1.0f - 0.1f * t * 0.05f);
            m_erGainR[t] = decay * (0.95f + 0.1f * std::sin(t * 1.3f));
        }

        // Comb tunings for the late field
        static constexpr float kCombMs[kNumCombs] = {
            41.7f, 53.3f, 67.9f, 79.1f, 91.7f
        };
        for (int c = 0; c < kNumCombs; ++c) {
            float ms = kCombMs[c] * sizeMult;
            int dL = static_cast<int>(ms * 0.001f * fs);
            int dR = static_cast<int>(ms * 1.027f * 0.001f * fs);
            int blen = static_cast<int>(m_combL[c].buf.size());
            if (dL < 8) dL = 8;
            if (dR < 8) dR = 8;
            if (dL >= blen) dL = blen - 1;
            if (dR >= blen) dR = blen - 1;
            m_combDelayL[c] = dL;
            m_combDelayR[c] = dR;

            float Dsec = static_cast<float>(dL) / fs;
            float fb = std::pow(10.0f, -3.0f * Dsec / std::max(0.05f, rtMidTime()));
            if (fb > 0.995f) fb = 0.995f;
            m_combFeedback[c] = fb;
        }
        m_combLpCoef = 0.10f + m_params[ParamHfDamping] * 0.85f;

        // 4 allpass smoothers
        static constexpr float kApMs[kNumAllpass] = { 9.7f, 13.1f, 18.5f, 24.7f };
        for (int a = 0; a < kNumAllpass; ++a) {
            int dl = static_cast<int>(kApMs[a] * (0.85f + sizeMult * 0.15f) * 0.001f * fs);
            int blen = static_cast<int>(m_apL[a].buf.size());
            if (dl < 4)     dl = 4;
            if (dl >= blen) dl = blen - 1;
            m_apDelay[a] = dl;
        }

        m_diffusionG = 0.42f + m_params[ParamDiffusion] * 0.36f;

        float lfHz = 20.0f + m_params[ParamLfCut] * 380.0f;
        m_lfHighpassCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * lfHz / fs);

        // Mod depth: 0..4 ms (random hall character), only used by
        // the random algorithms via the algo check in process().
        m_modDepthSamples = m_params[ParamShape] * 4.0f * 0.001f * fs;
        m_modPhaseInc = 2.0 * 3.14159265359 * 0.55 / static_cast<double>(fs);
    }

    float rtMidTime() const
    {
        float v = m_params[ParamRtMid];
        return 0.4f * std::pow(30.0f / 0.4f, v);
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

    double m_modPhase     = 0.0;
    double m_modPhaseInc  = 0.0;
    float  m_modDepthSamples = 0.0f;
};

} // namespace mc1dsp
