/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_510.h — dbx 510 Subharmonic Synthesizer (500 Series)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Two-band subharmonic synthesizer inspired by the dbx 510 500-series
 * module.  Generates subharmonics one octave below the input's low-
 * frequency content using zero-crossing octave division.
 *
 * Signal chain:
 *
 *   Input ──┬── BPF 48-72 Hz ── Octave Divider ── LP 36 Hz ── Band1 Level ──┐
 *           │                                                                 │
 *           ├── BPF 72-112 Hz ── Octave Divider ── LP 56 Hz ── Band2 Level ──┤
 *           │                                                                 │
 *           │                                      Synth Level ←── Sum ◄─────┘
 *           │                                          ↓
 *           └── Dry ─────────────── + ◄── HPF 20 Hz ──┘
 *                                   ↓
 *                               Mix ── Output
 *
 *   Zero-crossing octave divider: detects sign changes in the bandpassed
 *   signal and toggles a square wave at half frequency.  A lowpass filter
 *   smooths the square wave into a sine-like subharmonic.
 *
 * Header-only, C++17, real-time safe (no allocations in process()).
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx510 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* ── Parameter indices ──────────────────────────────────────── */

    enum ParamId {
        ParamBand1Level = 0,   // 0..1 → 0..100% (24-36 Hz band)
        ParamBand2Level,       // 0..1 → 0..100% (36-56 Hz band)
        ParamSynthLevel,       // 0..1 → 0..100% (overall subharmonic level)
        ParamCrossover,        // 0..1 → 50..120 Hz (crossover between bands)
        ParamTightness,        // 0..1 → 0..100% (output HPF: 15-30 Hz)
        ParamMix,              // 0..1 → 0..100%
        ParamOutput,           // 0..1 → -12..+6 dB
        kParamCount
    };

    FxDbx510() {
        setDefaults();
        computeFilters();
    }

    /* ── DspEffect interface ────────────────────────────────────── */

    const char* name()     const override { return "dbx 510 Subharmonic Synthesizer"; }
    const char* id()       const override { return "mc1.dbx.510"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        const int useCh = (channels < MAX_CH) ? channels : MAX_CH;

        const float band1Lvl  = band1Level_;
        const float band2Lvl  = band2Level_;
        const float synthLvl  = synthLevel_;
        const float mix       = mix_;
        const float dryAmt    = 1.0f - mix;
        const float outputLin = std::pow(10.0f, outputDb_ / 20.0f);

        for (size_t f = 0; f < frames; ++f) {

            for (int ch = 0; ch < useCh; ++ch) {
                float dry = pcm[f * channels + ch];

                /* ── Band 1: 48-72 Hz bandpass → octave divider → LP 36 Hz ── */

                float bp1 = bqTick(band1Bpf1_[ch], dry, ch);
                bp1       = bqTick(band1Bpf2_[ch], bp1, ch);   // 4th-order

                // Zero-crossing octave divider for band 1
                bool curSign1 = (bp1 >= 0.0f);
                if (curSign1 != b1LastSign_[ch]) {
                    b1Toggle_[ch] = !b1Toggle_[ch];
                }
                b1LastSign_[ch] = curSign1;
                float sub1Raw = b1Toggle_[ch] ? 1.0f : -1.0f;

                // Scale square wave by bandpass envelope for amplitude tracking
                float bp1Abs = std::fabs(bp1);
                b1Env_[ch] += 0.001f * (bp1Abs - b1Env_[ch]);  // slow follower
                float sub1 = sub1Raw * b1Env_[ch];

                // Lowpass to smooth square wave into sine-like subharmonic
                sub1 = bqTick(band1Lp_[ch], sub1, ch);
                sub1 *= band1Lvl;

                /* ── Band 2: 72-112 Hz bandpass → octave divider → LP 56 Hz ── */

                float bp2 = bqTick(band2Bpf1_[ch], dry, ch);
                bp2       = bqTick(band2Bpf2_[ch], bp2, ch);   // 4th-order

                // Zero-crossing octave divider for band 2
                bool curSign2 = (bp2 >= 0.0f);
                if (curSign2 != b2LastSign_[ch]) {
                    b2Toggle_[ch] = !b2Toggle_[ch];
                }
                b2LastSign_[ch] = curSign2;
                float sub2Raw = b2Toggle_[ch] ? 1.0f : -1.0f;

                // Scale by bandpass envelope
                float bp2Abs = std::fabs(bp2);
                b2Env_[ch] += 0.001f * (bp2Abs - b2Env_[ch]);
                float sub2 = sub2Raw * b2Env_[ch];

                // Lowpass to smooth
                sub2 = bqTick(band2Lp_[ch], sub2, ch);
                sub2 *= band2Lvl;

                /* ── Combine subharmonics ──────────────────────────────── */

                float synthMix = (sub1 + sub2) * synthLvl;

                /* ── Output HPF at 20 Hz (speaker protection) ─────────── */

                synthMix = bqTick(outputHpf_[ch], synthMix, ch);

                /* ── Mix with dry signal ──────────────────────────────── */

                float wet = (dry + synthMix) * outputLin;
                pcm[f * channels + ch] = wet * mix + dry * dryAmt;
            }

            /* ── Output metering ───────────────────────────────────── */

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
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(band1Bpf1_[c]);
            bqClear(band1Bpf2_[c]);
            bqClear(band1Lp_[c]);
            bqClear(band2Bpf1_[c]);
            bqClear(band2Bpf2_[c]);
            bqClear(band2Lp_[c]);
            bqClear(outputHpf_[c]);

            b1LastSign_[c] = false;
            b1Toggle_[c]   = false;
            b1Env_[c]      = 0.0f;
            b2LastSign_[c] = false;
            b2Toggle_[c]   = false;
            b2Env_[c]      = 0.0f;
        }

        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
        clipping_.store(false);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeFilters();
    }

    /* ── Parameters ─────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Band 1 Level", "Band 2 Level", "Synth Level",
            "Crossover", "Tightness", "Mix", "Output"
        };
        return (index >= 0 && index < kParamCount) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "%", "%", "%",
            "Hz", "%", "%", "dB"
        };
        return (index >= 0 && index < kParamCount) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case ParamBand1Level: return band1Level_;
            case ParamBand2Level: return band2Level_;
            case ParamSynthLevel: return synthLevel_;
            case ParamCrossover:  return (crossoverHz_ - 50.0f) / 70.0f;
            case ParamTightness:  return tightness_;
            case ParamMix:        return mix_;
            case ParamOutput:     return (outputDb_ + 12.0f) / 18.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (index) {
            case ParamBand1Level:
                band1Level_ = v;
                break;
            case ParamBand2Level:
                band2Level_ = v;
                break;
            case ParamSynthLevel:
                synthLevel_ = v;
                break;
            case ParamCrossover:
                crossoverHz_ = v * 70.0f + 50.0f;
                computeFilters();
                break;
            case ParamTightness:
                tightness_ = v;
                computeFilters();
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
            case ParamBand1Level:
                snprintf(buf, sizeof(buf), "%.0f%%", band1Level_ * 100.0f);
                break;
            case ParamBand2Level:
                snprintf(buf, sizeof(buf), "%.0f%%", band2Level_ * 100.0f);
                break;
            case ParamSynthLevel:
                snprintf(buf, sizeof(buf), "%.0f%%", synthLevel_ * 100.0f);
                break;
            case ParamCrossover:
                snprintf(buf, sizeof(buf), "%.0f Hz", crossoverHz_);
                break;
            case ParamTightness:
                snprintf(buf, sizeof(buf), "%.0f%%", tightness_ * 100.0f);
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

    float band1Level_  = 0.5f;    // 0..1 -> 0..100%
    float band2Level_  = 0.5f;    // 0..1 -> 0..100%
    float synthLevel_  = 0.6f;    // 0..1 -> 0..100%
    float crossoverHz_ = 85.0f;   // 50..120 Hz
    float tightness_   = 0.4f;    // 0..1 -> HPF 15..30 Hz
    float mix_         = 0.8f;    // 0..1
    float outputDb_    = 0.006f;  // -12..+6 dB (default ~0)

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

    /* ── Zero-crossing octave divider state (per channel) ──────── */

    bool  b1LastSign_[MAX_CH] = {};     // Band 1: sign of last sample
    bool  b1Toggle_[MAX_CH]   = {};     // Band 1: half-frequency toggle
    float b1Env_[MAX_CH]      = {};     // Band 1: envelope follower

    bool  b2LastSign_[MAX_CH] = {};     // Band 2: sign of last sample
    bool  b2Toggle_[MAX_CH]   = {};     // Band 2: half-frequency toggle
    float b2Env_[MAX_CH]      = {};     // Band 2: envelope follower

    /* ── Filter instances (per channel) ─────────────────────────── */

    BQ band1Bpf1_[MAX_CH] = {};    // Band 1 bandpass, stage 1
    BQ band1Bpf2_[MAX_CH] = {};    // Band 1 bandpass, stage 2 (4th-order)
    BQ band1Lp_[MAX_CH]   = {};    // Band 1 subharmonic smoothing LP

    BQ band2Bpf1_[MAX_CH] = {};    // Band 2 bandpass, stage 1
    BQ band2Bpf2_[MAX_CH] = {};    // Band 2 bandpass, stage 2 (4th-order)
    BQ band2Lp_[MAX_CH]   = {};    // Band 2 subharmonic smoothing LP

    BQ outputHpf_[MAX_CH]  = {};   // Output highpass (speaker protection)

    /* ── Pi constant ────────────────────────────────────────────── */

    static constexpr float kPi = 3.14159265358979323846f;

    /* ── Set defaults from normalized values ────────────────────── */

    void setDefaults() {
        /* Normalized defaults -> real values:
         * Band1Level  0.5   -> 50%
         * Band2Level  0.5   -> 50%
         * SynthLevel  0.6   -> 60%
         * Crossover   0.5   -> 85 Hz
         * Tightness   0.4   -> 40%
         * Mix         0.8   -> 80%
         * Output      0.667 -> ~0 dB
         */
        band1Level_  = 0.5f;
        band2Level_  = 0.5f;
        synthLevel_  = 0.6f;
        crossoverHz_ = 0.5f * 70.0f + 50.0f;           // 85 Hz
        tightness_   = 0.4f;
        mix_         = 0.8f;
        outputDb_    = 0.667f * 18.0f - 12.0f;          // +0.006 dB (~0)
    }

    /* ── Compute all filter coefficients ────────────────────────── */

    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);

        /* ── Band 1: Bandpass at ~60 Hz center (48-72 Hz), Q ~2 ──── */
        /*    Center frequency is crossover-relative:                  */
        /*    Band 1 center = crossover * 0.7 (approx 60 Hz at 85)    */

        float b1Center = crossoverHz_ * 0.706f;
        float nyq = sr * 0.49f;
        if (b1Center > nyq) b1Center = nyq;
        if (b1Center < 20.0f) b1Center = 20.0f;

        for (int c = 0; c < MAX_CH; ++c) {
            computeBPF(band1Bpf1_[c], b1Center, 2.0f, sr);
            computeBPF(band1Bpf2_[c], b1Center, 2.0f, sr);
        }

        /* ── Band 1 subharmonic smoothing LP at 36 Hz ──────────────── */
        /*    Cutoff is half of band 1 center (~half the BPF center)    */

        float lp1Freq = b1Center * 0.6f;   // ~36 Hz at default
        if (lp1Freq < 10.0f) lp1Freq = 10.0f;
        if (lp1Freq > nyq) lp1Freq = nyq;

        for (int c = 0; c < MAX_CH; ++c)
            computeLP(band1Lp_[c], lp1Freq, sr);

        /* ── Band 2: Bandpass at ~90 Hz center (72-112 Hz), Q ~2 ──── */

        float b2Center = crossoverHz_ * 1.06f;
        if (b2Center > nyq) b2Center = nyq;
        if (b2Center < 20.0f) b2Center = 20.0f;

        for (int c = 0; c < MAX_CH; ++c) {
            computeBPF(band2Bpf1_[c], b2Center, 2.0f, sr);
            computeBPF(band2Bpf2_[c], b2Center, 2.0f, sr);
        }

        /* ── Band 2 subharmonic smoothing LP at 56 Hz ──────────────── */

        float lp2Freq = b2Center * 0.62f;  // ~56 Hz at default
        if (lp2Freq < 10.0f) lp2Freq = 10.0f;
        if (lp2Freq > nyq) lp2Freq = nyq;

        for (int c = 0; c < MAX_CH; ++c)
            computeLP(band2Lp_[c], lp2Freq, sr);

        /* ── Output HPF: tightness param controls 15-30 Hz ─────────── */
        /*    At tightness 0 -> 15 Hz, tightness 1 -> 30 Hz             */

        float hpfFreq = tightness_ * 15.0f + 15.0f;
        if (hpfFreq > nyq) hpfFreq = nyq;
        if (hpfFreq < 5.0f) hpfFreq = 5.0f;

        for (int c = 0; c < MAX_CH; ++c)
            computeHP(outputHpf_[c], hpfFreq, sr);
    }

    /* ── RBJ Cookbook: Bandpass filter (constant 0 dB peak) ──────── */

    static void computeBPF(BQ& f, float freq, float Q, float sr) {
        float w0    = 2.0f * kPi * freq / sr;
        float cosw  = std::cos(w0);
        float sinw  = std::sin(w0);
        float alpha = sinw / (2.0f * Q);

        float a0 =  1.0f + alpha;
        f.b0 =  alpha / a0;
        f.b1 =  0.0f;
        f.b2 = -alpha / a0;
        f.a1 = -2.0f * cosw / a0;
        f.a2 =  (1.0f - alpha) / a0;
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
