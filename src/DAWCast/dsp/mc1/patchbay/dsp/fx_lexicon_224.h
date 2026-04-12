/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * dsp/fx_lexicon_224.h — Lexicon 224 Digital Reverb (1978)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Header-only emulation of the Lexicon 224 Digital Reverb — the
 * outboard rack box that defined the sound of '80s rock, pop, and
 * R&B production. Models the four core program types of the original
 * hardware: Concert Hall A, Concert Hall B, Plate, and Room.
 *
 * Algorithm:
 *   Input → DC-blocker → Pre-delay → 4 parallel modulated comb filters
 *           with one-pole low-pass feedback (per-band HF damping)
 *           → 2 series allpass diffusers → wet/dry blend.
 *
 * The structure is the classic Schroeder+Moorer reverb network used by
 * Lexicon's hardware: parallel combs build up density / decay time, the
 * series allpass smears the comb echoes into a dense tail, and a slow
 * LFO modulates one comb delay to break up periodicity (the "Lex shimmer").
 *
 * Real-time safe: all buffers are pre-allocated in setSampleRate().
 * No allocations and no locks on the audio thread.
 */

#pragma once

#include "dsp_effect.h"

#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace mc1dsp {

class FxLexicon224 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int kNumCombs    = 6;
    static constexpr int kNumAllpass  = 4;

    enum Program { Hall_A = 0, Hall_B = 1, Plate = 2, Room = 3 };

    enum ParamId {
        ParamProgram = 0,
        ParamPreDelay,        // 0..250 ms
        ParamDecay,           // 0.4..70 s (RT60)
        ParamSize,            // 0.5..2.0 (multiplies all delay times)
        ParamDiffusion,       // 0..1 → allpass g 0.40..0.78
        ParamHfDamping,       // 0..1 → 0..0.95 lowpass coefficient
        ParamLfCut,           // 0..1 → 20..400 Hz high-pass on input
        ParamBassMult,        // 0.5..2.0 (bass decay multiplier)
        ParamTrebleDecay,     // 0.2..1.0 (treble decay relative to mid)
        ParamModDepth,        // 0..1 → 0..3 ms modulation depth
        ParamMix,             // 0..1 dry/wet
        kParamCount
    };

    FxLexicon224()
    {
        // Default to Concert Hall A with classic Lexicon settings
        m_params[ParamProgram]     = 0.0f;          // Hall_A
        m_params[ParamPreDelay]    = 0.08f;         // ~20 ms
        m_params[ParamDecay]       = 0.45f;         // ~2.5 s
        m_params[ParamSize]        = 0.5f;          // 1.0 size mult
        m_params[ParamDiffusion]   = 0.7f;
        m_params[ParamHfDamping]   = 0.45f;
        m_params[ParamLfCut]       = 0.15f;
        m_params[ParamBassMult]    = 0.5f;          // 1.0
        m_params[ParamTrebleDecay] = 0.55f;         // ~0.6
        m_params[ParamModDepth]    = 0.35f;
        m_params[ParamMix]         = 0.30f;

        for (auto& s : m_combLpStateL) s = 0.0f;
        for (auto& s : m_combLpStateR) s = 0.0f;
    }

    /* ── DspEffect identity ──────────────────────────────────────── */

    const char* name()    const override { return "Lexicon 224 Digital Reverb"; }
    const char* id()      const override { return "mc1.lexicon.l224"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    /* ── Configuration ───────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        // Allocate pre-delay buffer (max 250 ms × 2 channels)
        const int preMax = static_cast<int>(0.30 * sr);
        m_preDelayBufL.assign(preMax, 0.0f);
        m_preDelayBufR.assign(preMax, 0.0f);
        m_preWriteIdx = 0;

        // Allocate comb buffers — pick the longest possible delay so we
        // can vary size at runtime without re-allocating.
        for (int c = 0; c < kNumCombs; ++c) {
            int len = static_cast<int>(0.16 * sr) + 64;  // 160 ms max + slack
            m_combL[c].buf.assign(len, 0.0f);
            m_combR[c].buf.assign(len, 0.0f);
            m_combL[c].writeIdx = 0;
            m_combR[c].writeIdx = 0;
        }

        // Allocate allpass diffuser buffers
        for (int a = 0; a < kNumAllpass; ++a) {
            int len = static_cast<int>(0.020 * sr) + 32;  // 20 ms max
            m_apL[a].buf.assign(len, 0.0f);
            m_apR[a].buf.assign(len, 0.0f);
            m_apL[a].writeIdx = 0;
            m_apR[a].writeIdx = 0;
        }

        m_modPhase = 0.0;
        m_dcL = m_dcR = 0.0f;
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
            m_combLpStateL[c] = m_combLpStateR[c] = 0.0f;
        }
        for (int a = 0; a < kNumAllpass; ++a) {
            std::fill(m_apL[a].buf.begin(), m_apL[a].buf.end(), 0.0f);
            std::fill(m_apR[a].buf.begin(), m_apR[a].buf.end(), 0.0f);
        }
        m_dcL = m_dcR = 0.0f;
        m_lfStateL = m_lfStateR = 0.0f;
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamProgram:     return "Program";
            case ParamPreDelay:    return "Pre Delay";
            case ParamDecay:       return "Decay";
            case ParamSize:        return "Size";
            case ParamDiffusion:   return "Diffusion";
            case ParamHfDamping:   return "HF Damping";
            case ParamLfCut:       return "LF Cut";
            case ParamBassMult:    return "Bass Mult";
            case ParamTrebleDecay: return "Treble Decay";
            case ParamModDepth:    return "Mod Depth";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamPreDelay:    return "ms";
            case ParamDecay:       return "s";
            case ParamLfCut:       return "Hz";
            case ParamHfDamping:   return "%";
            case ParamMix:         return "%";
            default:               return "";
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
            case ParamProgram: {
                static const char* progs[4] = {
                    "Hall A", "Hall B", "Plate", "Room"
                };
                int p = static_cast<int>(m_params[idx] * 3.999f);
                p = std::clamp(p, 0, 3);
                return progs[p];
            }
            case ParamPreDelay:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 250.0f);
                return buf;
            case ParamDecay: {
                float t = decayTime();
                if (t < 1.0f)
                    std::snprintf(buf, sizeof(buf), "%.0f ms", t * 1000.0f);
                else
                    std::snprintf(buf, sizeof(buf), "%.2f s", t);
                return buf;
            }
            case ParamSize:
                std::snprintf(buf, sizeof(buf), "%.2f", 0.5f + m_params[idx] * 1.5f);
                return buf;
            case ParamDiffusion:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamHfDamping:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamLfCut:
                std::snprintf(buf, sizeof(buf), "%.0f Hz", 20.0f + m_params[idx] * 380.0f);
                return buf;
            case ParamBassMult:
                std::snprintf(buf, sizeof(buf), "%.2fx", 0.5f + m_params[idx] * 1.5f);
                return buf;
            case ParamTrebleDecay:
                std::snprintf(buf, sizeof(buf), "%.2fx", 0.2f + m_params[idx] * 0.8f);
                return buf;
            case ParamModDepth:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 3.0f);
                return buf;
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

        const float wet = m_params[ParamMix];
        const float dry = 1.0f - wet;
        const float lfCoef = m_lfHighpassCoef;

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // ── DC blocker (very gentle high-pass at ~6 Hz) ────────
            float dcOutL = inL - m_dcL + 0.997f * 0.0f;
            float dcOutR = inR - m_dcR;
            m_dcL = inL * 0.003f + m_dcL * 0.997f;
            m_dcR = inR * 0.003f + m_dcR * 0.997f;
            float drySampL = dcOutL;
            float drySampR = dcOutR;
            (void)drySampL; (void)drySampR;

            // ── User LF cut (high-pass) on input feeding the tank ──
            float hpL = inL - m_lfStateL;
            float hpR = inR - m_lfStateR;
            m_lfStateL += lfCoef * hpL;
            m_lfStateR += lfCoef * hpR;
            float tankInL = inL - m_lfStateL;
            float tankInR = inR - m_lfStateR;

            // ── Pre-delay ──────────────────────────────────────────
            const int preMaxL = static_cast<int>(m_preDelayBufL.size());
            int preReadL = m_preWriteIdx - m_preDelayLen;
            if (preReadL < 0) preReadL += preMaxL;
            float pdL = m_preDelayBufL[preReadL];
            float pdR = m_preDelayBufR[preReadL];
            m_preDelayBufL[m_preWriteIdx] = tankInL;
            m_preDelayBufR[m_preWriteIdx] = tankInR;
            if (++m_preWriteIdx >= preMaxL) m_preWriteIdx = 0;

            // ── 4–6 parallel modulated comb filters ────────────────
            // LFO advances once per sample. One sinusoid drives all
            // combs in slight phase offsets so the modulation feels
            // organic, not periodic.
            m_modPhase += m_modPhaseInc;
            if (m_modPhase > 6.28318530718)
                m_modPhase -= 6.28318530718;

            float wetL = 0.0f;
            float wetR = 0.0f;
            for (int c = 0; c < kNumCombs; ++c) {
                CombLine& cl = m_combL[c];
                CombLine& cr = m_combR[c];

                // Modulated read offset (only on first 2 combs for that
                // classic Lex chorus shimmer; others are static)
                float modOff = 0.0f;
                if (c < 2) {
                    double ph = m_modPhase + c * 1.7;
                    modOff = static_cast<float>(std::sin(ph)) * m_modDepthSamples;
                }

                int delayL = m_combDelayL[c];
                int delayR = m_combDelayR[c];

                int readL = cl.writeIdx - delayL;
                int readR = cr.writeIdx - delayR;
                int blen  = static_cast<int>(cl.buf.size());
                if (readL < 0) readL += blen;
                if (readR < 0) readR += blen;

                // Linear interpolation for fractional modulation
                float fracL = modOff - std::floor(modOff);
                int   readL2 = readL - 1; if (readL2 < 0) readL2 += blen;
                float yL = cl.buf[readL] * (1.0f - fracL) + cl.buf[readL2] * fracL;
                float yR = cr.buf[readR];

                // One-pole low-pass on the feedback path = HF damping
                m_combLpStateL[c] = yL * (1.0f - m_combLpCoef[c]) +
                                    m_combLpStateL[c] * m_combLpCoef[c];
                m_combLpStateR[c] = yR * (1.0f - m_combLpCoef[c]) +
                                    m_combLpStateR[c] * m_combLpCoef[c];

                cl.buf[cl.writeIdx] = pdL + m_combFeedback[c] * m_combLpStateL[c];
                cr.buf[cr.writeIdx] = pdR + m_combFeedback[c] * m_combLpStateR[c];
                if (++cl.writeIdx >= blen) cl.writeIdx = 0;
                if (++cr.writeIdx >= blen) cr.writeIdx = 0;

                wetL += yL;
                wetR += yR;
            }
            wetL *= 0.18f;  // sum normalize
            wetR *= 0.18f;

            // ── Series allpass diffusers ──────────────────────────
            for (int a = 0; a < kNumAllpass; ++a) {
                wetL = allpassTick(m_apL[a], wetL, m_apDelay[a], m_diffusionG);
                wetR = allpassTick(m_apR[a], wetR, m_apDelay[a], m_diffusionG);
            }

            // ── Mix dry + wet ──────────────────────────────────────
            float outL = inL * dry + wetL * wet;
            float outR = inR * dry + wetR * wet;

            pcm[f * channels + 0] = outL;
            if (channels > 1) pcm[f * channels + 1] = outR;
        }
    }

private:
    /* ── Internal state ──────────────────────────────────────────── */

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
        float v      = in + g * bufOut;
        float out    = -g * v + bufOut;

        d.buf[d.writeIdx] = v;
        if (++d.writeIdx >= blen) d.writeIdx = 0;
        return out;
    }

    /* ── Recompute derived coefficients from m_params ───────────── */

    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        // Pre-delay length in samples
        m_preDelayLen = static_cast<int>(m_params[ParamPreDelay] * 0.250f * fs);
        if (m_preDelayLen < 1) m_preDelayLen = 1;
        if (m_preDelayLen >= static_cast<int>(m_preDelayBufL.size()))
            m_preDelayLen = static_cast<int>(m_preDelayBufL.size()) - 1;

        // Comb delay tunings — different program selects a base set,
        // size scales them around 0.5..2.0, modulation jitters them.
        const int prog = std::clamp(static_cast<int>(m_params[ParamProgram] * 3.999f), 0, 3);
        const float sizeMult = 0.5f + m_params[ParamSize] * 1.5f;

        // Tunings in milliseconds — chosen so the four programs sound
        // distinct but stay coprime to avoid resonant ringing.
        static constexpr float kCombMs[4][kNumCombs] = {
            // Hall A
            { 29.7f, 33.5f, 41.1f, 47.6f, 53.7f, 59.4f },
            // Hall B
            { 33.1f, 39.7f, 45.3f, 53.1f, 61.5f, 71.2f },
            // Plate
            { 22.3f, 25.1f, 31.7f, 35.9f, 42.5f, 47.1f },
            // Room
            { 16.4f, 19.9f, 23.7f, 27.1f, 31.3f, 36.5f },
        };
        // R channel offsets (~3% so the stereo spread is wide but the
        // mid stays consistent — very 224-like)
        static constexpr float kCombStereoOffset = 1.032f;

        for (int c = 0; c < kNumCombs; ++c) {
            float msL = kCombMs[prog][c] * sizeMult;
            float msR = msL * kCombStereoOffset;
            int dL = static_cast<int>(msL * 0.001f * fs);
            int dR = static_cast<int>(msR * 0.001f * fs);
            int blen = static_cast<int>(m_combL[c].buf.size());
            if (dL < 8)        dL = 8;
            if (dR < 8)        dR = 8;
            if (dL >= blen)    dL = blen - 1;
            if (dR >= blen)    dR = blen - 1;
            m_combDelayL[c] = dL;
            m_combDelayR[c] = dR;

            // Comb feedback derived from RT60 target. The classic
            // Schroeder formula: g = exp(-3 ln(10) D / (RT60 fs))
            float rt60 = decayTime();
            // Bass / treble multipliers tilt the per-comb decay so the
            // 224's tonal character (warm bass, smooth top) emerges.
            const float bassMult = 0.5f + m_params[ParamBassMult] * 1.5f;
            const float trebMult = 0.2f + m_params[ParamTrebleDecay] * 0.8f;
            float bandTilt = 1.0f;
            if (c < 2)       bandTilt = bassMult;     // first 2 combs are bass
            else if (c >= 4) bandTilt = trebMult;     // last 2 are treble
            float thisRt = rt60 * bandTilt;
            if (thisRt < 0.05f) thisRt = 0.05f;

            float Dsec = static_cast<float>(dL) / fs;
            float fb   = std::pow(10.0f, -3.0f * Dsec / thisRt);
            if (fb < 0.0f) fb = 0.0f;
            if (fb > 0.995f) fb = 0.995f;
            m_combFeedback[c] = fb;

            // HF damping → low-pass coefficient on the feedback signal
            // (one-pole, normalized so 0 = bypass, 1 = aggressive)
            float damp = m_params[ParamHfDamping];
            m_combLpCoef[c] = 0.10f + damp * 0.85f;
        }

        // Allpass delays (in samples) — fixed by program but scaled
        // gently with size so the diffusion follows the room.
        static constexpr float kApMs[kNumAllpass] = { 5.0f, 1.7f, 12.7f, 9.3f };
        for (int a = 0; a < kNumAllpass; ++a) {
            float ms = kApMs[a] * (0.85f + sizeMult * 0.15f);
            int d = static_cast<int>(ms * 0.001f * fs);
            int blen = static_cast<int>(m_apL[a].buf.size());
            if (d < 4)     d = 4;
            if (d >= blen) d = blen - 1;
            m_apDelay[a] = d;
        }

        // Diffusion → allpass coefficient (0.40..0.78 range)
        m_diffusionG = 0.40f + m_params[ParamDiffusion] * 0.38f;

        // LF cut high-pass coefficient (one-pole)
        float lfHz = 20.0f + m_params[ParamLfCut] * 380.0f;
        m_lfHighpassCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * lfHz / fs);

        // Mod depth in samples (0..3 ms)
        m_modDepthSamples = m_params[ParamModDepth] * 3.0f * 0.001f * fs;

        // LFO frequency: very slow (~0.45 Hz) — the 224's signature
        // shimmer rate
        m_modPhaseInc = 2.0 * 3.14159265359 * 0.45 / static_cast<double>(fs);
    }

    float decayTime() const
    {
        // ParamDecay 0..1 → 0.4..70 s, exponential mapping for usable
        // resolution at the short end.
        float v = m_params[ParamDecay];
        return 0.4f * std::pow(70.0f / 0.4f, v);
    }

    /* ── Member state ─────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    // Pre-delay
    std::vector<float> m_preDelayBufL;
    std::vector<float> m_preDelayBufR;
    int                m_preWriteIdx = 0;
    int                m_preDelayLen = 1;

    // Combs
    CombLine m_combL[kNumCombs];
    CombLine m_combR[kNumCombs];
    int      m_combDelayL[kNumCombs] = {};
    int      m_combDelayR[kNumCombs] = {};
    float    m_combFeedback[kNumCombs] = {};
    float    m_combLpCoef[kNumCombs]   = {};
    float    m_combLpStateL[kNumCombs] = {};
    float    m_combLpStateR[kNumCombs] = {};

    // Allpass
    DelayLine m_apL[kNumAllpass];
    DelayLine m_apR[kNumAllpass];
    int       m_apDelay[kNumAllpass] = {};
    float     m_diffusionG = 0.6f;

    // DC blocker / LF cut state
    float m_dcL = 0.0f, m_dcR = 0.0f;
    float m_lfStateL = 0.0f, m_lfStateR = 0.0f;
    float m_lfHighpassCoef = 0.01f;

    // Modulation LFO
    double m_modPhase    = 0.0;
    double m_modPhaseInc = 0.0;
    float  m_modDepthSamples = 0.0f;
};

} // namespace mc1dsp
