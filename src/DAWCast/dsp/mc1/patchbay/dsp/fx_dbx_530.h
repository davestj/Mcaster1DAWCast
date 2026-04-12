/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_530.h — dbx 530 Parametric EQ (500 Series)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * 4-band fully parametric EQ with variable HPF and LPF, inspired by
 * the dbx 530 500-series module.  LF and HF are shelving filters;
 * LMF and HMF are peaking bells with adjustable Q.  The HPF and LPF
 * are 2nd-order Butterworth and can be bypassed at their extremes.
 *
 * Signal chain:
 *
 *   Input → HPF → LF Shelf → LMF Bell → HMF Bell → HF Shelf → LPF
 *         → Mix → Output
 *
 *   HPF bypassed when param < 0.01 (≈ 20 Hz, effectively off).
 *   LPF bypassed when param > 0.99 (≈ 20 kHz, effectively off).
 *
 * Header-only, C++17, real-time safe (no allocations in process()).
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx530 : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* ── Parameter indices ──────────────────────────────────────── */

    enum ParamId {
        ParamHPF = 0,      // 0..1 → 20..400 Hz (0 = off)
        ParamLFFreq,       // 0..1 → 40..500 Hz
        ParamLFGain,       // 0..1 → -12..+12 dB (0.5 = flat)
        ParamLMFFreq,      // 0..1 → 200..2000 Hz
        ParamLMFGain,      // 0..1 → -12..+12 dB (0.5 = flat)
        ParamLMFQ,         // 0..1 → Q 0.5..8.0
        ParamHMFFreq,      // 0..1 → 800..8000 Hz
        ParamHMFGain,      // 0..1 → -12..+12 dB (0.5 = flat)
        ParamHMFQ,         // 0..1 → Q 0.5..8.0
        ParamHFFreq,       // 0..1 → 2000..16000 Hz
        ParamHFGain,       // 0..1 → -12..+12 dB (0.5 = flat)
        ParamLPF,          // 0..1 → 2000..20000 Hz (1 = off/20kHz)
        ParamMix,          // 0..1 → 0..100%
        ParamOutput,       // 0..1 → -12..+6 dB
        kParamCount
    };

    FxDbx530() {
        setDefaults();
        computeFilters();
    }

    /* ── DspEffect interface ────────────────────────────────────── */

    const char* name()     const override { return "dbx 530 Parametric EQ"; }
    const char* id()       const override { return "mc1.dbx.530"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::EQ; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        const int useCh = (channels < MAX_CH) ? channels : MAX_CH;

        const float outputLin = std::pow(10.0f, outputDb_ / 20.0f);
        const float mix    = mix_;
        const float dryAmt = 1.0f - mix;
        const bool  hpfOn  = (hpfHz_ > 22.0f);   // param < 0.01 ≈ 20 Hz → off
        const bool  lpfOn  = (lpfHz_ < 19800.0f); // param > 0.99 ≈ 20 kHz → off

        for (size_t f = 0; f < frames; ++f) {

            for (int ch = 0; ch < useCh; ++ch) {
                float dry = pcm[f * channels + ch];
                float x   = dry;

                /* ── HPF (2nd-order Butterworth) ──────────────────── */

                if (hpfOn)
                    x = bqTick(hpf_[ch], x, ch);

                /* ── LF Shelf ─────────────────────────────────────── */

                x = bqTick(lfShelf_[ch], x, ch);

                /* ── LMF Peaking Bell ─────────────────────────────── */

                x = bqTick(lmfBell_[ch], x, ch);

                /* ── HMF Peaking Bell ─────────────────────────────── */

                x = bqTick(hmfBell_[ch], x, ch);

                /* ── HF Shelf ─────────────────────────────────────── */

                x = bqTick(hfShelf_[ch], x, ch);

                /* ── LPF (2nd-order Butterworth) ──────────────────── */

                if (lpfOn)
                    x = bqTick(lpf_[ch], x, ch);

                /* ── Mix + Output gain ────────────────────────────── */

                float wet = x * outputLin;
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
            bqClear(hpf_[c]);
            bqClear(lfShelf_[c]);
            bqClear(lmfBell_[c]);
            bqClear(hmfBell_[c]);
            bqClear(hfShelf_[c]);
            bqClear(lpf_[c]);
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
            "HPF", "LF Freq", "LF Gain",
            "LMF Freq", "LMF Gain", "LMF Q",
            "HMF Freq", "HMF Gain", "HMF Q",
            "HF Freq", "HF Gain", "LPF",
            "Mix", "Output"
        };
        return (index >= 0 && index < kParamCount) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "Hz", "Hz", "dB",
            "Hz", "dB", "",
            "Hz", "dB", "",
            "Hz", "dB", "Hz",
            "%", "dB"
        };
        return (index >= 0 && index < kParamCount) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case ParamHPF:      return (hpfHz_ - 20.0f) / 380.0f;
            case ParamLFFreq:   return (lfFreq_ - 40.0f) / 460.0f;
            case ParamLFGain:   return (lfGainDb_ + 12.0f) / 24.0f;
            case ParamLMFFreq:  return (lmfFreq_ - 200.0f) / 1800.0f;
            case ParamLMFGain:  return (lmfGainDb_ + 12.0f) / 24.0f;
            case ParamLMFQ:     return (lmfQ_ - 0.5f) / 7.5f;
            case ParamHMFFreq:  return (hmfFreq_ - 800.0f) / 7200.0f;
            case ParamHMFGain:  return (hmfGainDb_ + 12.0f) / 24.0f;
            case ParamHMFQ:     return (hmfQ_ - 0.5f) / 7.5f;
            case ParamHFFreq:   return (hfFreq_ - 2000.0f) / 14000.0f;
            case ParamHFGain:   return (hfGainDb_ + 12.0f) / 24.0f;
            case ParamLPF:      return (lpfHz_ - 2000.0f) / 18000.0f;
            case ParamMix:      return mix_;
            case ParamOutput:   return (outputDb_ + 12.0f) / 18.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (index) {
            case ParamHPF:
                hpfHz_ = v * 380.0f + 20.0f;
                computeFilters();
                break;
            case ParamLFFreq:
                lfFreq_ = v * 460.0f + 40.0f;
                computeFilters();
                break;
            case ParamLFGain:
                lfGainDb_ = v * 24.0f - 12.0f;
                computeFilters();
                break;
            case ParamLMFFreq:
                lmfFreq_ = v * 1800.0f + 200.0f;
                computeFilters();
                break;
            case ParamLMFGain:
                lmfGainDb_ = v * 24.0f - 12.0f;
                computeFilters();
                break;
            case ParamLMFQ:
                lmfQ_ = v * 7.5f + 0.5f;
                computeFilters();
                break;
            case ParamHMFFreq:
                hmfFreq_ = v * 7200.0f + 800.0f;
                computeFilters();
                break;
            case ParamHMFGain:
                hmfGainDb_ = v * 24.0f - 12.0f;
                computeFilters();
                break;
            case ParamHMFQ:
                hmfQ_ = v * 7.5f + 0.5f;
                computeFilters();
                break;
            case ParamHFFreq:
                hfFreq_ = v * 14000.0f + 2000.0f;
                computeFilters();
                break;
            case ParamHFGain:
                hfGainDb_ = v * 24.0f - 12.0f;
                computeFilters();
                break;
            case ParamLPF:
                lpfHz_ = v * 18000.0f + 2000.0f;
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
            case ParamHPF:
                if (hpfHz_ <= 22.0f)
                    snprintf(buf, sizeof(buf), "Off");
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", hpfHz_);
                break;
            case ParamLFFreq:
                snprintf(buf, sizeof(buf), "%.0f Hz", lfFreq_);
                break;
            case ParamLFGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", lfGainDb_);
                break;
            case ParamLMFFreq:
                if (lmfFreq_ >= 1000.0f)
                    snprintf(buf, sizeof(buf), "%.2f kHz", lmfFreq_ / 1000.0f);
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", lmfFreq_);
                break;
            case ParamLMFGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", lmfGainDb_);
                break;
            case ParamLMFQ:
                snprintf(buf, sizeof(buf), "Q %.1f", lmfQ_);
                break;
            case ParamHMFFreq:
                if (hmfFreq_ >= 1000.0f)
                    snprintf(buf, sizeof(buf), "%.2f kHz", hmfFreq_ / 1000.0f);
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", hmfFreq_);
                break;
            case ParamHMFGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", hmfGainDb_);
                break;
            case ParamHMFQ:
                snprintf(buf, sizeof(buf), "Q %.1f", hmfQ_);
                break;
            case ParamHFFreq:
                if (hfFreq_ >= 1000.0f)
                    snprintf(buf, sizeof(buf), "%.2f kHz", hfFreq_ / 1000.0f);
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", hfFreq_);
                break;
            case ParamHFGain:
                snprintf(buf, sizeof(buf), "%+.1f dB", hfGainDb_);
                break;
            case ParamLPF:
                if (lpfHz_ >= 19800.0f)
                    snprintf(buf, sizeof(buf), "Off");
                else if (lpfHz_ >= 1000.0f)
                    snprintf(buf, sizeof(buf), "%.1f kHz", lpfHz_ / 1000.0f);
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", lpfHz_);
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

    float hpfHz_      =  20.0f;    // 20..400 Hz  (default: off)
    float lfFreq_     = 132.0f;    // 40..500 Hz
    float lfGainDb_   =   0.0f;    // -12..+12 dB
    float lmfFreq_    = 740.0f;    // 200..2000 Hz
    float lmfGainDb_  =   0.0f;    // -12..+12 dB
    float lmfQ_       =   2.75f;   // 0.5..8.0
    float hmfFreq_    = 3680.0f;   // 800..8000 Hz
    float hmfGainDb_  =   0.0f;    // -12..+12 dB
    float hmfQ_       =   2.75f;   // 0.5..8.0
    float hfFreq_     = 7600.0f;   // 2000..16000 Hz
    float hfGainDb_   =   0.0f;    // -12..+12 dB
    float lpfHz_      = 20000.0f;  // 2000..20000 Hz (default: off)
    float mix_        =   1.0f;    // 0..1
    float outputDb_   =   0.006f;  // -12..+6 dB (default ~0)

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

    /* ── Filter instances (per channel) ─────────────────────────── */

    BQ hpf_[MAX_CH]     = {};      // Highpass filter
    BQ lfShelf_[MAX_CH]  = {};     // Low-frequency shelf
    BQ lmfBell_[MAX_CH]  = {};     // Low-mid peaking bell
    BQ hmfBell_[MAX_CH]  = {};     // High-mid peaking bell
    BQ hfShelf_[MAX_CH]  = {};     // High-frequency shelf
    BQ lpf_[MAX_CH]      = {};     // Lowpass filter

    /* ── Pi constant ────────────────────────────────────────────── */

    static constexpr float kPi = 3.14159265358979323846f;

    /* ── Set defaults from normalized values ────────────────────── */

    void setDefaults() {
        /* Normalized defaults -> real values:
         * HPF       0.0   -> 20 Hz (off)
         * LFFreq    0.2   -> 132 Hz
         * LFGain    0.5   -> 0 dB (flat)
         * LMFFreq   0.3   -> 740 Hz
         * LMFGain   0.5   -> 0 dB (flat)
         * LMFQ      0.3   -> Q ~2.75
         * HMFFreq   0.4   -> 3680 Hz
         * HMFGain   0.5   -> 0 dB (flat)
         * HMFQ      0.3   -> Q ~2.75
         * HFFreq    0.4   -> 7600 Hz
         * HFGain    0.5   -> 0 dB (flat)
         * LPF       1.0   -> 20000 Hz (off)
         * Mix       1.0   -> 100%
         * Output    0.667 -> ~0 dB
         */
        hpfHz_      = 0.0f * 380.0f + 20.0f;              // 20 Hz (off)
        lfFreq_     = 0.2f * 460.0f + 40.0f;              // 132 Hz
        lfGainDb_   = 0.5f * 24.0f - 12.0f;               // 0 dB
        lmfFreq_    = 0.3f * 1800.0f + 200.0f;            // 740 Hz
        lmfGainDb_  = 0.5f * 24.0f - 12.0f;               // 0 dB
        lmfQ_       = 0.3f * 7.5f + 0.5f;                 // Q 2.75
        hmfFreq_    = 0.4f * 7200.0f + 800.0f;            // 3680 Hz
        hmfGainDb_  = 0.5f * 24.0f - 12.0f;               // 0 dB
        hmfQ_       = 0.3f * 7.5f + 0.5f;                 // Q 2.75
        hfFreq_     = 0.4f * 14000.0f + 2000.0f;          // 7600 Hz
        hfGainDb_   = 0.5f * 24.0f - 12.0f;               // 0 dB
        lpfHz_      = 1.0f * 18000.0f + 2000.0f;          // 20000 Hz (off)
        mix_        = 1.0f;                                // 100%
        outputDb_   = 0.667f * 18.0f - 12.0f;             // +0.006 dB (~0)
    }

    /* ── Compute all filter coefficients ────────────────────────── */

    void computeFilters() {
        float sr  = static_cast<float>(sampleRate_);
        float nyq = sr * 0.49f;

        /* ── HPF (2nd-order Butterworth highpass) ──────────────────── */

        {
            float freq = hpfHz_;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > nyq)   freq = nyq;
            for (int c = 0; c < MAX_CH; ++c)
                computeHP(hpf_[c], freq, sr);
        }

        /* ── LF Shelf (RBJ low shelf) ─────────────────────────────── */

        {
            float freq = lfFreq_;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > nyq)   freq = nyq;
            for (int c = 0; c < MAX_CH; ++c)
                computeLowShelf(lfShelf_[c], freq, lfGainDb_, sr);
        }

        /* ── LMF Peaking Bell (RBJ peaking EQ) ────────────────────── */

        {
            float freq = lmfFreq_;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > nyq)   freq = nyq;
            for (int c = 0; c < MAX_CH; ++c)
                computePeaking(lmfBell_[c], freq, lmfGainDb_, lmfQ_, sr);
        }

        /* ── HMF Peaking Bell (RBJ peaking EQ) ────────────────────── */

        {
            float freq = hmfFreq_;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > nyq)   freq = nyq;
            for (int c = 0; c < MAX_CH; ++c)
                computePeaking(hmfBell_[c], freq, hmfGainDb_, hmfQ_, sr);
        }

        /* ── HF Shelf (RBJ high shelf) ────────────────────────────── */

        {
            float freq = hfFreq_;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > nyq)   freq = nyq;
            for (int c = 0; c < MAX_CH; ++c)
                computeHighShelf(hfShelf_[c], freq, hfGainDb_, sr);
        }

        /* ── LPF (2nd-order Butterworth lowpass) ──────────────────── */

        {
            float freq = lpfHz_;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > nyq)   freq = nyq;
            for (int c = 0; c < MAX_CH; ++c)
                computeLP(lpf_[c], freq, sr);
        }
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

    /* ── RBJ Cookbook: Low shelf ─────────────────────────────────── */

    static void computeLowShelf(BQ& f, float freq, float gainDb, float sr) {
        float A     = std::pow(10.0f, gainDb / 40.0f);
        float w0    = 2.0f * kPi * freq / sr;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * 0.7071f);
        float sqA   = std::sqrt(A);

        float b0 =        A * ((A+1) - (A-1)*cosw0 + 2*sqA*alpha);
        float b1 =  2.0f *A * ((A-1) - (A+1)*cosw0);
        float b2 =        A * ((A+1) - (A-1)*cosw0 - 2*sqA*alpha);
        float a0 =              (A+1) + (A-1)*cosw0 + 2*sqA*alpha;
        float a1 = -2.0f *     ((A-1) + (A+1)*cosw0);
        float a2 =              (A+1) + (A-1)*cosw0 - 2*sqA*alpha;

        f.b0 = b0 / a0;
        f.b1 = b1 / a0;
        f.b2 = b2 / a0;
        f.a1 = a1 / a0;
        f.a2 = a2 / a0;
    }

    /* ── RBJ Cookbook: High shelf ────────────────────────────────── */

    static void computeHighShelf(BQ& f, float freq, float gainDb, float sr) {
        float A     = std::pow(10.0f, gainDb / 40.0f);
        float w0    = 2.0f * kPi * freq / sr;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * 0.7071f);
        float sqA   = std::sqrt(A);

        float b0 =        A * ((A+1) + (A-1)*cosw0 + 2*sqA*alpha);
        float b1 = -2.0f *A * ((A-1) + (A+1)*cosw0);
        float b2 =        A * ((A+1) + (A-1)*cosw0 - 2*sqA*alpha);
        float a0 =              (A+1) - (A-1)*cosw0 + 2*sqA*alpha;
        float a1 =  2.0f *     ((A-1) - (A+1)*cosw0);
        float a2 =              (A+1) - (A-1)*cosw0 - 2*sqA*alpha;

        f.b0 = b0 / a0;
        f.b1 = b1 / a0;
        f.b2 = b2 / a0;
        f.a1 = a1 / a0;
        f.a2 = a2 / a0;
    }

    /* ── RBJ Cookbook: Peaking EQ (bell) ─────────────────────────── */

    static void computePeaking(BQ& f, float freq, float gainDb, float Q,
                               float sr) {
        float A     = std::pow(10.0f, gainDb / 40.0f);
        float w0    = 2.0f * kPi * freq / sr;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        float b0 =  1.0f + alpha * A;
        float b1 = -2.0f * cosw0;
        float b2 =  1.0f - alpha * A;
        float a0 =  1.0f + alpha / A;
        float a1 = -2.0f * cosw0;
        float a2 =  1.0f - alpha / A;

        f.b0 = b0 / a0;
        f.b1 = b1 / a0;
        f.b2 = b2 / a0;
        f.a1 = a1 / a0;
        f.a2 = a2 / a0;
    }
};

} // namespace mc1dsp
