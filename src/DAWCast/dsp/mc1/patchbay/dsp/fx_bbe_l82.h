/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_bbe_l82.h — BBE L82 Loudness Maximizer
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Multi-band lookahead limiter that increases loudness without audible pumping:
 *   Input -> LR4 3-band crossover split (200 Hz, 2500 Hz)
 *         -> Per-band lookahead peak limiter (5ms delay, envelope follower)
 *         -> Sensitivity macro drives all 3 band thresholds
 *         -> Recombine limited bands
 *         -> Brickwall ceiling limiter (hard clip)
 *         -> Mix (dry/wet) -> Output gain
 */

#pragma once

#include "dsp_effect.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxBbeL82 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;
    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float LOOKAHEAD_SEC = 0.005f;  /* 5 ms */

    enum ParamId {
        ParamSensitivity = 0,  // 0..1 -> 0-10 display (macro threshold)
        ParamLoThresh,          // 0..1 -> -24..0 dB (low band threshold offset)
        ParamMidThresh,         // 0..1 -> -24..0 dB
        ParamHiThresh,          // 0..1 -> -24..0 dB
        ParamRelease,           // 0..1 -> 50..500 ms
        ParamCeiling,           // 0..1 -> -6..0 dBFS
        ParamMix,               // 0..1 -> 0-100%
        ParamOutput,            // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxBbeL82() {
        initBuffers(48000);
        computeFilters();
        updateThresholds();
        updateRelease();
    }

    const char* name()     const override { return "BBE L82 Loudness Maximizer"; }
    const char* id()       const override { return "mc1.bbe.l82"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;
        int ch = std::min(channels, MAX_CH);

        float maxGr = 0.0f;  /* track max GR for metering */

        for (size_t f = 0; f < frames; ++f) {
            float dry[MAX_CH] = {};
            float wet[MAX_CH] = {};

            for (int c = 0; c < ch; ++c) {
                float in = pcm[f * channels + c];
                dry[c] = in;

                /* --- LR4 3-band crossover split --- */
                /* Low: two cascaded Butterworth LP at 200 Hz */
                float low = bqTick(loLP1_[c], in, c);
                low = bqTick(loLP2_[c], low, c);

                /* High: two cascaded Butterworth HP at 2500 Hz */
                float high = bqTick(hiHP1_[c], in, c);
                high = bqTick(hiHP2_[c], high, c);

                /* Mid: complementary (input minus low and high) */
                float mid = in - low - high;

                /* --- Per-band lookahead peak limiting --- */
                low  = limitBand(delayLo_[c], envLo_[c], low, threshLoLin_, c);
                mid  = limitBand(delayMid_[c], envMid_[c], mid, threshMidLin_, c);
                high = limitBand(delayHi_[c], envHi_[c], high, threshHiLin_, c);

                /* --- Recombine --- */
                float sum = low + mid + high;

                /* --- Brickwall ceiling limiter (hard clip) --- */
                sum = std::max(-ceilingLin_, std::min(ceilingLin_, sum));

                wet[c] = sum;
            }

            /* --- Mix (dry/wet) + Output gain --- */
            for (int c = 0; c < ch; ++c) {
                float out = dry[c] + mix_ * (wet[c] - dry[c]);
                pcm[f * channels + c] = out * outputLin_;
            }
        }

        /* Store per-band GR for potential meter readout */
        m_grLo  = computeBandGr(envLo_,  threshLoLin_,  ch);
        m_grMid = computeBandGr(envMid_, threshMidLin_, ch);
        m_grHi  = computeBandGr(envHi_,  threshHiLin_,  ch);

        maxGr = std::max(m_grLo, std::max(m_grMid, m_grHi));
        meterGainReduction_.store(maxGr, std::memory_order_relaxed);
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(loLP1_[c]); bqClear(loLP2_[c]);
            bqClear(hiHP1_[c]); bqClear(hiHP2_[c]);
            delayClear(delayLo_[c]);
            delayClear(delayMid_[c]);
            delayClear(delayHi_[c]);
            envLo_[c]  = 0.0f;
            envMid_[c] = 0.0f;
            envHi_[c]  = 0.0f;
        }
        m_grLo = m_grMid = m_grHi = 0.0f;
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        initBuffers(sr);
        computeFilters();
        updateRelease();
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Sensitivity", "Lo Thresh", "Mid Thresh", "Hi Thresh",
            "Release", "Ceiling", "Mix", "Output"
        };
        return (i >= 0 && i < kParamCount) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case ParamLoThresh: case ParamMidThresh: case ParamHiThresh:
                return "dB";
            case ParamRelease: return "ms";
            case ParamCeiling: return "dBFS";
            case ParamMix:     return "%";
            case ParamOutput:  return "dB";
            default: return "";
        }
    }

    float paramValue(int i) const override {
        switch (i) {
            case ParamSensitivity: return sensitivityN_;
            case ParamLoThresh:    return loThreshN_;
            case ParamMidThresh:   return midThreshN_;
            case ParamHiThresh:    return hiThreshN_;
            case ParamRelease:     return releaseN_;
            case ParamCeiling:     return ceilingN_;
            case ParamMix:         return mix_;
            case ParamOutput:      return outputN_;
            default: return 0.0f;
        }
    }

    void setParamValue(int i, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (i) {
            case ParamSensitivity:
                sensitivityN_ = v;
                updateThresholds();
                break;
            case ParamLoThresh:
                loThreshN_ = v;
                updateThresholds();
                break;
            case ParamMidThresh:
                midThreshN_ = v;
                updateThresholds();
                break;
            case ParamHiThresh:
                hiThreshN_ = v;
                updateThresholds();
                break;
            case ParamRelease:
                releaseN_ = v;
                updateRelease();
                break;
            case ParamCeiling:
                ceilingN_ = v;
                ceilingDb_  = v * 6.0f - 6.0f;           /* 0..1 -> -6..0 dBFS */
                ceilingLin_ = std::pow(10.0f, ceilingDb_ / 20.0f);
                break;
            case ParamMix:
                mix_ = v;
                break;
            case ParamOutput:
                outputN_   = v;
                outputDb_  = v * 18.0f - 12.0f;          /* 0..1 -> -12..+6 dB */
                outputLin_ = std::pow(10.0f, outputDb_ / 20.0f);
                break;
        }
    }

    std::string paramDisplayValue(int i) const override {
        char buf[32];
        switch (i) {
            case ParamSensitivity: snprintf(buf, 32, "%.1f", sensitivityN_ * 10.0f); break;
            case ParamLoThresh:    snprintf(buf, 32, "%+.1f dB", loThreshDb()); break;
            case ParamMidThresh:   snprintf(buf, 32, "%+.1f dB", midThreshDb()); break;
            case ParamHiThresh:    snprintf(buf, 32, "%+.1f dB", hiThreshDb()); break;
            case ParamRelease:     snprintf(buf, 32, "%.0f ms", releaseMs()); break;
            case ParamCeiling:     snprintf(buf, 32, "%.1f dBFS", ceilingDb_); break;
            case ParamMix:         snprintf(buf, 32, "%.0f%%", mix_ * 100.0f); break;
            case ParamOutput:      snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

    /* Per-band gain reduction readout (dB, positive = reduction) */
    float grLo()  const { return m_grLo; }
    float grMid() const { return m_grMid; }
    float grHi()  const { return m_grHi; }

private:
    /* ---- Normalized parameter storage ---- */
    float sensitivityN_ = 0.4f;
    float loThreshN_    = 0.5f;
    float midThreshN_   = 0.5f;
    float hiThreshN_    = 0.5f;
    float releaseN_     = 0.3f;
    float ceilingN_     = 0.95f;
    float mix_          = 1.0f;
    float outputN_      = 0.667f;

    /* ---- Derived values ---- */
    float threshLoLin_   = 1.0f;   /* linear threshold for low band */
    float threshMidLin_  = 1.0f;   /* linear threshold for mid band */
    float threshHiLin_   = 1.0f;   /* linear threshold for high band */
    float releaseCoeff_  = 0.9999f;
    float ceilingDb_     = -0.3f;
    float ceilingLin_    = 0.966f;  /* pow(10, -0.3/20) */
    float outputDb_      = 0.006f;  /* ~0 dB */
    float outputLin_     = 1.0f;

    /* ---- Per-band GR meters (dB, positive = reduction) ---- */
    float m_grLo  = 0.0f;
    float m_grMid = 0.0f;
    float m_grHi  = 0.0f;

    /* ---- Envelope follower state per channel ---- */
    float envLo_[MAX_CH]  = {};
    float envMid_[MAX_CH] = {};
    float envHi_[MAX_CH]  = {};

    /* ---- Biquad ---- */
    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch]
                - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y;
        return y;
    }
    static void bqClear(BQ& f) {
        for (int c = 0; c < 2; ++c) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0;
    }

    /* ---- Delay line (lookahead buffer) ---- */
    struct DL { std::vector<float> buf; size_t wp=0, delay=0; };

    static float delayTick(DL& d, float in) {
        d.buf[d.wp] = in;
        size_t rd = (d.wp + d.buf.size() - d.delay) % d.buf.size();
        float out = d.buf[rd];
        d.wp = (d.wp + 1) % d.buf.size();
        return out;
    }
    static void delayClear(DL& d) {
        std::fill(d.buf.begin(), d.buf.end(), 0.0f); d.wp = 0;
    }

    /* ---- Filter arrays per channel ---- */
    std::array<BQ, MAX_CH> loLP1_{}, loLP2_{};   /* LR4 low-pass at 200 Hz */
    std::array<BQ, MAX_CH> hiHP1_{}, hiHP2_{};   /* LR4 high-pass at 2500 Hz */

    /* ---- Lookahead delay lines: 3 bands x MAX_CH ---- */
    std::array<DL, MAX_CH> delayLo_{}, delayMid_{}, delayHi_{};

    /* ---- Per-band lookahead peak limiter ---- */
    float limitBand(DL& dl, float& env, float in, float threshLin, int /*c*/) {
        /* Envelope follower: instant attack, programmable release */
        float absIn = std::fabs(in);
        if (absIn > env) {
            env = absIn;                         /* instant attack */
        } else {
            env = std::max(absIn, env * releaseCoeff_);  /* release */
        }

        /* Gain reduction in dB */
        float grDb = 0.0f;
        if (env > 1e-12f && threshLin > 1e-12f) {
            float peakDb  = 20.0f * std::log10(env);
            float threshDb = 20.0f * std::log10(threshLin);
            grDb = std::max(0.0f, peakDb - threshDb);
        }

        /* Apply GR to delayed (lookahead) signal */
        float delayed = delayTick(dl, in);
        float grLin = std::pow(10.0f, -grDb / 20.0f);
        return delayed * grLin;
    }

    /* Compute current band GR in dB from envelope state (for metering) */
    float computeBandGr(const float* env, float threshLin, int ch) const {
        float maxGr = 0.0f;
        for (int c = 0; c < ch; ++c) {
            if (env[c] > 1e-12f && threshLin > 1e-12f) {
                float peakDb  = 20.0f * std::log10(env[c]);
                float threshDb = 20.0f * std::log10(threshLin);
                float gr = std::max(0.0f, peakDb - threshDb);
                if (gr > maxGr) maxGr = gr;
            }
        }
        return maxGr;
    }

    /* ---- Threshold helpers ---- */
    /* Sensitivity 0 = thresh at 0 dB, Sensitivity 1 = thresh at -24 dB */
    float macroThreshDb() const { return -sensitivityN_ * 24.0f; }

    /* Individual band thresh: macro + offset. Offset: 0..1 -> -24..0 dB */
    float loThreshDb()  const { return macroThreshDb() + (loThreshN_ * 24.0f - 24.0f); }
    float midThreshDb() const { return macroThreshDb() + (midThreshN_ * 24.0f - 24.0f); }
    float hiThreshDb()  const { return macroThreshDb() + (hiThreshN_ * 24.0f - 24.0f); }

    void updateThresholds() {
        threshLoLin_  = std::pow(10.0f, loThreshDb() / 20.0f);
        threshMidLin_ = std::pow(10.0f, midThreshDb() / 20.0f);
        threshHiLin_  = std::pow(10.0f, hiThreshDb() / 20.0f);
    }

    /* ---- Release helper ---- */
    float releaseMs() const { return 50.0f + releaseN_ * 450.0f; }  /* 50..500 ms */

    void updateRelease() {
        float relSec = releaseMs() / 1000.0f;
        float sr = static_cast<float>(sampleRate_);
        releaseCoeff_ = std::exp(-1.0f / (sr * relSec));
    }

    /* ---- Initialize lookahead buffers ---- */
    void initBuffers(int sr) {
        size_t delaySamples = static_cast<size_t>(sr * LOOKAHEAD_SEC) + 1;
        for (int c = 0; c < MAX_CH; ++c) {
            delayLo_[c].buf.assign(delaySamples, 0.0f);
            delayLo_[c].wp = 0;
            delayLo_[c].delay = delaySamples - 1;

            delayMid_[c].buf.assign(delaySamples, 0.0f);
            delayMid_[c].wp = 0;
            delayMid_[c].delay = delaySamples - 1;

            delayHi_[c].buf.assign(delaySamples, 0.0f);
            delayHi_[c].wp = 0;
            delayHi_[c].delay = delaySamples - 1;
        }
    }

    /* ---- Compute all filter coefficients ---- */
    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);

        for (int c = 0; c < MAX_CH; ++c) {
            /* LR4 crossover: 200 Hz and 2500 Hz */
            computeLP(loLP1_[c], 200.0f, sr);
            computeLP(loLP2_[c], 200.0f, sr);
            computeHP(hiHP1_[c], 2500.0f, sr);
            computeHP(hiHP2_[c], 2500.0f, sr);
        }
    }

    /* ---- RBJ Cookbook: 2nd-order Butterworth LP (Q = 0.7071) ---- */
    static void computeLP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * PI * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 = (1.0f - cosw0) * 0.5f / a0;
        f.b1 = (1.0f - cosw0) / a0;
        f.b2 = f.b0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    /* ---- RBJ Cookbook: 2nd-order Butterworth HP (Q = 0.7071) ---- */
    static void computeHP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * PI * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 = (1.0f + cosw0) * 0.5f / a0;
        f.b1 = -(1.0f + cosw0) / a0;
        f.b2 = f.b0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }
};

} // namespace mc1dsp
