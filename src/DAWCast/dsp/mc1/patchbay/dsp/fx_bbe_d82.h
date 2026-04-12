/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_bbe_d82.h — BBE D82 Sonic Maximizer
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * 3-band phase-corrective processor (modern successor to BBE 882i):
 *   Input -> LR4 crossover split (Lo X-Over, Hi X-Over)
 *         -> Phase delay alignment (low ~2.5ms, mid ~0.5ms)
 *         -> Lo Contour (low-shelf boost at 80Hz)
 *         -> Process (high-shelf presence at 3kHz)
 *         -> Drive (soft saturation on high band)
 *         -> Stereo Width (M/S processing on high band)
 *         -> Recombine -> Mix (dry/wet) -> Output gain
 */

#pragma once

#include "dsp_effect.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxBbeD82 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;
    static constexpr float PI = 3.14159265358979323846f;

    enum ParamId {
        ParamLoContour = 0,  // 0..1 -> 0-10 display
        ParamProcess,        // 0..1 -> 0-10 display
        ParamDrive,          // 0..1 -> 0-10 display
        ParamStereoWidth,    // 0..1 -> 0-100% display
        ParamLoXOver,        // 0..1 -> 80-250 Hz
        ParamHiXOver,        // 0..1 -> 800-2500 Hz
        ParamMix,            // 0..1 -> 0-100%
        ParamOutput,         // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxBbeD82() { initDelays(48000); computeFilters(); }

    const char* name()     const override { return "BBE D82 Sonic Maximizer"; }
    const char* id()       const override { return "mc1.bbe.d82"; }
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
                /* Low: two cascaded Butterworth LP at Lo X-Over */
                float low = bqTick(loLP1_[c], in, c);
                low = bqTick(loLP2_[c], low, c);

                /* High: two cascaded Butterworth HP at Hi X-Over */
                float high = bqTick(hiHP1_[c], in, c);
                high = bqTick(hiHP2_[c], high, c);

                /* Mid: allpass-compensated input minus low and high */
                float apLo = bqTick(apLo_[c], in, c);
                float apHi = bqTick(apHi_[c], apLo, c);
                float mid = apHi - low - high;

                /* --- Phase alignment via delay lines --- */
                low = delayTick(delayLo_[c], low);
                mid = delayTick(delayMid_[c], mid);

                /* --- Lo Contour: low-shelf boost at 80 Hz --- */
                low = bqTick(loShelf_[c], low, c);

                /* --- Process: high-shelf presence at 3 kHz --- */
                high = bqTick(hiShelf_[c], high, c);

                /* --- Drive: soft saturation on high band --- */
                high *= driveGain_;
                high = std::tanh(high);

                wet[c] = low + mid + high;
            }

            /* --- Stereo Width: M/S on high band (stereo only) --- */
            if (ch == 2) {
                float midS = (wet[0] + wet[1]) * 0.5f;
                float side  = (wet[0] - wet[1]) * 0.5f;
                side *= widthMul_;
                wet[0] = midS + side;
                wet[1] = midS - side;
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
            bqClear(apLo_[c]);  bqClear(apHi_[c]);
            bqClear(loShelf_[c]); bqClear(hiShelf_[c]);
            delayClear(delayLo_[c]); delayClear(delayMid_[c]);
        }
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        initDelays(sr);
        computeFilters();
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Lo Contour", "Process", "Drive", "Stereo Width",
            "Lo X-Over", "Hi X-Over", "Mix", "Output"
        };
        return (i >= 0 && i < kParamCount) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case ParamLoXOver: case ParamHiXOver: return "Hz";
            case ParamStereoWidth: case ParamMix:  return "%";
            case ParamOutput: return "dB";
            default: return "";
        }
    }

    float paramValue(int i) const override {
        switch (i) {
            case ParamLoContour:   return loContourN_;
            case ParamProcess:     return processN_;
            case ParamDrive:       return driveN_;
            case ParamStereoWidth: return widthN_;
            case ParamLoXOver:     return loXOverN_;
            case ParamHiXOver:     return hiXOverN_;
            case ParamMix:         return mix_;
            case ParamOutput:      return outputN_;
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
                computeFilters();
                break;
            case ParamDrive:
                driveN_ = v;
                driveGain_ = 1.0f + v * 2.0f;  /* 0->1.0x, 1.0->3.0x */
                break;
            case ParamStereoWidth:
                widthN_ = v;
                widthMul_ = v * 2.0f;  /* 0=mono, 0.5=normal(1x), 1.0=wide(2x) */
                break;
            case ParamLoXOver:
                loXOverN_ = v;
                computeFilters();
                initDelays(sampleRate_);
                break;
            case ParamHiXOver:
                hiXOverN_ = v;
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
            case ParamLoContour:   snprintf(buf, 32, "%.1f", loContourN_ * 10.0f); break;
            case ParamProcess:     snprintf(buf, 32, "%.1f", processN_ * 10.0f); break;
            case ParamDrive:       snprintf(buf, 32, "%.1f", driveN_ * 10.0f); break;
            case ParamStereoWidth: snprintf(buf, 32, "%.0f%%", widthN_ * 100.0f); break;
            case ParamLoXOver:     snprintf(buf, 32, "%.0f Hz", loXOverHz()); break;
            case ParamHiXOver:     snprintf(buf, 32, "%.0f Hz", hiXOverHz()); break;
            case ParamMix:         snprintf(buf, 32, "%.0f%%", mix_ * 100.0f); break;
            case ParamOutput:      snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Normalized parameter storage */
    float loContourN_ = 0.5f;
    float processN_   = 0.5f;
    float driveN_     = 0.3f;
    float widthN_     = 0.5f;
    float loXOverN_   = 0.24f;   /* ~120 Hz */
    float hiXOverN_   = 0.41f;   /* ~1500 Hz */
    float mix_        = 1.0f;
    float outputN_    = 0.667f;

    /* Derived values */
    float driveGain_  = 1.6f;    /* 1.0 + 0.3*2.0 */
    float widthMul_   = 1.0f;    /* 0.5 * 2.0 */
    float outputDb_   = 0.006f;  /* ~0 dB */
    float outputLin_  = 1.0f;

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

    /* ---- Delay line ---- */
    struct DL { std::vector<float> buf; size_t wp=0, delay=0; };

    void initDelays(int sr) {
        size_t maxD = static_cast<size_t>(sr * 0.003) + 1;  /* 3ms max */
        for (int c = 0; c < MAX_CH; ++c) {
            delayLo_[c].buf.assign(maxD, 0.0f); delayLo_[c].wp = 0;
            delayLo_[c].delay = static_cast<size_t>(sr * 0.0025);  /* 2.5ms */
            delayMid_[c].buf.assign(maxD, 0.0f); delayMid_[c].wp = 0;
            delayMid_[c].delay = static_cast<size_t>(sr * 0.0005); /* 0.5ms */
        }
    }
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

    /* ---- Frequency helpers ---- */
    float loXOverHz() const { return 80.0f + loXOverN_ * 170.0f; }   /* 80-250 Hz */
    float hiXOverHz() const { return 800.0f + hiXOverN_ * 1700.0f; } /* 800-2500 Hz */

    /* ---- Filter arrays per channel ---- */
    std::array<BQ, MAX_CH> loLP1_{}, loLP2_{};   /* LR4 low-pass (cascaded) */
    std::array<BQ, MAX_CH> hiHP1_{}, hiHP2_{};   /* LR4 high-pass (cascaded) */
    std::array<BQ, MAX_CH> apLo_{}, apHi_{};     /* Allpass for mid extraction */
    std::array<BQ, MAX_CH> loShelf_{}, hiShelf_{};
    std::array<DL, MAX_CH> delayLo_{}, delayMid_{};

    /* ---- Compute all filter coefficients ---- */
    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);
        float loFreq = loXOverHz();
        float hiFreq = hiXOverHz();
        float loContourDb = loContourN_ * 12.0f;  /* 0..1 -> 0..12 dB */
        float processDb   = processN_ * 12.0f;    /* 0..1 -> 0..12 dB */

        for (int c = 0; c < MAX_CH; ++c) {
            computeLP(loLP1_[c], loFreq, sr);
            computeLP(loLP2_[c], loFreq, sr);
            computeHP(hiHP1_[c], hiFreq, sr);
            computeHP(hiHP2_[c], hiFreq, sr);
            computeAP(apLo_[c], loFreq, sr);
            computeAP(apHi_[c], hiFreq, sr);
            computeShelf(loShelf_[c], 80.0f, loContourDb, sr, true);
            computeShelf(hiShelf_[c], 3000.0f, processDb, sr, false);
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

    /* ---- RBJ Cookbook: 2nd-order Allpass (Q = 0.7071) ---- */
    static void computeAP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * PI * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 = (1.0f - alpha) / a0;
        f.b1 = -2.0f * cosw0 / a0;
        f.b2 = (1.0f + alpha) / a0;
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
