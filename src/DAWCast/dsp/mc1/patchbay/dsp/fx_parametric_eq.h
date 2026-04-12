/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_parametric_eq.h — 10-Band Parametric EQ (RBJ biquad IIR)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Wraps mc1dsp::DspEq from Mcaster1DSPEncoder with the DspEffect interface.
 * 10 bands: 80Hz (low shelf) → 150,400,800,1500,3000,5000,8000,12000 Hz → 16kHz (high shelf)
 * ±24 dB gain per band, RBJ Audio EQ Cookbook biquad coefficients.
 */

#pragma once

#include "dsp_effect.h"

#include <array>
#include <cmath>
#include <cstring>

namespace mc1dsp {

/* ── Biquad filter (direct form I, 2-channel state) ──────────────── */
struct PEQBiquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float              a1 = 0.0f, a2 = 0.0f;
    float x1[2] = {}, x2[2] = {};
    float y1[2] = {}, y2[2] = {};

    inline float tick(float x, int ch) {
        float y = b0*x + b1*x1[ch] + b2*x2[ch] - a1*y1[ch] - a2*y2[ch];
        x2[ch] = x1[ch]; x1[ch] = x;
        y2[ch] = y1[ch]; y1[ch] = y;
        return y;
    }
    void clear() {
        for (int c = 0; c < 2; c++) x1[c] = x2[c] = y1[c] = y2[c] = 0.0f;
    }
};

enum class PEQBandType { LowShelf, Peaking, HighShelf };

/* ── 10-Band Parametric EQ Plugin ────────────────────────────────── */

class FxParametricEq : public DspEffect {
public:
    static constexpr int NUM_BANDS = 10;
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    FxParametricEq() { initBands(); recomputeAll(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "10-Band Parametric EQ"; }
    const char* id()       const override { return "mc1.eq.parametric10"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::EQ; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;
        for (size_t f = 0; f < frames; ++f) {
            for (int ch = 0; ch < channels; ++ch) {
                float sample = pcm[f * channels + ch];
                for (int b = 0; b < NUM_BANDS; ++b) {
                    if (bandEnabled_[b])
                        sample = filters_[b].tick(sample, ch);
                }
                pcm[f * channels + ch] = sample;
            }
        }
    }

    void reset() override {
        for (auto& f : filters_) f.clear();
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        recomputeAll();
    }

    /* ── Parameters: 10 bands × gain (normalized 0–1 maps to -24..+24 dB) ── */

    int paramCount() const override { return NUM_BANDS; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "80 Hz", "150 Hz", "400 Hz", "800 Hz", "1.5 kHz",
            "3 kHz", "5 kHz", "8 kHz", "12 kHz", "16 kHz"
        };
        return (index >= 0 && index < NUM_BANDS) ? names[index] : "";
    }

    const char* paramUnit(int /*index*/) const override { return "dB"; }

    float paramValue(int index) const override {
        if (index < 0 || index >= NUM_BANDS) return 0.5f;
        return (bandGainDb_[index] + 24.0f) / 48.0f;  // -24..+24 → 0..1
    }

    void setParamValue(int index, float value) override {
        if (index < 0 || index >= NUM_BANDS) return;
        bandGainDb_[index] = value * 48.0f - 24.0f;  // 0..1 → -24..+24
        recompute(index);
    }

    std::string paramDisplayValue(int index) const override {
        if (index < 0 || index >= NUM_BANDS) return "";
        char buf[32];
        snprintf(buf, sizeof(buf), "%+.1f dB", bandGainDb_[index]);
        return buf;
    }

    /* ── Direct access for presets ────────────────────────────────── */

    void setBandGainDb(int index, float db) {
        if (index >= 0 && index < NUM_BANDS) {
            bandGainDb_[index] = db;
            recompute(index);
        }
    }

    float bandGainDb(int index) const {
        return (index >= 0 && index < NUM_BANDS) ? bandGainDb_[index] : 0.0f;
    }

private:
    static constexpr float kFreqs[NUM_BANDS] = {
        80, 150, 400, 800, 1500, 3000, 5000, 8000, 12000, 16000
    };
    static constexpr PEQBandType kTypes[NUM_BANDS] = {
        PEQBandType::LowShelf, PEQBandType::Peaking, PEQBandType::Peaking,
        PEQBandType::Peaking, PEQBandType::Peaking, PEQBandType::Peaking,
        PEQBandType::Peaking, PEQBandType::Peaking, PEQBandType::Peaking,
        PEQBandType::HighShelf
    };

    float bandGainDb_[NUM_BANDS] = {};
    bool  bandEnabled_[NUM_BANDS] = {};
    PEQBiquad filters_[NUM_BANDS];

    void initBands() {
        for (int i = 0; i < NUM_BANDS; ++i) {
            bandGainDb_[i] = 0.0f;
            bandEnabled_[i] = true;
        }
    }

    void recomputeAll() {
        for (int i = 0; i < NUM_BANDS; ++i) recompute(i);
    }

    void recompute(int index) {
        if (index < 0 || index >= NUM_BANDS) return;

        float freq = kFreqs[index];
        float gain = bandGainDb_[index];
        float Q = 1.0f;
        float sr = static_cast<float>(sampleRate_);
        auto type = kTypes[index];

        /* RBJ Audio EQ Cookbook coefficients */
        float A  = std::pow(10.0f, gain / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        float b0, b1, b2, a0, a1, a2;

        if (type == PEQBandType::Peaking) {
            b0 =  1.0f + alpha * A;
            b1 = -2.0f * cosw0;
            b2 =  1.0f - alpha * A;
            a0 =  1.0f + alpha / A;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha / A;
        } else if (type == PEQBandType::LowShelf) {
            float sqA = std::sqrt(A);
            b0 =        A * ((A+1) - (A-1)*cosw0 + 2*sqA*alpha);
            b1 =  2.0f *A * ((A-1) - (A+1)*cosw0);
            b2 =        A * ((A+1) - (A-1)*cosw0 - 2*sqA*alpha);
            a0 =              (A+1) + (A-1)*cosw0 + 2*sqA*alpha;
            a1 = -2.0f *     ((A-1) + (A+1)*cosw0);
            a2 =              (A+1) + (A-1)*cosw0 - 2*sqA*alpha;
        } else { /* HighShelf */
            float sqA = std::sqrt(A);
            b0 =        A * ((A+1) + (A-1)*cosw0 + 2*sqA*alpha);
            b1 = -2.0f *A * ((A-1) + (A+1)*cosw0);
            b2 =        A * ((A+1) + (A-1)*cosw0 - 2*sqA*alpha);
            a0 =              (A+1) - (A-1)*cosw0 + 2*sqA*alpha;
            a1 =  2.0f *     ((A-1) - (A+1)*cosw0);
            a2 =              (A+1) - (A-1)*cosw0 - 2*sqA*alpha;
        }

        /* Normalize */
        filters_[index].b0 = b0 / a0;
        filters_[index].b1 = b1 / a0;
        filters_[index].b2 = b2 / a0;
        filters_[index].a1 = a1 / a0;
        filters_[index].a2 = a2 / a0;
    }
};

} // namespace mc1dsp
