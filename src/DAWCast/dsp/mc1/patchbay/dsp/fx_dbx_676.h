/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_676.h — dbx 676 Tube Mic Preamp Channel Strip
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * dbx's flagship tube channel strip.
 * Processing chain:
 *   Tube Preamp (gain + drive + 2nd-harmonic saturation)
 *     -> Variable HPF (2nd-order Butterworth, 40-200 Hz)
 *     -> 3-Band Semi-Parametric EQ (lo shelf / mid bell / hi shelf)
 *     -> OverEasy Compressor (RMS envelope, soft-knee)
 *     -> Output trim
 *
 * Header-only, C++17, real-time safe (no allocations in process()).
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx676 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    enum ParamId {
        ParamGain = 0,      // 0..1 -> 0-60 dB
        ParamDrive,         // 0..1 -> 0-10 (tube saturation)
        ParamHPF,           // 0..1 -> 40-200 Hz
        ParamLoGain,        // 0..1 -> -12..+12 dB (centered at 0.5)
        ParamMidFreq,       // 0..1 -> 200-5000 Hz
        ParamMidGain,       // 0..1 -> -12..+12 dB (centered at 0.5)
        ParamHiGain,        // 0..1 -> -12..+12 dB (centered at 0.5)
        ParamCompThresh,    // 0..1 -> -40..0 dB
        ParamCompRatio,     // 0..1 -> 1:1..10:1
        ParamTubeMix,       // 0..1 -> tube blend (0=clean, 1=full tube)
        ParamMix,           // 0..1 -> dry/wet
        ParamOutput,        // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxDbx676() {
        /* Set defaults */
        params_[ParamGain]       = 0.5f;    // 30 dB
        params_[ParamDrive]      = 0.3f;    // moderate tube
        params_[ParamHPF]        = 0.0f;    // 40 Hz (lowest)
        params_[ParamLoGain]     = 0.5f;    // flat
        params_[ParamMidFreq]    = 0.4f;    // ~1.1 kHz
        params_[ParamMidGain]    = 0.5f;    // flat
        params_[ParamHiGain]     = 0.5f;    // flat
        params_[ParamCompThresh] = 0.75f;   // -10 dB
        params_[ParamCompRatio]  = 0.22f;   // ~3:1
        params_[ParamTubeMix]    = 0.6f;    // 60% tube
        params_[ParamMix]        = 1.0f;    // full wet
        params_[ParamOutput]     = 0.667f;  // ~0 dB

        deriveAll();
    }

    const char* name()     const override { return "dbx 676 Tube Mic Preamp"; }
    const char* id()       const override { return "mc1.dbx.676"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    /* ---- Real-time audio processing -------------------------------- */

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;
        int ch = std::min(channels, MAX_CH);

        for (size_t f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float dry = pcm[f * channels + c];
                float s = dry;

                /* 1. Tube Preamp: gain + saturation */
                s *= inputGainLin_;
                float tubeOut = tubeStage(s);
                s = s + tubeMix_ * (tubeOut - s);   /* blend clean/tube */

                /* 2. Variable HPF */
                s = bqTick(hpf_[c], s, c);

                /* 3. 3-Band Semi-Parametric EQ */
                s = bqTick(loShelf_[c], s, c);
                s = bqTick(midBell_[c], s, c);
                s = bqTick(hiShelf_[c], s, c);

                /* 4. OverEasy Compressor */
                s = compressOverEasy(s, c);

                /* 5. Output trim */
                s *= outputGainLin_;

                /* Dry/wet mix */
                float out = dry + mix_ * (s - dry);
                pcm[f * channels + c] = out;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(hpf_[c]);
            bqClear(loShelf_[c]);
            bqClear(midBell_[c]);
            bqClear(hiShelf_[c]);
            rmsSum_[c] = 0.0f;
            rmsIdx_[c] = 0;
            for (size_t i = 0; i < kRmsBufMax; ++i) rmsBuf_[c][i] = 0.0f;
            compEnv_[c] = 1.0f;
        }
        meterGainReduction_.store(0.0f, std::memory_order_relaxed);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeFilters();
        computeCompressor();
    }

    /* ---- Parameters ------------------------------------------------ */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Input Gain", "Drive", "HPF Freq",
            "Lo Gain", "Mid Freq", "Mid Gain", "Hi Gain",
            "Comp Threshold", "Comp Ratio",
            "Tube Mix", "Mix", "Output"
        };
        return (i >= 0 && i < kParamCount) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case ParamGain:       return "dB";
            case ParamDrive:      return "";
            case ParamHPF:        return "Hz";
            case ParamLoGain:     return "dB";
            case ParamMidFreq:    return "Hz";
            case ParamMidGain:    return "dB";
            case ParamHiGain:     return "dB";
            case ParamCompThresh: return "dB";
            case ParamCompRatio:  return ":1";
            case ParamTubeMix:    return "%";
            case ParamMix:        return "%";
            case ParamOutput:     return "dB";
            default: return "";
        }
    }

    float paramValue(int i) const override {
        return (i >= 0 && i < kParamCount) ? params_[i] : 0.0f;
    }

    void setParamValue(int i, float v) override {
        if (i < 0 || i >= kParamCount) return;
        params_[i] = std::max(0.0f, std::min(1.0f, v));

        switch (i) {
            case ParamGain:
                inputGainLin_ = std::pow(10.0f, (params_[ParamGain] * 60.0f) / 20.0f);
                break;
            case ParamDrive:
                drive_ = params_[ParamDrive] * 10.0f;
                break;
            case ParamHPF:
                computeFilters();
                break;
            case ParamLoGain:
            case ParamMidFreq:
            case ParamMidGain:
            case ParamHiGain:
                computeFilters();
                break;
            case ParamCompThresh:
            case ParamCompRatio:
                computeCompressor();
                break;
            case ParamTubeMix:
                tubeMix_ = params_[ParamTubeMix];
                break;
            case ParamMix:
                mix_ = params_[ParamMix];
                break;
            case ParamOutput:
                outputGainLin_ = std::pow(10.0f, mapOutput() / 20.0f);
                break;
        }
    }

    std::string paramDisplayValue(int i) const override {
        char buf[48];
        switch (i) {
            case ParamGain:
                snprintf(buf, sizeof(buf), "%.1f dB", params_[ParamGain] * 60.0f);
                break;
            case ParamDrive:
                snprintf(buf, sizeof(buf), "%.1f", params_[ParamDrive] * 10.0f);
                break;
            case ParamHPF:
                snprintf(buf, sizeof(buf), "%.0f Hz", mapHpfFreq());
                break;
            case ParamLoGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", mapBipolar12(params_[ParamLoGain]));
                break;
            case ParamMidFreq:
                snprintf(buf, sizeof(buf), "%.0f Hz", mapMidFreq());
                break;
            case ParamMidGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", mapBipolar12(params_[ParamMidGain]));
                break;
            case ParamHiGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", mapBipolar12(params_[ParamHiGain]));
                break;
            case ParamCompThresh:
                snprintf(buf, sizeof(buf), "%.1f dB", mapCompThresh());
                break;
            case ParamCompRatio:
                snprintf(buf, sizeof(buf), "%.1f:1", mapCompRatio());
                break;
            case ParamTubeMix:
                snprintf(buf, sizeof(buf), "%.0f%%", params_[ParamTubeMix] * 100.0f);
                break;
            case ParamMix:
                snprintf(buf, sizeof(buf), "%.0f%%", params_[ParamMix] * 100.0f);
                break;
            case ParamOutput:
                snprintf(buf, sizeof(buf), "%+.1f dB", mapOutput());
                break;
            default:
                buf[0] = 0;
        }
        return buf;
    }

