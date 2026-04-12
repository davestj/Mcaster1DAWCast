/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_bbe_mach3.h — BBE Mach 3 Bass
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Psychoacoustic bass enhancer that synthesizes harmonics above the
 * fundamental frequency so bass "feels" present even on small speakers:
 *   Input -> BPF at user frequency (40-200 Hz, Q=1.0)
 *         -> Full-wave rectify (generates 2f, 3f, 4f harmonics)
 *         -> HPF at Tightness-controlled cutoff (remove sub-bass DC)
 *         -> LPF at 300 Hz (remove high-frequency fizz)
 *         -> Scale by Drive
 *         -> Mix harmonics back with dry signal
 *         -> Low-shelf Bass Boost at user frequency (0..12 dB)
 *         -> Mix (dry/wet) -> Output gain
 */

#pragma once

#include "dsp_effect.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxBbeMach3 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;
    static constexpr float PI = 3.14159265358979323846f;

    enum ParamId {
        ParamFrequency = 0,   // 0..1 -> 40..200 Hz
        ParamBassBoost,        // 0..1 -> 0..12 dB
        ParamDrive,            // 0..1 -> 0-10 display
        ParamTightness,        // 0..1 -> 0-100%
        ParamMix,              // 0..1 -> 0-100%
        ParamOutput,           // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxBbeMach3() { computeFilters(); }

    const char* name()     const override { return "BBE Mach 3 Bass"; }
    const char* id()       const override { return "mc1.bbe.mach3bass"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;
        int ch = std::min(channels, MAX_CH);

        for (size_t f = 0; f < frames; ++f) {
            float dry[MAX_CH] = {};
            float wet[MAX_CH] = {};

            for (int c = 0; c < ch; ++c) {
                float in = pcm[f * channels + c];
                dry[c] = in;

                /* --- Step 1: Bandpass input around user frequency --- */
                float bp = bqTick(bpf_[c], in, c);

                /* --- Step 2: Full-wave rectify to generate harmonics --- */
                float rect = fabsf(bp);

                /* --- Step 3: HPF the rectified signal (Tightness control) --- */
                float hpOut = bqTick(harmonicHP1_[c], rect, c);
                hpOut = bqTick(harmonicHP2_[c], hpOut, c);

                /* --- Step 4: LPF at 300 Hz to remove fizz --- */
                float lpOut = bqTick(harmonicLP1_[c], hpOut, c);
                lpOut = bqTick(harmonicLP2_[c], lpOut, c);

                /* --- Step 5: Scale harmonic bus by Drive --- */
                float harmonics = lpOut * driveLin_;

                /* --- Step 6: Mix harmonics back with dry signal --- */
                float combined = in + harmonics;

                /* --- Step 7: Low-shelf Bass Boost at user frequency --- */
                float boosted = bqTick(loShelf_[c], combined, c);

                wet[c] = boosted;
            }

            /* --- Step 8: Mix (dry/wet) + Output gain --- */
            for (int c = 0; c < ch; ++c) {
                float out = dry[c] + mix_ * (wet[c] - dry[c]);
                pcm[f * channels + c] = out * outputLin_;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(bpf_[c]);
            bqClear(harmonicHP1_[c]); bqClear(harmonicHP2_[c]);
            bqClear(harmonicLP1_[c]); bqClear(harmonicLP2_[c]);
            bqClear(loShelf_[c]);
        }
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeFilters();
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Frequency", "Bass Boost", "Drive",
            "Tightness", "Mix", "Output"
        };
        return (i >= 0 && i < kParamCount) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case ParamFrequency: return "Hz";
            case ParamBassBoost: return "dB";
            case ParamTightness: case ParamMix: return "%";
            case ParamOutput: return "dB";
            default: return "";
        }
    }

    float paramValue(int i) const override {
        switch (i) {
            case ParamFrequency: return frequencyN_;
            case ParamBassBoost: return bassBoostN_;
            case ParamDrive:     return driveN_;
            case ParamTightness: return tightnessN_;
            case ParamMix:       return mix_;
            case ParamOutput:    return outputN_;
            default: return 0.0f;
        }
    }

    void setParamValue(int i, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (i) {
            case ParamFrequency:
                frequencyN_ = v;
                computeFilters();
                break;
            case ParamBassBoost:
                bassBoostN_ = v;
                computeFilters();
                break;
            case ParamDrive:
                driveN_ = v;
                driveLin_ = 0.5f + v * 2.5f;  /* 0..1 -> 0.5..3.0 */
                break;
            case ParamTightness:
                tightnessN_ = v;
                computeFilters();
                break;
            case ParamMix:
                mix_ = v;
                break;
            case ParamOutput:
                outputN_ = v;
                outputDb_ = v * 18.0f - 12.0f;  /* 0..1 -> -12..+6 dB */
                outputLin_ = std::pow(10.0f, outputDb_ / 20.0f);
                break;
        }
    }

    std::string paramDisplayValue(int i) const override {
        char buf[32];
        switch (i) {
            case ParamFrequency: snprintf(buf, 32, "%.0f Hz", frequencyHz()); break;
            case ParamBassBoost: snprintf(buf, 32, "%+.1f dB", bassBoostDb()); break;
            case ParamDrive:     snprintf(buf, 32, "%.1f", driveN_ * 10.0f); break;
            case ParamTightness: snprintf(buf, 32, "%.0f%%", tightnessN_ * 100.0f); break;
            case ParamMix:       snprintf(buf, 32, "%.0f%%", mix_ * 100.0f); break;
            case ParamOutput:    snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Normalized parameter storage */
    float frequencyN_ = 0.375f;    /* ~100 Hz */
    float bassBoostN_ = 0.5f;
    float driveN_     = 0.5f;
    float tightnessN_ = 0.4f;
    float mix_        = 0.8f;
    float outputN_    = 0.667f;

    /* Derived values */
    float driveLin_   = 1.75f;     /* 0.5 + 0.5*2.5 */
    float outputDb_   = 0.006f;    /* ~0 dB */
    float outputLin_  = 1.0f;

    /* ---- Frequency helpers ---- */
    float frequencyHz()  const { return 40.0f + frequencyN_ * 160.0f; }  /* 40..200 Hz */
    float bassBoostDb()  const { return bassBoostN_ * 12.0f; }           /* 0..12 dB */
    float tightnessHz()  const { return 40.0f + tightnessN_ * 120.0f; }  /* 0..1 -> 40..160 Hz */

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

    /* ---- Filter arrays per channel ---- */
    std::array<BQ, MAX_CH> bpf_{};           /* Bandpass at user frequency */
    std::array<BQ, MAX_CH> harmonicHP1_{};   /* Butterworth HP (Tightness) */
    std::array<BQ, MAX_CH> harmonicHP2_{};   /* Butterworth HP (Tightness) */
    std::array<BQ, MAX_CH> harmonicLP1_{};   /* Butterworth LP at 300 Hz */
    std::array<BQ, MAX_CH> harmonicLP2_{};   /* Butterworth LP at 300 Hz */
    std::array<BQ, MAX_CH> loShelf_{};       /* Low-shelf at user frequency */

    /* ---- Compute all filter coefficients ---- */
    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);
        float freq = frequencyHz();
        float boostDb = bassBoostDb();
        float hpCutoff = tightnessHz();

        for (int c = 0; c < MAX_CH; ++c) {
            /* Bandpass at user frequency, Q = 1.0 (constant 0 dB peak) */
            computeBPF(bpf_[c], freq, 1.0f, sr);

            /* Harmonic HP: 2nd-order Butterworth at Tightness frequency */
            computeHP(harmonicHP1_[c], hpCutoff, sr);
            computeHP(harmonicHP2_[c], hpCutoff, sr);

            /* Harmonic LP: 2nd-order Butterworth at 300 Hz */
            computeLP(harmonicLP1_[c], 300.0f, sr);
            computeLP(harmonicLP2_[c], 300.0f, sr);

            /* Low-shelf for Bass Boost at user frequency */
            computeShelf(loShelf_[c], freq, boostDb, sr);
        }
    }

    /* ---- RBJ Cookbook: BPF (constant 0 dB peak gain) ---- */
    static void computeBPF(BQ& f, float freq, float Q, float sr) {
        float w0 = 2.0f * PI * freq / sr;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 = alpha / a0;
        f.b1 = 0.0f;
        f.b2 = -alpha / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
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

    /* ---- RBJ Cookbook: Low-shelf ---- */
    static void computeShelf(BQ& f, float freq, float gainDb, float sr) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * PI * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.707f);
        float cosw0 = std::cos(w0);
        float sqA = std::sqrt(A);
        float a0 = (A+1)+(A-1)*cosw0+2*sqA*alpha;
        f.b0 = A*((A+1)-(A-1)*cosw0+2*sqA*alpha) / a0;
        f.b1 = 2*A*((A-1)-(A+1)*cosw0) / a0;
        f.b2 = A*((A+1)-(A-1)*cosw0-2*sqA*alpha) / a0;
        f.a1 = -2*((A-1)+(A+1)*cosw0) / a0;
        f.a2 = ((A+1)+(A-1)*cosw0-2*sqA*alpha) / a0;
    }
};

} // namespace mc1dsp
