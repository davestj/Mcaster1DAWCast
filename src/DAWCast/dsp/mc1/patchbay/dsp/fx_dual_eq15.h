/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dual_eq15.h — Dual Channel 15-Band Graphic EQ (Independent L/R)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Two independent 15-band graphic EQs — one per channel (Left / Right).
 * Designed for broadcasters who mic on L and run music on R, letting
 * each channel be equalized independently.
 *
 * 15 ISO 1/3-octave bands: 25, 40, 63, 100, 160, 250, 400, 630,
 *   1000, 1600, 2500, 4000, 6300, 10000, 16000 Hz
 * Band 0 (25 Hz) = low shelf, Band 14 (16 kHz) = high shelf,
 * Bands 1-13 = peaking biquad (Q = 2.0).
 * ±12 dB gain per band. RBJ Audio EQ Cookbook coefficients.
 *
 * Parameters 0-14  = Left channel bands
 * Parameters 15-29 = Right channel bands
 */

#pragma once

#include "dsp_effect.h"

#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace mc1dsp {

class FxDualEq15 : public DspEffect {
public:
    static constexpr int NUM_BANDS   = 15;
    static constexpr int NUM_PARAMS  = NUM_BANDS * 2;   /* 30: L0-L14, R0-R14 */
    static constexpr float MAX_GAIN  = 12.0f;           /* ±12 dB */
    static constexpr float BAND_Q    = 2.0f;            /* graphic EQ bandwidth */
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    FxDualEq15() { initBands(); recomputeAll(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "Dual 15-Band EQ (L/R)"; }
    const char* id()       const override { return "mc1.eq.dual15"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::EQ; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        clipping_.store(false, std::memory_order_relaxed);
        clipBand_.store(-1, std::memory_order_relaxed);

        if (channels == 1) {
            /* Mono: process through left filter bank only */
            for (size_t f = 0; f < frames; ++f) {
                float sample = pcm[f];
                for (int b = 0; b < NUM_BANDS; ++b) {
                    sample = bqTick(filtersL_[b], sample);
                    if (std::fabs(sample) > 0.99f) {
                        clipping_.store(true, std::memory_order_relaxed);
                        clipBand_.store(b, std::memory_order_relaxed);
                    }
                }
                pcm[f] = sample;
            }
        } else {
            /* Stereo: index 0 = left, index 1 = right */
            for (size_t f = 0; f < frames; ++f) {
                /* Left channel */
                float sL = pcm[f * channels + 0];
                for (int b = 0; b < NUM_BANDS; ++b) {
                    sL = bqTick(filtersL_[b], sL);
                    if (std::fabs(sL) > 0.99f) {
                        clipping_.store(true, std::memory_order_relaxed);
                        clipBand_.store(b, std::memory_order_relaxed);
                    }
                }
                pcm[f * channels + 0] = sL;

                /* Right channel */
                float sR = pcm[f * channels + 1];
                for (int b = 0; b < NUM_BANDS; ++b) {
                    sR = bqTick(filtersR_[b], sR);
                    if (std::fabs(sR) > 0.99f) {
                        clipping_.store(true, std::memory_order_relaxed);
                        clipBand_.store(NUM_BANDS + b, std::memory_order_relaxed);
                    }
                }
                pcm[f * channels + 1] = sR;
            }
        }
    }

    void reset() override {
        for (int b = 0; b < NUM_BANDS; ++b) {
            bqClear(filtersL_[b]);
            bqClear(filtersR_[b]);
        }
        clipping_.store(false, std::memory_order_relaxed);
        clipBand_.store(-1, std::memory_order_relaxed);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        recomputeAll();
    }

    /* ── Parameters: 30 total (0-14 = L bands, 15-29 = R bands) ── */

    int paramCount() const override { return NUM_PARAMS; }

    const char* paramName(int index) const override {
        static const char* names[NUM_PARAMS] = {
            "L 25 Hz",  "L 40 Hz",  "L 63 Hz",  "L 100 Hz", "L 160 Hz",
            "L 250 Hz", "L 400 Hz", "L 630 Hz", "L 1 kHz",  "L 1.6 kHz",
            "L 2.5 kHz","L 4 kHz",  "L 6.3 kHz","L 10 kHz", "L 16 kHz",
            "R 25 Hz",  "R 40 Hz",  "R 63 Hz",  "R 100 Hz", "R 160 Hz",
            "R 250 Hz", "R 400 Hz", "R 630 Hz", "R 1 kHz",  "R 1.6 kHz",
            "R 2.5 kHz","R 4 kHz",  "R 6.3 kHz","R 10 kHz", "R 16 kHz"
        };
        return (index >= 0 && index < NUM_PARAMS) ? names[index] : "";
    }

    const char* paramUnit(int /*index*/) const override { return "dB"; }

    float paramValue(int index) const override {
        if (index < 0 || index >= NUM_PARAMS) return 0.5f;
        if (index < NUM_BANDS)
            return (gainDbL_[index] + MAX_GAIN) / (2.0f * MAX_GAIN);
        else
            return (gainDbR_[index - NUM_BANDS] + MAX_GAIN) / (2.0f * MAX_GAIN);
    }

    void setParamValue(int index, float value) override {
        if (index < 0 || index >= NUM_PARAMS) return;
        if (value < 0.0f) value = 0.0f; else if (value > 1.0f) value = 1.0f;
        float db = value * (2.0f * MAX_GAIN) - MAX_GAIN;

        if (index < NUM_BANDS) {
            gainDbL_[index] = db;
            recomputeBand(filtersL_[index], index, db);
        } else {
            int b = index - NUM_BANDS;
            gainDbR_[b] = db;
            recomputeBand(filtersR_[b], b, db);
        }
    }

    std::string paramDisplayValue(int index) const override {
        if (index < 0 || index >= NUM_PARAMS) return "";
        float db = (index < NUM_BANDS) ? gainDbL_[index]
                                       : gainDbR_[index - NUM_BANDS];
        char buf[32];
        snprintf(buf, sizeof(buf), "%+.1f dB", db);
        return buf;
    }

    /* ── Direct access for presets ───────────────────────────────── */

    void setBandGainDb(int channel, int band, float db) {
        if (band < 0 || band >= NUM_BANDS) return;
        if (db < -MAX_GAIN) db = -MAX_GAIN; else if (db > MAX_GAIN) db = MAX_GAIN;
        if (channel == 0) {
            gainDbL_[band] = db;
            recomputeBand(filtersL_[band], band, db);
        } else if (channel == 1) {
            gainDbR_[band] = db;
            recomputeBand(filtersR_[band], band, db);
        }
    }

    float bandGainDb(int channel, int band) const {
        if (band < 0 || band >= NUM_BANDS) return 0.0f;
        if (channel == 0) return gainDbL_[band];
        if (channel == 1) return gainDbR_[band];
        return 0.0f;
    }

private:
    /* ── ISO 1/3-octave center frequencies ──────────────────────── */
    static constexpr float kFreqs[NUM_BANDS] = {
        25, 40, 63, 100, 160, 250, 400, 630, 1000, 1600,
        2500, 4000, 6300, 10000, 16000
    };

    enum class BandType { LowShelf, Peaking, HighShelf };

    static constexpr BandType bandType(int b) {
        if (b == 0)              return BandType::LowShelf;
        if (b == NUM_BANDS - 1)  return BandType::HighShelf;
        return BandType::Peaking;
    }

    /* ── Per-channel gain arrays ────────────────────────────────── */
    float gainDbL_[NUM_BANDS] = {};
    float gainDbR_[NUM_BANDS] = {};

    /* ── Biquad filter (single-channel state) ───────────────────── */
    struct BQ {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float              a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    static float bqTick(BQ& f, float x) {
        float y = f.b0*x + f.b1*f.x1 + f.b2*f.x2 - f.a1*f.y1 - f.a2*f.y2;
        f.x2 = f.x1; f.x1 = x;
        f.y2 = f.y1; f.y1 = y;
        return y;
    }

    static void bqClear(BQ& f) {
        f.x1 = f.x2 = f.y1 = f.y2 = 0.0f;
    }

    /* ── Separate filter banks for L and R ──────────────────────── */
    BQ filtersL_[NUM_BANDS];
    BQ filtersR_[NUM_BANDS];

    /* ── Initialization ─────────────────────────────────────────── */

    void initBands() {
        for (int b = 0; b < NUM_BANDS; ++b) {
            gainDbL_[b] = 0.0f;
            gainDbR_[b] = 0.0f;
        }
    }

    void recomputeAll() {
        for (int b = 0; b < NUM_BANDS; ++b) {
            recomputeBand(filtersL_[b], b, gainDbL_[b]);
            recomputeBand(filtersR_[b], b, gainDbR_[b]);
        }
    }

    /* ── RBJ Audio EQ Cookbook coefficient computation ───────────── */

    void recomputeBand(BQ& filt, int bandIndex, float gainDb) {
        float freq = kFreqs[bandIndex];
        float sr   = static_cast<float>(sampleRate_);
        auto  type = bandType(bandIndex);

        float A     = std::pow(10.0f, gainDb / 40.0f);
        float w0    = 2.0f * 3.14159265f * freq / sr;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * BAND_Q);

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

        /* Normalize */
        filt.b0 = b0 / a0;
        filt.b1 = b1 / a0;
        filt.b2 = b2 / a0;
        filt.a1 = a1 / a0;
        filt.a2 = a2 / a0;
    }
};

} // namespace mc1dsp