private:
    /* ---- Stored parameters (all normalized 0..1) ------------------- */

    float params_[kParamCount] = {};

    /* ---- Derived / cached values ----------------------------------- */

    float inputGainLin_ = 1.0f;
    float drive_        = 3.0f;
    float tubeMix_      = 0.6f;
    float mix_          = 1.0f;
    float outputGainLin_ = 1.0f;

    /* Compressor derived */
    float compThreshDb_  = -10.0f;
    float compRatio_     = 3.0f;
    float compAttCoeff_  = 0.0f;
    float compRelCoeff_  = 0.0f;

    /* ---- Parameter mapping helpers --------------------------------- */

    float mapHpfFreq()   const { return 40.0f + params_[ParamHPF] * 160.0f; }
    float mapMidFreq()   const { return 200.0f + params_[ParamMidFreq] * 4800.0f; }
    float mapCompThresh() const { return params_[ParamCompThresh] * 40.0f - 40.0f; }
    float mapCompRatio() const { return 1.0f + params_[ParamCompRatio] * 9.0f; }
    float mapOutput()    const { return params_[ParamOutput] * 18.0f - 12.0f; }

    static float mapBipolar12(float v) { return (v - 0.5f) * 24.0f; } /* 0..1 -> -12..+12 */

    /* ---- Derive all cached values from params ---------------------- */

    void deriveAll() {
        inputGainLin_ = std::pow(10.0f, (params_[ParamGain] * 60.0f) / 20.0f);
        drive_        = params_[ParamDrive] * 10.0f;
        tubeMix_      = params_[ParamTubeMix];
        mix_          = params_[ParamMix];
        outputGainLin_ = std::pow(10.0f, mapOutput() / 20.0f);
        computeFilters();
        computeCompressor();
    }

    /* ---- Tube saturation stage ------------------------------------- */

    float tubeStage(float x) const {
        float d = std::max(0.1f, std::min(10.0f, drive_));
        /* Asymmetric soft clipping with 2nd-harmonic emphasis:
         * y = tanh(x * drive) + 0.1 * tanh(2 * x * drive)
         * The second term adds even-harmonic warmth characteristic
         * of tube circuits (positive half-cycle clips differently). */
        float xd = x * d;
        return std::tanh(xd) + 0.1f * std::tanh(2.0f * xd);
    }

    /* ---- Biquad struct --------------------------------------------- */

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

    /* ---- Filter instances per channel ------------------------------ */

    BQ hpf_[MAX_CH]     = {};   /* Variable highpass */
    BQ loShelf_[MAX_CH]  = {};   /* Low shelf at 80 Hz */
    BQ midBell_[MAX_CH]  = {};   /* Mid peaking bell, sweepable */
    BQ hiShelf_[MAX_CH]  = {};   /* High shelf at 8 kHz */

    /* ---- RMS envelope for compressor ------------------------------- */

    static constexpr size_t kRmsBufMax = 512;  /* >= 10ms at 48kHz */

    float rmsBuf_[MAX_CH][kRmsBufMax] = {};
    float rmsSum_[MAX_CH]  = {};
    size_t rmsIdx_[MAX_CH] = {};
    size_t rmsLen_          = 480;     /* ~10ms at 48kHz */
    float compEnv_[MAX_CH] = { 1.0f, 1.0f };

    /* ---- OverEasy compressor --------------------------------------- */

    float compressOverEasy(float s, int c) {
        /* RMS envelope: running sum of squared samples */
        float sq = s * s;
        rmsSum_[c] -= rmsBuf_[c][rmsIdx_[c]];
        rmsBuf_[c][rmsIdx_[c]] = sq;
        rmsSum_[c] += sq;
        rmsIdx_[c] = (rmsIdx_[c] + 1) % rmsLen_;

        /* Protect against negative sum from float drift */
        float rmsVal = std::sqrt(std::max(0.0f, std::min(1e10f, rmsSum_[c] / static_cast<float>(rmsLen_))));
        float levelDb = (rmsVal > 1e-10f)
                        ? 20.0f * std::log10(rmsVal)
                        : -96.0f;

        /* Soft-knee (OverEasy) gain computation:
         * Hard-knee would be: if (level > thresh) reduce.
         * OverEasy smoothly transitions over a +/-6 dB knee window. */
        float grDb = 0.0f;
        float kneeWidth = 6.0f;  /* dB half-width of soft knee */
        float overDb = levelDb - compThreshDb_;

        if (overDb > kneeWidth) {
            /* Above knee: full ratio compression */
            grDb = overDb * (1.0f - 1.0f / compRatio_);
        } else if (overDb > -kneeWidth) {
            /* Inside knee: smooth quadratic interpolation */
            float t = (overDb + kneeWidth) / (2.0f * kneeWidth);
            grDb = t * t * overDb * (1.0f - 1.0f / compRatio_);
        }
        /* Below knee: no compression (grDb stays 0) */

        float targetLin = std::pow(10.0f, -grDb / 20.0f);

        /* Smooth gain envelope with attack/release */
        float coeff = (targetLin < compEnv_[c]) ? compAttCoeff_ : compRelCoeff_;
        compEnv_[c] += coeff * (targetLin - compEnv_[c]);

        /* Meter gain reduction (pick max reduction across channels) */
        float grMeter = -20.0f * std::log10(std::max(1e-10f, compEnv_[c]));
        float prev = meterGainReduction_.load(std::memory_order_relaxed);
        if (grMeter > prev)
            meterGainReduction_.store(grMeter, std::memory_order_relaxed);

        return s * compEnv_[c];
    }

    /* ---- Filter coefficient computation ---------------------------- */

    static constexpr float kPi = 3.14159265358979323846f;

    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);

        float hpfFreq = mapHpfFreq();
        float loGainDb = mapBipolar12(params_[ParamLoGain]);
        float midFreq  = mapMidFreq();
        float midGainDb = mapBipolar12(params_[ParamMidGain]);
        float hiGainDb = mapBipolar12(params_[ParamHiGain]);

        for (int c = 0; c < MAX_CH; ++c) {
            computeHP(hpf_[c], hpfFreq, sr);
            computeShelf(loShelf_[c], 80.0f, loGainDb, sr, true);
            computePeaking(midBell_[c], midFreq, midGainDb, 1.5f, sr);
            computeShelf(hiShelf_[c], 8000.0f, hiGainDb, sr, false);
        }
    }

    void computeCompressor() {
        float sr = static_cast<float>(sampleRate_);

        compThreshDb_ = mapCompThresh();
        compRatio_    = mapCompRatio();

        /* RMS window length: ~10ms */
        rmsLen_ = std::max(static_cast<size_t>(1),
                           std::min(static_cast<size_t>(kRmsBufMax),
                                    static_cast<size_t>(sr * 0.01f)));

        /* Fixed attack ~5ms, release ~100ms (reasonable broadcast defaults) */
        float attackMs  = 5.0f;
        float releaseMs = 100.0f;
        compAttCoeff_ = 1.0f - std::exp(-1.0f / (attackMs * 0.001f * sr));
        compRelCoeff_ = 1.0f - std::exp(-1.0f / (releaseMs * 0.001f * sr));
    }

    /* ---- RBJ Cookbook: highpass (2nd-order Butterworth) ------------- */

    static void computeHP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * kPi * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q = sqrt(2)/2 */
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 =  (1.0f + cosw0) / 2.0f / a0;
        f.b1 = -(1.0f + cosw0) / a0;
        f.b2 =  (1.0f + cosw0) / 2.0f / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    /* ---- RBJ Cookbook: low shelf / high shelf ----------------------- */

    static void computeShelf(BQ& f, float freq, float gainDb, float sr, bool low) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * kPi * freq / sr;
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

    /* ---- RBJ Cookbook: peaking EQ (bell) ---------------------------- */

    static void computePeaking(BQ& f, float freq, float gainDb, float Q, float sr) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * kPi * freq / sr;
        float sinw0 = std::sin(w0);
        float cosw0 = std::cos(w0);
        float alpha = sinw0 / (2.0f * Q);
        float a0 = 1.0f + alpha / A;
        f.b0 = (1.0f + alpha * A) / a0;
        f.b1 = -2.0f * cosw0 / a0;
        f.b2 = (1.0f - alpha * A) / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha / A) / a0;
    }
};

} // namespace mc1dsp
