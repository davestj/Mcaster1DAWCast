/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_xenyx_preamp.h — Mackie Xenyx Mic Preamp (Channel Strip)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Signal chain: Input Gain → HPF → One-Knob Compressor → 3-Band EQ → Output Level
 * Inspired by the Mackie Xenyx series console channel strip.
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxXenyxPreamp : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /* Parameter indices */
    enum Param {
        P_INPUT_GAIN = 0,   // 0 to +60 dB
        P_HPF_ENABLE,       // 0 or 1
        P_HPF_FREQ,         // 75 to 300 Hz
        P_COMP_AMOUNT,      // 0 to 100%
        P_EQ_LOW,           // -15 to +15 dB @ 80 Hz shelf
        P_EQ_MID,           // -15 to +15 dB (sweepable peaking)
        P_EQ_MID_FREQ,      // 100 to 8000 Hz
        P_EQ_HIGH,          // -15 to +15 dB @ 12 kHz shelf
        P_OUTPUT_LEVEL,     // -60 to +10 dB
        P_COUNT
    };

    FxXenyxPreamp() { updateCoeffs(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "Mackie Xenyx Preamp"; }
    const char* id()       const override { return "mc1.channel.xenyx"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        float inputGainLin  = std::pow(10.0f, inputGainDb_ / 20.0f);
        float outputLevelLin = std::pow(10.0f, outputLevelDb_ / 20.0f);

        /* Derive one-knob compressor parameters from amount (0-100%) */
        float amount = compAmount_ / 100.0f;
        float compThreshDb = 0.0f - amount * 30.0f;          /* 0 dBFS at 0%, -30 at 100% */
        float compRatio = 1.0f + amount * 7.0f;              /* 1:1 at 0%, 8:1 at 100% */
        /* Auto makeup gain: approximate gain lost from compression */
        float makeupDb = 0.0f;
        if (compRatio > 1.01f) {
            makeupDb = (-compThreshDb) * (1.0f - 1.0f / compRatio) * 0.5f;
        }
        float makeupLin = std::pow(10.0f, makeupDb / 20.0f);

        for (size_t f = 0; f < frames; ++f) {
            for (int ch = 0; ch < channels && ch < 2; ++ch) {
                float s = pcm[f * channels + ch];

                /* 1. Input Gain */
                s *= inputGainLin;

                /* Store input peak for metering */
                float inAbs = std::fabs(s);
                float inDb = (inAbs > 1e-10f) ? 20.0f * std::log10(inAbs) : -96.0f;
                meterInputPeak_.store(inDb, std::memory_order_relaxed);

                /* 2. High-Pass Filter (Butterworth 2nd-order) */
                if (hpfEnable_) {
                    s = bqTick(hpf_[ch], s, ch);
                }

                /* 3. One-Knob Compressor */
                if (compAmount_ > 0.01f) {
                    float peakDb = (std::fabs(s) > 1e-10f)
                        ? 20.0f * std::log10(std::fabs(s)) : -96.0f;
                    float grDb = 0.0f;
                    if (peakDb > compThreshDb) {
                        float over = peakDb - compThreshDb;
                        grDb = over * (1.0f - 1.0f / compRatio);
                    }
                    float target = std::pow(10.0f, -grDb / 20.0f);
                    float coeff = (target < compEnv_[ch]) ? compAttackCoeff_ : compReleaseCoeff_;
                    compEnv_[ch] += coeff * (target - compEnv_[ch]);
                    s *= compEnv_[ch] * makeupLin;
                    meterGainReduction_.store(grDb, std::memory_order_relaxed);
                }

                /* 4. 3-Band EQ */
                s = bqTick(eqLow_[ch], s, ch);     /* 80 Hz low shelf */
                s = bqTick(eqMid_[ch], s, ch);      /* Sweepable mid peaking */
                s = bqTick(eqHigh_[ch], s, ch);     /* 12 kHz high shelf */

                /* 5. Output Level */
                s *= outputLevelLin;
                pcm[f * channels + ch] = s;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < 2; ++c) {
            compEnv_[c] = 1.0f;
            bqClear(hpf_[c]);
            bqClear(eqLow_[c]); bqClear(eqMid_[c]); bqClear(eqHigh_[c]);
        }
        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        updateCoeffs();
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Input Gain", "HPF Enable", "HPF Frequency", "Comp Amount",
            "EQ Low", "EQ Mid", "EQ Mid Freq", "EQ High", "Output Level"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dB", "", "Hz", "%", "dB", "dB", "Hz", "dB", "dB"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_INPUT_GAIN:   return inputGainDb_ / 60.0f;
            case P_HPF_ENABLE:   return hpfEnable_ ? 1.0f : 0.0f;
            case P_HPF_FREQ:     return (hpfFreq_ - 75.0f) / 225.0f;
            case P_COMP_AMOUNT:  return compAmount_ / 100.0f;
            case P_EQ_LOW:       return (eqLowDb_ + 15.0f) / 30.0f;
            case P_EQ_MID:       return (eqMidDb_ + 15.0f) / 30.0f;
            case P_EQ_MID_FREQ:  return (eqMidFreq_ - 100.0f) / 7900.0f;
            case P_EQ_HIGH:      return (eqHighDb_ + 15.0f) / 30.0f;
            case P_OUTPUT_LEVEL: return (outputLevelDb_ + 60.0f) / 70.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        switch (index) {
            case P_INPUT_GAIN:   inputGainDb_ = v * 60.0f; break;
            case P_HPF_ENABLE:   hpfEnable_ = (v >= 0.5f); break;
            case P_HPF_FREQ:     hpfFreq_ = v * 225.0f + 75.0f; updateCoeffs(); break;
            case P_COMP_AMOUNT:  compAmount_ = v * 100.0f; break;
            case P_EQ_LOW:       eqLowDb_ = v * 30.0f - 15.0f; updateCoeffs(); break;
            case P_EQ_MID:       eqMidDb_ = v * 30.0f - 15.0f; updateCoeffs(); break;
            case P_EQ_MID_FREQ:  eqMidFreq_ = v * 7900.0f + 100.0f; updateCoeffs(); break;
            case P_EQ_HIGH:      eqHighDb_ = v * 30.0f - 15.0f; updateCoeffs(); break;
            case P_OUTPUT_LEVEL: outputLevelDb_ = v * 70.0f - 60.0f; break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case P_INPUT_GAIN:   snprintf(buf, 32, "%+.1f dB", inputGainDb_); break;
            case P_HPF_ENABLE:   snprintf(buf, 32, "%s", hpfEnable_ ? "On" : "Off"); break;
            case P_HPF_FREQ:     snprintf(buf, 32, "%.0f Hz", hpfFreq_); break;
            case P_COMP_AMOUNT:  snprintf(buf, 32, "%.0f%%", compAmount_); break;
            case P_EQ_LOW:       snprintf(buf, 32, "%+.1f dB", eqLowDb_); break;
            case P_EQ_MID:       snprintf(buf, 32, "%+.1f dB", eqMidDb_); break;
            case P_EQ_MID_FREQ:  snprintf(buf, 32, "%.0f Hz", eqMidFreq_); break;
            case P_EQ_HIGH:      snprintf(buf, 32, "%+.1f dB", eqHighDb_); break;
            case P_OUTPUT_LEVEL: snprintf(buf, 32, "%+.1f dB", outputLevelDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Parameters */
    float inputGainDb_   =  20.0f;
    bool  hpfEnable_     = false;
    float hpfFreq_       = 100.0f;
    float compAmount_    =   0.0f;  /* 0-100% */
    float eqLowDb_       =   0.0f;
    float eqMidDb_       =   0.0f;
    float eqMidFreq_     = 2500.0f;
    float eqHighDb_      =   0.0f;
    float outputLevelDb_ =   0.0f;

    /* Compressor state (fixed attack=10ms, release=150ms) */
    float compEnv_[2] = {1.0f, 1.0f};
    float compAttackCoeff_  = 0.0f;
    float compReleaseCoeff_ = 0.0f;

    /* Biquad filters */
    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };

    BQ hpf_[2];
    BQ eqLow_[2], eqMid_[2], eqHigh_[2];

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch] - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y;
        return y;
    }
    static void bqClear(BQ& f) {
        for(int c=0;c<2;c++) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0;
    }

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        /* Fixed compressor time constants (Xenyx one-knob: 10ms attack, 150ms release) */
        compAttackCoeff_  = 1.0f - std::exp(-1.0f / (10.0f * 0.001f * sr));
        compReleaseCoeff_ = 1.0f - std::exp(-1.0f / (150.0f * 0.001f * sr));

        for (int c = 0; c < 2; ++c) {
            /* HPF: 2nd-order Butterworth high-pass (RBJ cookbook) */
            computeHP(hpf_[c], hpfFreq_, sr);

            /* EQ Low: 80 Hz low shelf */
            computeShelf(eqLow_[c], 80.0f, eqLowDb_, sr, true);

            /* EQ Mid: sweepable peaking, Q=1.5 */
            computePeaking(eqMid_[c], eqMidFreq_, eqMidDb_, 1.5f, sr);

            /* EQ High: 12 kHz high shelf */
            computeShelf(eqHigh_[c], 12000.0f, eqHighDb_, sr, false);
        }
    }

    /* ── RBJ Cookbook filter computations ─────────────────────────── */

    static void computeHP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q = sqrt(2)/2 Butterworth */
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 =  (1.0f + cosw0) / 2.0f / a0;
        f.b1 = -(1.0f + cosw0) / a0;
        f.b2 =  (1.0f + cosw0) / 2.0f / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    static void computeShelf(BQ& f, float freq, float gainDb, float sr, bool low) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / sr;
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

    static void computePeaking(BQ& f, float freq, float gainDb, float Q, float sr) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha / A;
        f.b0 = (1.0f + alpha * A) / a0;
        f.b1 = (-2.0f * cosw0) / a0;
        f.b2 = (1.0f - alpha * A) / a0;
        f.a1 = (-2.0f * cosw0) / a0;
        f.a2 = (1.0f - alpha / A) / a0;
    }
};

} // namespace mc1dsp
