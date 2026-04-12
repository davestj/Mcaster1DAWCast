/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_580.h — dbx 580 Mic Preamp (500 Series)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Clean solid-state mic preamp module (500 Series format).
 * Processing chain:
 *   Pad (-20 dB switchable)
 *     -> Gain (0-60 dB clean amplification + subtle transistor warmth)
 *     -> Phase Invert (polarity flip)
 *     -> HPF (2nd-order Butterworth, 40-400 Hz)
 *     -> Output trim (-12..+6 dB)
 *
 * The "transparent" preamp in the dbx family — minimal coloration,
 * just a hint of solid-state character via soft-clipping: x / (1 + |x|).
 *
 * Header-only, C++17, real-time safe (no allocations in process()).
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx580 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    enum ParamId {
        ParamGain = 0,     // 0..1 -> 0-60 dB
        ParamPad,          // 0..1 -> off/on (threshold 0.5)
        ParamPhase,        // 0..1 -> normal/inverted (threshold 0.5)
        ParamPhantom,      // 0..1 -> off/on (display only)
        ParamHPF,          // 0..1 -> 40-400 Hz (0 = off)
        ParamOutput,       // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxDbx580() {
        /* Set defaults */
        params_[ParamGain]    = 0.5f;    // 30 dB
        params_[ParamPad]     = 0.0f;    // off
        params_[ParamPhase]   = 0.0f;    // normal
        params_[ParamPhantom] = 0.0f;    // off
        params_[ParamHPF]     = 0.0f;    // off (lowest)
        params_[ParamOutput]  = 0.667f;  // ~0 dB

        deriveAll();
    }

    const char* name()     const override { return "dbx 580 Mic Preamp"; }
    const char* id()       const override { return "mc1.dbx.580"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    /* ---- Real-time audio processing -------------------------------- */

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;
        int ch = std::min(channels, MAX_CH);

        for (size_t f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float s = pcm[f * channels + c];

                /* 1. Pad: -20 dB attenuation */
                if (padOn_) s *= padLin_;

                /* 2. Gain + subtle transistor warmth */
                s *= inputGainLin_;
                s = transistorClip(s);

                /* 3. Phase invert */
                if (phaseInvert_) s = -s;

                /* 4. HPF (bypassed when param is 0) */
                if (hpfActive_)
                    s = bqTick(hpf_[c], s, c);

                /* 5. Output trim */
                s *= outputGainLin_;

                pcm[f * channels + c] = s;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c)
            bqClear(hpf_[c]);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeHPF();
    }

    /* ---- Parameters ------------------------------------------------ */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Input Gain", "Pad", "Phase",
            "Phantom", "HPF Freq", "Output"
        };
        return (i >= 0 && i < kParamCount) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case ParamGain:   return "dB";
            case ParamPad:    return "";
            case ParamPhase:  return "";
            case ParamPhantom:return "";
            case ParamHPF:    return "Hz";
            case ParamOutput: return "dB";
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
            case ParamPad:
                padOn_ = (params_[ParamPad] > 0.5f);
                break;
            case ParamPhase:
                phaseInvert_ = (params_[ParamPhase] > 0.5f);
                break;
            case ParamPhantom:
                /* Display only — no audio effect */
                break;
            case ParamHPF:
                hpfActive_ = (params_[ParamHPF] > 0.001f);
                computeHPF();
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
            case ParamPad:
                snprintf(buf, sizeof(buf), "%s", padOn_ ? "-20 dB" : "Off");
                break;
            case ParamPhase:
                snprintf(buf, sizeof(buf), "%s", phaseInvert_ ? "Inverted" : "Normal");
                break;
            case ParamPhantom:
                snprintf(buf, sizeof(buf), "%s", (params_[ParamPhantom] > 0.5f) ? "+48V" : "Off");
                break;
            case ParamHPF:
                if (!hpfActive_)
                    snprintf(buf, sizeof(buf), "Off");
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", mapHpfFreq());
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

    float inputGainLin_  = 1.0f;
    float outputGainLin_ = 1.0f;
    bool  padOn_         = false;
    bool  phaseInvert_   = false;
    bool  hpfActive_     = false;

    static constexpr float padLin_ = 0.1f;  /* -20 dB = 10^(-20/20) = 0.1 */

    /* ---- Parameter mapping helpers --------------------------------- */

    float mapHpfFreq() const { return 40.0f + params_[ParamHPF] * 360.0f; }
    float mapOutput()  const { return params_[ParamOutput] * 18.0f - 12.0f; }

    /* ---- Derive all cached values from params ---------------------- */

    void deriveAll() {
        inputGainLin_  = std::pow(10.0f, (params_[ParamGain] * 60.0f) / 20.0f);
        outputGainLin_ = std::pow(10.0f, mapOutput() / 20.0f);
        padOn_         = (params_[ParamPad] > 0.5f);
        phaseInvert_   = (params_[ParamPhase] > 0.5f);
        hpfActive_     = (params_[ParamHPF] > 0.001f);
        computeHPF();
    }

    /* ---- Transistor soft-clipping ---------------------------------- */

    static float transistorClip(float x) {
        /* Solid-state soft-clip: x / (1 + |x|)
         * Much gentler than tanh — produces odd harmonics only,
         * very subtle warmth at moderate levels.
         * Scale so unity-gain signals pass nearly unchanged. */
        float ax = std::abs(x);
        if (ax < 0.5f) return x;  /* Linear below threshold — truly clean */
        return x / (1.0f + ax);
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

    /* ---- Filter instance ------------------------------------------- */

    BQ hpf_[MAX_CH] = {};

    /* ---- HPF coefficient computation ------------------------------- */

    static constexpr float kPi = 3.14159265358979323846f;

    void computeHPF() {
        if (!hpfActive_) return;
        float sr   = static_cast<float>(sampleRate_);
        float freq = mapHpfFreq();
        for (int c = 0; c < MAX_CH; ++c)
            computeHP(hpf_[c], freq, sr);
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
};

} // namespace mc1dsp
