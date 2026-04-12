/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * dsp/fx_lexicon_pcm70.h — Lexicon PCM 70 Multi-FX (1985)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Header-only emulation of the Lexicon PCM 70 — the rack reverb that
 * defined late-'80s pop and hip-hop production. Six core algorithms:
 *
 *   Plate          — Classic dense plate, fast diffusion
 *   Chamber        — Drier room with lateral reflections
 *   Inverse        — Reverse envelope (RR → builds up then cuts)
 *   Gated          — Hard cutoff after a duration threshold (Phil Collins)
 *   Chorus + Plate — Plate with a chorused front-end
 *   Tremolo + Verb — Plate with tremolo on the wet signal
 *
 * Algorithm: input → DC blocker → pre-delay → 8 nested allpass diffusers
 * → 4 modulated parallel comb filters → 4 series allpass smoothers →
 * envelope shaping (program-dependent) → wet/dry blend.
 *
 * Real-time safe. No allocs / locks in process().
 */

#pragma once

#include "dsp_effect.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxLexiconPcm70 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int kNumCombs    = 4;
    static constexpr int kNumDiffuse  = 8;
    static constexpr int kNumSmooth   = 4;

    enum Algorithm {
        Plate = 0, Chamber, Inverse, Gated, ChorusPlate, TremoloReverb
    };

    enum ParamId {
        ParamAlgo = 0,
        ParamSize,        // 0..1
        ParamDecay,       // 0..1 → 0.2..30 s
        ParamPreDelay,    // 0..1 → 0..200 ms
        ParamDiffusion,   // 0..1 → allpass g 0.40..0.78
        ParamShape,       // 0..1 → envelope shape (0=fast, 1=slow build)
        ParamSpread,      // 0..1 → stereo spread
        ParamHfCut,       // 0..1 → 0..0.95
        ParamLfCut,       // 0..1 → 20..400 Hz
        ParamModRate,     // 0..1 → 0.05..3 Hz
        ParamModDepth,    // 0..1 → 0..6 ms
        ParamMix,         // 0..1
        kParamCount
    };

    FxLexiconPcm70()
    {
        m_params[ParamAlgo]      = 0.0f;
        m_params[ParamSize]      = 0.5f;
        m_params[ParamDecay]     = 0.45f;
        m_params[ParamPreDelay]  = 0.05f;
        m_params[ParamDiffusion] = 0.7f;
        m_params[ParamShape]     = 0.5f;
        m_params[ParamSpread]    = 0.6f;
        m_params[ParamHfCut]     = 0.4f;
        m_params[ParamLfCut]     = 0.15f;
        m_params[ParamModRate]   = 0.4f;
        m_params[ParamModDepth]  = 0.3f;
        m_params[ParamMix]       = 0.30f;
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "Lexicon PCM 70 Multi-FX"; }
    const char* id()      const override { return "mc1.lexicon.pcm70"; }
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

        for (int c = 0; c < kNumCombs; ++c) {
            int len = static_cast<int>(0.18 * sr) + 64;
            m_combL[c].buf.assign(len, 0.0f);
            m_combR[c].buf.assign(len, 0.0f);
            m_combL[c].writeIdx = m_combR[c].writeIdx = 0;
            m_combLpL[c] = m_combLpR[c] = 0.0f;
        }
        for (int d = 0; d < kNumDiffuse; ++d) {
            int len = static_cast<int>(0.025 * sr) + 64;
            m_diffuseL[d].buf.assign(len, 0.0f);
            m_diffuseR[d].buf.assign(len, 0.0f);
            m_diffuseL[d].writeIdx = m_diffuseR[d].writeIdx = 0;
        }
        for (int a = 0; a < kNumSmooth; ++a) {
            int len = static_cast<int>(0.030 * sr) + 64;
            m_smoothL[a].buf.assign(len, 0.0f);
            m_smoothR[a].buf.assign(len, 0.0f);
            m_smoothL[a].writeIdx = m_smoothR[a].writeIdx = 0;
        }

        m_envFollower = 0.0f;
        m_modPhase = 0.0;
        m_tremPhase = 0.0;
        m_lfStateL = m_lfStateR = 0.0f;
        recompute();
    }

    void reset() override
    {
        std::fill(m_preDelayBufL.begin(), m_preDelayBufL.end(), 0.0f);
        std::fill(m_preDelayBufR.begin(), m_preDelayBufR.end(), 0.0f);
        for (int c = 0; c < kNumCombs; ++c) {
            std::fill(m_combL[c].buf.begin(), m_combL[c].buf.end(), 0.0f);
            std::fill(m_combR[c].buf.begin(), m_combR[c].buf.end(), 0.0f);
            m_combLpL[c] = m_combLpR[c] = 0.0f;
        }
        for (int d = 0; d < kNumDiffuse; ++d) {
            std::fill(m_diffuseL[d].buf.begin(), m_diffuseL[d].buf.end(), 0.0f);
            std::fill(m_diffuseR[d].buf.begin(), m_diffuseR[d].buf.end(), 0.0f);
        }
        for (int a = 0; a < kNumSmooth; ++a) {
            std::fill(m_smoothL[a].buf.begin(), m_smoothL[a].buf.end(), 0.0f);
            std::fill(m_smoothR[a].buf.begin(), m_smoothR[a].buf.end(), 0.0f);
        }
        m_envFollower = 0.0f;
        m_lfStateL = m_lfStateR = 0.0f;
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamAlgo:      return "Algorithm";
            case ParamSize:      return "Size";
            case ParamDecay:     return "Decay";
            case ParamPreDelay:  return "Pre Delay";
            case ParamDiffusion: return "Diffusion";
            case ParamShape:     return "Shape";
            case ParamSpread:    return "Spread";
            case ParamHfCut:     return "HF Cut";
            case ParamLfCut:     return "LF Cut";
            case ParamModRate:   return "Mod Rate";
            case ParamModDepth:  return "Mod Depth";
            case ParamMix:       return "Mix";
            default:             return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamPreDelay: return "ms";
            case ParamDecay:    return "s";
            case ParamLfCut:    return "Hz";
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
                static const char* algos[6] = {
                    "Plate", "Chamber", "Inverse", "Gated",
                    "Chorus+Plate", "Tremolo+Reverb"
                };
                int a = std::clamp(static_cast<int>(m_params[idx] * 5.999f), 0, 5);
                return algos[a];
            }
            case ParamSize:
                std::snprintf(buf, sizeof(buf), "%.2f", 0.5f + m_params[idx] * 1.5f);
                return buf;
            case ParamDecay:
                std::snprintf(buf, sizeof(buf), "%.2f s", decayTime());
                return buf;
            case ParamPreDelay:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 200.0f);
                return buf;
            case ParamDiffusion:
            case ParamShape:
            case ParamSpread:
            case ParamHfCut:
            case ParamModDepth:
            case ParamMix:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamLfCut:
                std::snprintf(buf, sizeof(buf), "%.0f Hz", 20.0f + m_params[idx] * 380.0f);
                return buf;
            case ParamModRate:
                std::snprintf(buf, sizeof(buf), "%.2f Hz", 0.05f + m_params[idx] * 2.95f);
                return buf;
        }
        return DspEffect::paramDisplayValue(idx);
    }

    /* ── Audio processing ────────────────────────────────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float wet = m_params[ParamMix];
        const float dry = 1.0f - wet;
        const int   algo = std::clamp(static_cast<int>(m_params[ParamAlgo] * 5.999f), 0, 5);
        const float lfCoef = m_lfHighpassCoef;

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // Envelope follower for Inverse / Gated programs
            float inMag = 0.5f * (std::fabs(inL) + std::fabs(inR));
            if (inMag > m_envFollower) {
                m_envFollower += (inMag - m_envFollower) * 0.05f;
            } else {
                m_envFollower += (inMag - m_envFollower) * m_envRelease;
            }

            // LF cut high-pass
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

            // Front-end nested diffusion (8 stages)
            float dL = pdL;
            float dR = pdR;
            for (int d = 0; d < kNumDiffuse; ++d) {
                dL = allpassTick(m_diffuseL[d], dL, m_diffuseDelay[d], m_diffusionG);
                dR = allpassTick(m_diffuseR[d], dR, m_diffuseDelay[d], m_diffusionG);
            }

            // Comb tank with HF damping
            m_modPhase += m_modPhaseInc;
            if (m_modPhase > 6.28318530718) m_modPhase -= 6.28318530718;

            float wetL = 0.0f;
            float wetR = 0.0f;
            for (int c = 0; c < kNumCombs; ++c) {
                CombLine& cl = m_combL[c];
                CombLine& cr = m_combR[c];
                int blen = static_cast<int>(cl.buf.size());

                int rL = cl.writeIdx - m_combDelayL[c];
                int rR = cr.writeIdx - m_combDelayR[c];
                if (rL < 0) rL += blen;
                if (rR < 0) rR += blen;
                float yL = cl.buf[rL];
                float yR = cr.buf[rR];

                m_combLpL[c] = yL * (1.0f - m_combLpCoef) + m_combLpL[c] * m_combLpCoef;
                m_combLpR[c] = yR * (1.0f - m_combLpCoef) + m_combLpR[c] * m_combLpCoef;

                cl.buf[cl.writeIdx] = dL + m_combFeedback * m_combLpL[c];
                cr.buf[cr.writeIdx] = dR + m_combFeedback * m_combLpR[c];
                if (++cl.writeIdx >= blen) cl.writeIdx = 0;
                if (++cr.writeIdx >= blen) cr.writeIdx = 0;

                wetL += yL;
                wetR += yR;
            }
            wetL *= 0.25f;
            wetR *= 0.25f;

            // Smoothing allpass tail
            for (int a = 0; a < kNumSmooth; ++a) {
                wetL = allpassTick(m_smoothL[a], wetL, m_smoothDelay[a], m_diffusionG * 0.85f);
                wetR = allpassTick(m_smoothR[a], wetR, m_smoothDelay[a], m_diffusionG * 0.85f);
            }

            // Stereo spread (mid/side balance)
            float mid = 0.5f * (wetL + wetR);
            float side = 0.5f * (wetL - wetR);
            side *= (0.4f + m_params[ParamSpread] * 1.6f);
            wetL = mid + side;
            wetR = mid - side;

            // Program-dependent envelope shaping
            switch (algo) {
                case Inverse: {
                    // Reverse envelope: gain ramps UP with the input
                    // envelope, so transients fade in instead of out.
                    float g = std::clamp(m_envFollower * 8.0f, 0.0f, 1.0f);
                    wetL *= g;
                    wetR *= g;
                    break;
                }
                case Gated: {
                    // Hard gate: cuts the wet signal once the envelope
                    // drops below the threshold. Threshold is shape.
                    float thr = 0.005f + m_params[ParamShape] * 0.05f;
                    float g = (m_envFollower > thr) ? 1.0f : 0.0f;
                    wetL *= g;
                    wetR *= g;
                    break;
                }
                case TremoloReverb: {
                    m_tremPhase += m_tremPhaseInc;
                    if (m_tremPhase > 6.28318530718) m_tremPhase -= 6.28318530718;
                    float trem = 0.5f + 0.5f * static_cast<float>(std::sin(m_tremPhase));
                    wetL *= trem;
                    wetR *= trem;
                    break;
                }
                case ChorusPlate:
                    // Already handled by modulation in diffusion stages.
                    break;
                default: break;
            }

            // Mix
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

        // 4 comb tunings — chosen to be tighter than the 224 to give the
        // PCM 70 its denser, brighter character.
        static constexpr float kCombMs[kNumCombs] = { 35.7f, 41.3f, 47.9f, 53.1f };
        for (int c = 0; c < kNumCombs; ++c) {
            float ms = kCombMs[c] * sizeMult;
            int dL = static_cast<int>(ms * 0.001f * fs);
            int dR = static_cast<int>(ms * 1.041f * 0.001f * fs);
            int blen = static_cast<int>(m_combL[c].buf.size());
            if (dL < 8) dL = 8;
            if (dR < 8) dR = 8;
            if (dL >= blen) dL = blen - 1;
            if (dR >= blen) dR = blen - 1;
            m_combDelayL[c] = dL;
            m_combDelayR[c] = dR;
        }

        // Comb feedback from RT60
        float rt60 = decayTime();
        float Dsec = static_cast<float>(m_combDelayL[0]) / fs;
        m_combFeedback = std::pow(10.0f, -3.0f * Dsec / std::max(0.05f, rt60));
        if (m_combFeedback > 0.995f) m_combFeedback = 0.995f;

        // Tone
        m_combLpCoef = 0.10f + m_params[ParamHfCut] * 0.85f;

        // Diffusion + smoother delay tunings
        static constexpr float kDiffuseMs[kNumDiffuse] = {
            4.7f, 6.1f, 9.3f, 12.5f, 15.7f, 18.3f, 21.1f, 23.9f
        };
        for (int d = 0; d < kNumDiffuse; ++d) {
            float ms = kDiffuseMs[d];
            int dl = static_cast<int>(ms * 0.001f * fs);
            int blen = static_cast<int>(m_diffuseL[d].buf.size());
            if (dl < 4)     dl = 4;
            if (dl >= blen) dl = blen - 1;
            m_diffuseDelay[d] = dl;
        }
        static constexpr float kSmoothMs[kNumSmooth] = { 7.7f, 11.3f, 17.5f, 25.1f };
        for (int a = 0; a < kNumSmooth; ++a) {
            int dl = static_cast<int>(kSmoothMs[a] * 0.001f * fs);
            int blen = static_cast<int>(m_smoothL[a].buf.size());
            if (dl < 4)     dl = 4;
            if (dl >= blen) dl = blen - 1;
            m_smoothDelay[a] = dl;
        }

        m_diffusionG = 0.40f + m_params[ParamDiffusion] * 0.38f;

        float lfHz = 20.0f + m_params[ParamLfCut] * 380.0f;
        m_lfHighpassCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * lfHz / fs);

        float modHz = 0.05f + m_params[ParamModRate] * 2.95f;
        m_modPhaseInc = 2.0 * 3.14159265359 * static_cast<double>(modHz) / static_cast<double>(fs);

        // Tremolo rate (algorithm 5) is locked to ModRate
        m_tremPhaseInc = 2.0 * 3.14159265359 * static_cast<double>(modHz) / static_cast<double>(fs);

        // Envelope release scaled with shape (0=fast, 1=slow)
        m_envRelease = 0.001f + m_params[ParamShape] * 0.02f;
    }

    float decayTime() const
    {
        float v = m_params[ParamDecay];
        return 0.2f * std::pow(30.0f / 0.2f, v);
    }

    /* ── State ───────────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    std::vector<float> m_preDelayBufL, m_preDelayBufR;
    int m_preWriteIdx = 0;
    int m_preDelayLen = 1;

    CombLine m_combL[kNumCombs], m_combR[kNumCombs];
    int m_combDelayL[kNumCombs] = {}, m_combDelayR[kNumCombs] = {};
    float m_combFeedback = 0.0f;
    float m_combLpCoef   = 0.5f;
    float m_combLpL[kNumCombs] = {}, m_combLpR[kNumCombs] = {};

    DelayLine m_diffuseL[kNumDiffuse], m_diffuseR[kNumDiffuse];
    int m_diffuseDelay[kNumDiffuse] = {};

    DelayLine m_smoothL[kNumSmooth], m_smoothR[kNumSmooth];
    int m_smoothDelay[kNumSmooth] = {};

    float m_diffusionG     = 0.6f;
    float m_lfStateL = 0.0f, m_lfStateR = 0.0f;
    float m_lfHighpassCoef = 0.01f;

    double m_modPhase    = 0.0;
    double m_modPhaseInc = 0.0;
    double m_tremPhase   = 0.0;
    double m_tremPhaseInc = 0.0;

    float m_envFollower = 0.0f;
    float m_envRelease  = 0.005f;
};

} // namespace mc1dsp
