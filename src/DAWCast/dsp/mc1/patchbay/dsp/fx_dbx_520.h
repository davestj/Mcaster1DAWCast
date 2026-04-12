/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_520.h — dbx 520 De-Esser (500 Series)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Split-band de-esser with tunable frequency, inspired by the dbx 520
 * 500-series module.  Unlike a full-band ducker, this design applies gain
 * reduction ONLY to the high-frequency band above the crossover point,
 * leaving the low band untouched — preserving body and warmth while
 * taming sibilance.
 *
 * Signal chain:
 *
 *   Input ──┬── Sidechain BPF (tunable freq + Q) ─── Envelope Follower
 *           │                                            ↓
 *           ├── LP Crossover ──────────────────────── (pass-through)
 *           │                                            ↓
 *           └── HP Crossover ── Gain Reduction ──────── Sum ── Mix ── Output
 *
 *   Listen mode: routes the sidechain BPF signal directly to the output
 *   so the user can audition what the detector is hearing.
 *
 * Header-only, C++17, real-time safe (no allocations in process()).
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx520 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* ── Parameter indices ──────────────────────────────────────── */

    enum ParamId {
        ParamFrequency = 0,    // 0..1 -> 1000..12000 Hz
        ParamRange,            // 0..1 -> 0..24 dB max reduction
        ParamThreshold,        // 0..1 -> -40..0 dB
        ParamWidth,            // 0..1 -> Q 0.5..8.0 (narrow to wide)
        ParamListen,           // 0..1 -> off/on (threshold 0.5)
        ParamMix,              // 0..1 -> 0..100%
        ParamOutput,           // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxDbx520() {
        setDefaults();
        computeCoeffs();
    }

    /* ── DspEffect interface ────────────────────────────────────── */

    const char* name()     const override { return "dbx 520 De-Esser"; }
    const char* id()       const override { return "mc1.dbx.520"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        const int useCh = (channels < MAX_CH) ? channels : MAX_CH;

        const float outputGainLin = std::pow(10.0f, outputDb_ / 20.0f);
        const float mix    = mix_;
        const float dryAmt = 1.0f - mix;
        const float rangeDb  = rangeDb_;
        const float threshDb = thresholdDb_;
        const bool  listen   = (listen_ >= 0.5f);

        for (size_t f = 0; f < frames; ++f) {

            /* ── Sum to mono for sidechain detection ───────────── */

            float mono = 0.0f;
            for (int ch = 0; ch < useCh; ++ch)
                mono += pcm[f * channels + ch];
            if (useCh > 1) mono *= 0.5f;

            /* ── Sidechain bandpass filter (mono) ──────────────── */

            float scOut = bqTick(scBpf_, mono, 0);

            /* ── Sidechain envelope follower ───────────────────── */

            float rectified = std::fabs(scOut);
            float coeff = (rectified > scEnv_) ? attackCoeff_ : releaseCoeff_;
            scEnv_ += coeff * (rectified - scEnv_);

            float envDb = (scEnv_ > 1e-10f)
                        ? 20.0f * std::log10(scEnv_) : -96.0f;

            /* Input metering (sidechain level) */
            meterInputPeak_.store(envDb, std::memory_order_relaxed);

            /* ── Gain reduction computation ─────────────────────── */

            float grDb = 0.0f;
            if (envDb > threshDb) {
                grDb = envDb - threshDb;
                /* Clamp to the Range parameter (max reduction) */
                if (grDb > rangeDb) grDb = rangeDb;
            }

            meterGainReduction_.store(grDb, std::memory_order_relaxed);

            float grLin = std::pow(10.0f, -grDb / 20.0f);

            /* ── Per-channel split-band processing ─────────────── */

            for (int ch = 0; ch < useCh; ++ch) {
                float dry = pcm[f * channels + ch];

                if (listen) {
                    /* Listen mode: output the sidechain BPF signal */
                    pcm[f * channels + ch] = scOut * outputGainLin;
                    continue;
                }

                /* Split into low and high bands at the crossover frequency */
                float lo = bqTick(xoverLp_[ch], dry, ch);
                float hi = bqTick(xoverHp_[ch], dry, ch);

                /* Apply gain reduction to the high band only */
                hi *= grLin;

                /* Recombine bands */
                float wet = (lo + hi) * outputGainLin;

                /* Dry/wet mix */
                pcm[f * channels + ch] = wet * mix + dry * dryAmt;
            }

            /* ── Output metering ───────────────────────────────── */

            float outPeak = 0.0f;
            for (int ch = 0; ch < useCh; ++ch) {
                float absOut = std::fabs(pcm[f * channels + ch]);
                if (absOut > outPeak) outPeak = absOut;
            }
            float outDb = (outPeak > 1e-10f)
                        ? 20.0f * std::log10(outPeak) : -96.0f;
            meterOutputPeak_.store(outDb, std::memory_order_relaxed);

            /* Clipping detection */
            if (outPeak > 1.0f)
                clipping_.store(true, std::memory_order_relaxed);
        }
    }

    void reset() override {
        scEnv_ = 0.0f;
        bqClear(scBpf_);
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(xoverLp_[c]);
            bqClear(xoverHp_[c]);
        }

        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
        clipping_.store(false);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeCoeffs();
    }

    /* ── Parameters ─────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Frequency", "Range", "Threshold",
            "Width", "Listen", "Mix", "Output"
        };
        return (index >= 0 && index < kParamCount) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "Hz", "dB", "dB",
            "", "", "%", "dB"
        };
        return (index >= 0 && index < kParamCount) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case ParamFrequency: return (frequencyHz_ - 1000.0f) / 11000.0f;
            case ParamRange:     return rangeDb_ / 24.0f;
            case ParamThreshold: return (thresholdDb_ + 40.0f) / 40.0f;
            case ParamWidth:     return (width_ - 0.5f) / 7.5f;
            case ParamListen:    return listen_;
            case ParamMix:       return mix_;
            case ParamOutput:    return (outputDb_ + 12.0f) / 18.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (index) {
            case ParamFrequency:
                frequencyHz_ = v * 11000.0f + 1000.0f;
                computeCoeffs();
                break;
            case ParamRange:
                rangeDb_ = v * 24.0f;
                break;
            case ParamThreshold:
                thresholdDb_ = v * 40.0f - 40.0f;
                break;
            case ParamWidth:
                width_ = v * 7.5f + 0.5f;
                computeCoeffs();
                break;
            case ParamListen:
                listen_ = (v >= 0.5f) ? 1.0f : 0.0f;
                break;
            case ParamMix:
                mix_ = v;
                break;
            case ParamOutput:
                outputDb_ = v * 18.0f - 12.0f;
                break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[48];
        switch (index) {
            case ParamFrequency:
                if (frequencyHz_ >= 1000.0f)
                    snprintf(buf, sizeof(buf), "%.2f kHz", frequencyHz_ / 1000.0f);
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", frequencyHz_);
                break;
            case ParamRange:
                snprintf(buf, sizeof(buf), "%.1f dB", rangeDb_);
                break;
            case ParamThreshold:
                snprintf(buf, sizeof(buf), "%+.1f dB", thresholdDb_);
                break;
            case ParamWidth:
                snprintf(buf, sizeof(buf), "Q %.1f", width_);
                break;
            case ParamListen:
                snprintf(buf, sizeof(buf), "%s",
                         (listen_ >= 0.5f) ? "On" : "Off");
                break;
            case ParamMix:
                snprintf(buf, sizeof(buf), "%.0f%%", mix_ * 100.0f);
                break;
            case ParamOutput:
                snprintf(buf, sizeof(buf), "%+.1f dB", outputDb_);
                break;
            default:
                buf[0] = 0;
        }
        return buf;
    }

private:
    /* ── Parameter storage ──────────────────────────────────────── */

    float frequencyHz_  = 5950.0f;   // 1000..12000 Hz
    float rangeDb_      =   12.0f;   // 0..24 dB max reduction
    float thresholdDb_  =  -16.0f;   // -40..0 dB
    float width_        =    2.75f;  // Q 0.5..8.0
    float listen_       =    0.0f;   // 0=off, 1=on
    float mix_          =    1.0f;   // 0..1
    float outputDb_     =    0.006f; // -12..+6 dB

    /* ── Sidechain envelope state ───────────────────────────────── */

    float scEnv_         = 0.0f;
    float attackCoeff_   = 0.0f;   // ~0.5 ms
    float releaseCoeff_  = 0.0f;   // ~20 ms

    /* ── Biquad struct (per-channel state) ──────────────────────── */

    struct BQ {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1[2] = {}, x2[2] = {}, y1[2] = {}, y2[2] = {};
    };

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch]
                - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch] = f.x1[ch]; f.x1[ch] = x;
        f.y2[ch] = f.y1[ch]; f.y1[ch] = y;
        return y;
    }

    static void bqClear(BQ& f) {
        for (int c = 0; c < 2; ++c)
            f.x1[c] = f.x2[c] = f.y1[c] = f.y2[c] = 0;
    }

    /* ── Filter instances ───────────────────────────────────────── */

    BQ scBpf_          = {};             // Sidechain bandpass
    BQ xoverLp_[MAX_CH] = {};            // Crossover lowpass (per channel)
    BQ xoverHp_[MAX_CH] = {};            // Crossover highpass (per channel)

    /* ── Pi constant ────────────────────────────────────────────── */

    static constexpr float kPi = 3.14159265358979323846f;

    /* ── Set defaults from normalized values ────────────────────── */

    void setDefaults() {
        /* Normalized defaults -> real values:
         * Frequency  0.45  -> ~5950 Hz
         * Range      0.5   -> 12 dB
         * Threshold  0.6   -> -16 dB
         * Width      0.3   -> Q ~2.75
         * Listen     0.0   -> off
         * Mix        1.0   -> 100%
         * Output     0.667 -> ~0 dB
         */
        frequencyHz_  = 0.45f * 11000.0f + 1000.0f;    // 5950 Hz
        rangeDb_      = 0.5f  * 24.0f;                  // 12 dB
        thresholdDb_  = 0.6f  * 40.0f - 40.0f;          // -16 dB
        width_        = 0.3f  * 7.5f + 0.5f;            // Q 2.75
        listen_       = 0.0f;                            // off
        mix_          = 1.0f;                            // 100%
        outputDb_     = 0.667f * 18.0f - 12.0f;         // +0.006 dB (~0)
    }

    /* ── Compute all filter coefficients ────────────────────────── */

    void computeCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        /* Clamp frequency to Nyquist margin */
        float f0 = frequencyHz_;
        float nyq = sr * 0.49f;
        if (f0 > nyq) f0 = nyq;
        if (f0 < 20.0f) f0 = 20.0f;

        /* ── Sidechain bandpass (RBJ cookbook BPF, constant 0 dB peak) ── */
        {
            float Q  = width_;
            float w0 = 2.0f * kPi * f0 / sr;
            float cosw = std::cos(w0);
            float sinw = std::sin(w0);
            float alpha = sinw / (2.0f * Q);

            float b0 =  alpha;
            float b1 =  0.0f;
            float b2 = -alpha;
            float a0 =  1.0f + alpha;
            float a1 = -2.0f * cosw;
            float a2 =  1.0f - alpha;

            scBpf_.b0 = b0 / a0;
            scBpf_.b1 = b1 / a0;
            scBpf_.b2 = b2 / a0;
            scBpf_.a1 = a1 / a0;
            scBpf_.a2 = a2 / a0;
        }

        /* ── Crossover lowpass (2nd-order Butterworth) ─────────────────── */
        for (int c = 0; c < MAX_CH; ++c)
            computeLP(xoverLp_[c], f0, sr);

        /* ── Crossover highpass (2nd-order Butterworth) ────────────────── */
        for (int c = 0; c < MAX_CH; ++c)
            computeHP(xoverHp_[c], f0, sr);

        /* ── Envelope follower time constants ──────────────────────────── */
        auto msCoeff = [&](float ms) -> float {
            return 1.0f - std::exp(-1.0f / (ms * 0.001f * sr));
        };
        attackCoeff_  = msCoeff(0.5f);    // 0.5 ms — fast attack
        releaseCoeff_ = msCoeff(20.0f);   // 20 ms — medium release
    }

    /* ── RBJ Cookbook: 2nd-order Butterworth lowpass ─────────────── */

    static void computeLP(BQ& f, float freq, float sr) {
        float w0    = 2.0f * kPi * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q = sqrt(2)/2 */
        float cosw0 = std::cos(w0);
        float a0    = 1.0f + alpha;

        f.b0 = (1.0f - cosw0) / 2.0f / a0;
        f.b1 = (1.0f - cosw0) / a0;
        f.b2 = (1.0f - cosw0) / 2.0f / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    /* ── RBJ Cookbook: 2nd-order Butterworth highpass ────────────── */

    static void computeHP(BQ& f, float freq, float sr) {
        float w0    = 2.0f * kPi * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q = sqrt(2)/2 */
        float cosw0 = std::cos(w0);
        float a0    = 1.0f + alpha;

        f.b0 =  (1.0f + cosw0) / 2.0f / a0;
        f.b1 = -(1.0f + cosw0) / a0;
        f.b2 =  (1.0f + cosw0) / 2.0f / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 =  (1.0f - alpha) / a0;
    }
};

} // namespace mc1dsp
