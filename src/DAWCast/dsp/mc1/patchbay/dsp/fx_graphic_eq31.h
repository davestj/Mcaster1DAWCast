/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_graphic_eq31.h — Stereo 31-Band Graphic EQ (ISO 1/3-Octave)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Full ISO 1/3-octave graphic EQ with stereo-linked RBJ biquad filters.
 * 31 bands: 20 Hz (low shelf) → 25..16000 Hz (peaking) → 20 kHz (high shelf)
 * ±12 dB gain per band, constant-Q (4.3) for tight 1/3-octave response.
 * Real-time safe with per-band clipping detection.
 */

#pragma once

#include "dsp_effect.h"

#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace mc1dsp {

/* ── 31-Band Graphic EQ Plugin ─────────────────────────────────────── */

class FxGraphicEq31 : public DspEffect {
public:
    static constexpr int NUM_BANDS = 31;
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    FxGraphicEq31() { initBands(); recomputeAll(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "31-Band Graphic EQ"; }
    const char* id()       const override { return "mc1.eq.graphic31"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::EQ; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        bool clipped = false;
        int  clippedBand = -1;

        for (size_t f = 0; f < frames; ++f) {
            for (int ch = 0; ch < channels; ++ch) {
                float sample = pcm[f * channels + ch];
                for (int b = 0; b < NUM_BANDS; ++b) {
                    sample = filters_[b].tick(sample, ch);

                    /* Per-band clipping detection */
                    if (std::fabs(sample) > 0.99f) {
                        clipped = true;
                        clippedBand = b;
                    }
                }
                pcm[f * channels + ch] = sample;
            }
        }

        if (clipped) {
            clipping_.store(true, std::memory_order_relaxed);
            clipBand_.store(clippedBand, std::memory_order_relaxed);
        }
    }

    void reset() override {
        for (auto& f : filters_) f.clear();
        clipping_.store(false, std::memory_order_relaxed);
        clipBand_.store(-1, std::memory_order_relaxed);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        recomputeAll();
    }

    /* ── Parameters: 31 bands × gain (normalized 0–1 maps to -12..+12 dB) ── */

    int paramCount() const override { return NUM_BANDS; }

    const char* paramName(int index) const override {
        static const char* names[NUM_BANDS] = {
            "20 Hz",   "25 Hz",   "31.5 Hz", "40 Hz",   "50 Hz",
            "63 Hz",   "80 Hz",   "100 Hz",  "125 Hz",  "160 Hz",
            "200 Hz",  "250 Hz",  "315 Hz",  "400 Hz",  "500 Hz",
            "630 Hz",  "800 Hz",  "1 kHz",   "1.25 kHz","1.6 kHz",
            "2 kHz",   "2.5 kHz", "3.15 kHz","4 kHz",   "5 kHz",
            "6.3 kHz", "8 kHz",   "10 kHz",  "12.5 kHz","16 kHz",
            "20 kHz"
        };
        return (index >= 0 && index < NUM_BANDS) ? names[index] : "";
    }

    const char* paramUnit(int /*index*/) const override { return "dB"; }

    float paramValue(int index) const override {
        if (index < 0 || index >= NUM_BANDS) return 0.5f;
        return (bandGainDb_[index] + 12.0f) / 24.0f;  // -12..+12 → 0..1
    }

    void setParamValue(int index, float value) override {
        if (index < 0 || index >= NUM_BANDS) return;
        if (value < 0.0f) value = 0.0f; else if (value > 1.0f) value = 1.0f;
        bandGainDb_[index] = value * 24.0f - 12.0f;  // 0..1 → -12..+12
        recompute(index);
    }

    std::string paramDisplayValue(int index) const override {
        if (index < 0 || index >= NUM_BANDS) return "";
        char buf[32];
        snprintf(buf, sizeof(buf), "%+.1f dB", bandGainDb_[index]);
        return buf;
    }

    /* ── Direct access for presets ────────────────────────────────── */

    void setBandGainDb(int band, float db) {
        if (band >= 0 && band < NUM_BANDS) {
            if (db < -12.0f) db = -12.0f; else if (db > 12.0f) db = 12.0f;
            bandGainDb_[band] = db;
            recompute(band);
        }
    }

    float bandGainDb(int band) const {
        return (band >= 0 && band < NUM_BANDS) ? bandGainDb_[band] : 0.0f;
    }

private:
    /* ── Biquad filter (direct form I, 2-channel state) ────────── */

    struct BQ {
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

    /* ── Band type enumeration ─────────────────────────────────── */

    enum class BandType { LowShelf, Peaking, HighShelf };

    /* ── ISO 1/3-octave center frequencies (Hz) ────────────────── */

    static constexpr float kFreqs[NUM_BANDS] = {
           20.0f,    25.0f,    31.5f,    40.0f,    50.0f,
           63.0f,    80.0f,   100.0f,   125.0f,   160.0f,
          200.0f,   250.0f,   315.0f,   400.0f,   500.0f,
          630.0f,   800.0f,  1000.0f,  1250.0f,  1600.0f,
         2000.0f,  2500.0f,  3150.0f,  4000.0f,  5000.0f,
         6300.0f,  8000.0f, 10000.0f, 12500.0f, 16000.0f,
        20000.0f
    };

    /* ── Band types: shelf on endpoints, peaking for 1–29 ──────── */

    static constexpr BandType kTypes[NUM_BANDS] = {
        BandType::LowShelf,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking, BandType::Peaking, BandType::Peaking, BandType::Peaking,
        BandType::Peaking,
        BandType::HighShelf
    };

    /* ── Constant Q for 1/3-octave bandwidth ───────────────────── */

    static constexpr float kQ = 4.3f;

    /* ── Per-band state ────────────────────────────────────────── */

    float bandGainDb_[NUM_BANDS] = {};
    BQ    filters_[NUM_BANDS];

    /* ── Initialization ────────────────────────────────────────── */

    void initBands() {
        for (int i = 0; i < NUM_BANDS; ++i)
            bandGainDb_[i] = 0.0f;
    }

    void recomputeAll() {
        for (int i = 0; i < NUM_BANDS; ++i) recompute(i);
    }

    /* ── RBJ Audio EQ Cookbook coefficient computation ──────────── */

    void recompute(int index) {
        if (index < 0 || index >= NUM_BANDS) return;

        float freq = kFreqs[index];
        float gain = bandGainDb_[index];
        float sr   = static_cast<float>(sampleRate_);
        auto  type = kTypes[index];

        /* RBJ Audio EQ Cookbook coefficients */
        float A     = std::pow(10.0f, gain / 40.0f);
        float w0    = 2.0f * 3.14159265f * freq / sr;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * kQ);

        float b0, b1, b2, a0, a1, a2;

        if (type == BandType::Peaking) {
            b0 =  1.0f + alpha * A;
            b1 = -2.0f * cosw0;
            b2 =  1.0f - alpha * A;
            a0 =  1.0f + alpha / A;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha / A;
        } else if (type == BandType::LowShelf) {
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

        /* Normalize by a0 */
        filters_[index].b0 = b0 / a0;
        filters_[index].b1 = b1 / a0;
        filters_[index].b2 = b2 / a0;
        filters_[index].a1 = a1 / a0;
        filters_[index].a2 = a2 / a0;
    }
};

} // namespace mc1dsp
