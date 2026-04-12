/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_bbe_h82.h — BBE H82 Harmonic Maximizer
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Harmonic exciter with subharmonic restoration:
 *   Input -> LR4 crossover split (150 Hz, 1.2 kHz)
 *         -> Low band: subharmonic restoration (rectify + BP 30-80 Hz)
 *         -> Mid band: gentle presence shelf at 800 Hz
 *         -> High band: polynomial waveshaper (even/odd harmonics)
 *         -> DC blocker on high band
 *         -> Recombine -> Mix (dry/wet) -> Output gain
 */

#pragma once

#include "dsp_effect.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxBbeH82 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;
    static constexpr float PI = 3.14159265358979323846f;

    enum ParamId {
        ParamLoContour = 0,  // 0..1 -> 0-10 display (low shelf boost)
        ParamProcess,        // 0..1 -> 0-10 display (harmonic generation drive)
        ParamHarmonics,      // 0..1 -> even(0) to odd(1) blend
        ParamLoRestore,      // 0..1 -> 0-100% (subharmonic level)
        ParamMix,            // 0..1 -> 0-100%
        ParamOutput,         // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxBbeH82() { computeFilters(); }

    const char* name()     const override { return "BBE H82 Harmonic Maximizer"; }
    const char* id()       const override { return "mc1.bbe.h82"; }
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

                /* --- LR4 3-band crossover split --- */
                /* Low: two cascaded Butterworth LP at 150 Hz */
                float low = bqTick(loLP1_[c], in, c);
                low = bqTick(loLP2_[c], low, c);

                /* High: two cascaded Butterworth HP at 1.2 kHz */
                float high = bqTick(hiHP1_[c], in, c);
                high = bqTick(hiHP2_[c], high, c);

                /* Mid: complementary (input minus low and high) */
                float mid = in - low - high;

                /* --- Lo Contour: low-shelf boost at 80 Hz --- */
                low = bqTick(loShelf_[c], low, c);

                /* --- Subharmonic restoration on low band --- */
                /* Full-wave rectify to generate octave-up harmonics */
                float rectified = fabsf(low);
                /* Bandpass 30-80 Hz to extract subharmonic content */
                float subLP = bqTick(subLP1_[c], rectified, c);
                subLP = bqTick(subLP2_[c], subLP, c);
                float sub = bqTick(subHP1_[c], subLP, c);
                sub = bqTick(subHP2_[c], sub, c);
                /* Mix subharmonic back into low band */
                low = low + sub * loRestoreAmt_;

                /* --- Mid band: gentle presence shelf at 800 Hz --- */
                mid = bqTick(midShelf_[c], mid, c);

                /* --- High band: polynomial waveshaper --- */
                float processAmt = processN_ * 0.5f;     /* scale Process to 0..0.5 */
                float hx = high * (1.0f + processAmt);   /* drive into shaper */

                /* Harmonics blend: 0 = even-dominant, 1 = odd-dominant */
                float evenK = (1.0f - harmonicsN_) * processAmt;  /* x^2, x^4 coefs */
                float oddK  = harmonicsN_ * processAmt;            /* x^3, x^5 coefs */

                /* y = x + evenK*(x^2 + x^4) + oddK*(x^3 + x^5) */
                float x2 = hx * hx;
                float x3 = x2 * hx;
                float x4 = x2 * x2;
                float x5 = x4 * hx;
                float shaped = hx + evenK * (x2 + x4) + oddK * (x3 + x5);

                /* Soft-clip to prevent runaway */
                shaped = std::tanh(shaped);

                /* DC blocker: 1-pole HP at 10 Hz */
                float dcIn = shaped;
                float dcOut = dcIn - dcX1_[c] + dcCoeff_ * dcY1_[c];
                dcX1_[c] = dcIn;
                dcY1_[c] = dcOut;
                high = dcOut;

                /* --- Recombine --- */
                wet[c] = low + mid + high;
            }

            /* --- Mix (dry/wet) + Output gain --- */
            for (int c = 0; c < ch; ++c) {
                float out = dry[c] + mix_ * (wet[c] - dry[c]);
                pcm[f * channels + c] = out * outputLin_;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(loLP1_[c]); bqClear(loLP2_[c]);
            bqClear(hiHP1_[c]); bqClear(hiHP2_[c]);
            bqClear(loShelf_[c]); bqClear(midShelf_[c]);
            bqClear(subLP1_[c]); bqClear(subLP2_[c]);
            bqClear(subHP1_[c]); bqClear(subHP2_[c]);
            dcX1_[c] = 0.0f; dcY1_[c] = 0.0f;
        }
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeFilters();
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Lo Contour", "Process", "Harmonics",
            "Lo Restore", "Mix", "Output"
        };
        return (i >= 0 && i < kParamCount) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case ParamLoRestore: case ParamMix: return "%";
            case ParamOutput: return "dB";
            default: return "";
        }
    }

    float paramValue(int i) const override {
        switch (i) {
            case ParamLoContour: return loContourN_;
            case ParamProcess:   return processN_;
            case ParamHarmonics: return harmonicsN_;
            case ParamLoRestore: return loRestoreN_;
            case ParamMix:       return mix_;
            case ParamOutput:    return outputN_;
            default: return 0.0f;
        }
    }

    void setParamValue(int i, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (i) {
            case ParamLoContour:
                loContourN_ = v;
                computeFilters();
                break;
            case ParamProcess:
                processN_ = v;
                break;
            case ParamHarmonics:
                harmonicsN_ = v;
                break;
            case ParamLoRestore:
                loRestoreN_ = v;
                loRestoreAmt_ = v;
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
            case ParamLoContour: snprintf(buf, 32, "%.1f", loContourN_ * 10.0f); break;
            case ParamProcess:   snprintf(buf, 32, "%.1f", processN_ * 10.0f); break;
            case ParamHarmonics: snprintf(buf, 32, "%.0f%% odd", harmonicsN_ * 100.0f); break;
            case ParamLoRestore: snprintf(buf, 32, "%.0f%%", loRestoreN_ * 100.0f); break;
            case ParamMix:       snprintf(buf, 32, "%.0f%%", mix_ * 100.0f); break;
            case ParamOutput:    snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Normalized parameter storage */
    float loContourN_  = 0.5f;
    float processN_    = 0.5f;
    float harmonicsN_  = 0.5f;
    float loRestoreN_  = 0.3f;
    float mix_         = 1.0f;
    float outputN_     = 0.667f;

    /* Derived values */
    float loRestoreAmt_ = 0.3f;
    float outputDb_     = 0.006f;  /* ~0 dB */
    float outputLin_    = 1.0f;

    /* DC blocker state per channel */
    float dcX1_[MAX_CH] = {};
    float dcY1_[MAX_CH] = {};
    float dcCoeff_      = 0.9986f;  /* 1 - (2*pi*10/48000) */

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
    std::array<BQ, MAX_CH> loLP1_{}, loLP2_{};     /* LR4 low-pass at 150 Hz */
    std::array<BQ, MAX_CH> hiHP1_{}, hiHP2_{};     /* LR4 high-pass at 1.2 kHz */
    std::array<BQ, MAX_CH> loShelf_{};              /* Low-shelf at 80 Hz */
    std::array<BQ, MAX_CH> midShelf_{};             /* Mid presence shelf at 800 Hz */
    std::array<BQ, MAX_CH> subLP1_{}, subLP2_{};   /* Subharmonic LP at 80 Hz (LR4) */
    std::array<BQ, MAX_CH> subHP1_{}, subHP2_{};   /* Subharmonic HP at 30 Hz (LR4) */

    /* ---- Compute all filter coefficients ---- */
    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);
        float loContourDb = loContourN_ * 12.0f;  /* 0..1 -> 0..12 dB */

        /* DC blocker coefficient: 1 - (2*pi*10/sr) */
        dcCoeff_ = 1.0f - (2.0f * PI * 10.0f / sr);

        for (int c = 0; c < MAX_CH; ++c) {
            /* LR4 crossover: 150 Hz and 1.2 kHz */
            computeLP(loLP1_[c], 150.0f, sr);
            computeLP(loLP2_[c], 150.0f, sr);
            computeHP(hiHP1_[c], 1200.0f, sr);
            computeHP(hiHP2_[c], 1200.0f, sr);

            /* Lo Contour shelf at 80 Hz */
            computeShelf(loShelf_[c], 80.0f, loContourDb, sr, true);

            /* Mid presence shelf at 800 Hz (+2 dB gentle) */
            computeShelf(midShelf_[c], 800.0f, 2.0f, sr, false);

            /* Subharmonic bandpass: LP at 80 Hz + HP at 30 Hz (both LR4) */
            computeLP(subLP1_[c], 80.0f, sr);
            computeLP(subLP2_[c], 80.0f, sr);
            computeHP(subHP1_[c], 30.0f, sr);
            computeHP(subHP2_[c], 30.0f, sr);
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

    /* ---- RBJ Cookbook: Low-shelf / High-shelf ---- */
    static void computeShelf(BQ& f, float freq, float gainDb, float sr, bool low) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * PI * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.707f);
        float cosw0 = std::cos(w0);
        float sqA = std::sqrt(A);
        float a0;
        if (low) {
            f.b0 = A*((A+1)-(A-1)*cosw0+2*sqA*alpha);
            f.b1 = 2*A*((A-1)-(A+1)*cosw0);
            f.b2 = A*((A+1)-(A-1)*cosw0-2*sqA*alpha);
            a0   = (A+1)+(A-1)*cosw0+2*sqA*alpha;
            f.a1 = -2*((A-1)+(A+1)*cosw0);
            f.a2 = (A+1)+(A-1)*cosw0-2*sqA*alpha;
        } else {
            f.b0 = A*((A+1)+(A-1)*cosw0+2*sqA*alpha);
            f.b1 = -2*A*((A-1)+(A+1)*cosw0);
            f.b2 = A*((A+1)+(A-1)*cosw0-2*sqA*alpha);
            a0   = (A+1)-(A-1)*cosw0+2*sqA*alpha;
            f.a1 = 2*((A-1)-(A+1)*cosw0);
            f.a2 = (A+1)-(A-1)*cosw0-2*sqA*alpha;
        }
        f.b0/=a0; f.b1/=a0; f.b2/=a0; f.a1/=a0; f.a2/=a0;
    }
};

} // namespace mc1dsp
