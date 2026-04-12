/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_560a.h — dbx 560A Compressor / Limiter (500 Series)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Classic dbx VCA compressor with true RMS detection and OverEasy knee,
 * in 500-series format.  Signal chain:
 *
 *   Input -> Sidechain HPF (optional) -> True RMS Detector
 *         -> OverEasy Soft-Knee Compressor -> Envelope Follower
 *         -> Gain Reduction -> Output Gain -> Dry/Wet Mix
 *
 * The OverEasy knee uses cubic interpolation across a 10 dB transition
 * zone centered on the threshold.  Auto-release provides program-dependent
 * timing that speeds up on transients and slows on sustained material.
 *
 * Header-only, C++17, real-time safe (no allocations in process()).
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx560A : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* ── Parameter indices ──────────────────────────────────────── */

    enum ParamId {
        ParamThreshold = 0,    // 0..1 -> -40..0 dB
        ParamRatio,            // 0..1 -> 1:1..20:1
        ParamAttack,           // 0..1 -> 1..100 ms
        ParamRelease,          // 0..1 -> 50..500 ms
        ParamKnee,             // 0..1 -> hard(0)..OverEasy(1)
        ParamScHPF,            // 0..1 -> off(0)/80..300 Hz sidechain HPF
        ParamAuto,             // 0..1 -> off/on auto-release (threshold 0.5)
        ParamOutput,           // 0..1 -> -12..+6 dB
        ParamMix,              // 0..1 -> 0..100%
        kParamCount
    };

    FxDbx560A() {
        setDefaults();
        computeCoeffs();
    }

    /* ── DspEffect interface ────────────────────────────────────── */

    const char* name()     const override { return "dbx 560A Compressor/Limiter"; }
    const char* id()       const override { return "mc1.dbx.560a"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        const int useCh = (channels < MAX_CH) ? channels : MAX_CH;

        const float outputGainLin = std::pow(10.0f, outputDb_ / 20.0f);
        const float mix    = mix_;
        const float dryAmt = 1.0f - mix;
        const float ratio  = ratio_;
        const float threshDb = thresholdDb_;
        const bool  overEasy = (knee_ >= 0.01f);
        const float kneeHalfWidth = knee_ * 5.0f;   /* 0..5 dB half-width (10 dB max zone) */
        const bool  autoRelease = (autoRelease_ >= 0.5f);

        for (size_t f = 0; f < frames; ++f) {

            /* ── True RMS detection (squared-sample ring buffer) ───── */

            /* Sum mono for detector across channels */
            float monoSample = 0.0f;
            for (int ch = 0; ch < useCh; ++ch)
                monoSample += pcm[f * channels + ch];
            if (useCh > 1) monoSample *= 0.5f;

            /* Apply sidechain HPF to detector path only */
            float scSample = monoSample;
            if (scHpfActive_)
                scSample = bqTick(scHpf_, scSample);

            /* Ring buffer: store squared sample, maintain running sum */
            float sq = scSample * scSample;
            rmsSum_ -= rmsRing_[rmsPos_];
            rmsRing_[rmsPos_] = sq;
            rmsSum_ += sq;
            rmsPos_ = (rmsPos_ + 1) & (kRmsWindowMax - 1);

            /* Guard against floating-point drift producing negative sums */
            if (rmsSum_ < 0.0f) rmsSum_ = 0.0f;

            float rmsLin = std::sqrt(rmsSum_ / static_cast<float>(rmsWindowLen_));
            float envDb = (rmsLin > 1e-10f)
                        ? 20.0f * std::log10(rmsLin) : -96.0f;

            /* Input metering */
            meterInputPeak_.store(envDb, std::memory_order_relaxed);

            /* ── OverEasy Compressor gain computation ───────────────── */

            float compGrDb = 0.0f;
            float ratioComp = 1.0f - 1.0f / ratio;

            if (overEasy && kneeHalfWidth > 0.01f) {
                /* Cubic-interpolated soft knee within +/-kneeHalfWidth of threshold */
                float kneeBottom = threshDb - kneeHalfWidth;
                float kneeTop    = threshDb + kneeHalfWidth;

                if (envDb >= kneeTop) {
                    /* Full compression above the knee */
                    compGrDb = (envDb - threshDb) * ratioComp;
                } else if (envDb > kneeBottom) {
                    /* Inside the OverEasy transition zone — cubic interpolation.
                     * Normalized position t = 0 (bottom) to 1 (top). Cubic provides
                     * smoother onset than the quadratic used in the 266xs. */
                    float t = (envDb - kneeBottom) / (kneeTop - kneeBottom);
                    float t2 = t * t;
                    float t3 = t2 * t;
                    /* Hermite basis: smooth from 0 GR at bottom to full-ratio GR at top */
                    float blend = 3.0f * t2 - 2.0f * t3;  /* smoothstep */
                    float fullGr = (envDb - threshDb) * ratioComp;
                    compGrDb = blend * fullGr;
                    /* Ensure non-negative GR */
                    if (compGrDb < 0.0f) compGrDb = 0.0f;
                }
                /* Below kneeBottom: compGrDb stays 0 */
            } else {
                /* Hard knee */
                if (envDb > threshDb) {
                    compGrDb = (envDb - threshDb) * ratioComp;
                }
            }

            /* ── Envelope follower with attack/release smoothing ────── */

            float target = std::pow(10.0f, -compGrDb / 20.0f);
            float coeff;

            if (target < compEnv_) {
                /* Attack: gain is decreasing (more compression) */
                coeff = attackCoeff_;
            } else {
                /* Release: gain is returning toward unity */
                if (autoRelease) {
                    /* Program-dependent auto release:
                     * Fast release when GR is changing rapidly (transients),
                     * slow release on sustained material. */
                    float grDelta = std::fabs(target - compEnv_);
                    float blend = grDelta * 20.0f;  /* scale delta into 0..1 range */
                    blend = std::max(0.0f, std::min(1.0f, blend));
                    coeff = fastReleaseCoeff_ * blend
                          + slowReleaseCoeff_ * (1.0f - blend);
                } else {
                    coeff = releaseCoeff_;
                }
            }

            compEnv_ += coeff * (target - compEnv_);

            /* Store gain reduction for meter readout (positive dB) */
            float envGrDb = (compEnv_ > 1e-10f)
                          ? -20.0f * std::log10(compEnv_) : 0.0f;
            if (envGrDb < 0.0f) envGrDb = 0.0f;
            meterGainReduction_.store(envGrDb, std::memory_order_relaxed);

            /* ── Apply gain reduction + output + mix ────────────────── */

            for (int ch = 0; ch < useCh; ++ch) {
                float dry = pcm[f * channels + ch];
                float wet = dry * compEnv_ * outputGainLin;
                pcm[f * channels + ch] = wet * mix + dry * dryAmt;
            }

            /* Output metering */
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
        /* Clear RMS ring buffer */
        for (int i = 0; i < kRmsWindowMax; ++i)
            rmsRing_[i] = 0.0f;
        rmsSum_ = 0.0f;
        rmsPos_ = 0;

        compEnv_ = 1.0f;
        bqClear(scHpf_);

        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
        clipping_.store(false);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeCoeffs();
    }

    /* ── Parameters ─────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Threshold", "Ratio", "Attack", "Release",
            "Knee", "SC HPF", "Auto Release",
            "Output", "Mix"
        };
        return (index >= 0 && index < kParamCount) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dB", ":1", "ms", "ms",
            "", "Hz", "",
            "dB", "%"
        };
        return (index >= 0 && index < kParamCount) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case ParamThreshold:  return (thresholdDb_ + 40.0f) / 40.0f;
            case ParamRatio:      return (ratio_ - 1.0f) / 19.0f;
            case ParamAttack:     return (attackMs_ - 1.0f) / 99.0f;
            case ParamRelease:    return (releaseMs_ - 50.0f) / 450.0f;
            case ParamKnee:       return knee_;
            case ParamScHPF:      return scHpfNorm_;
            case ParamAuto:       return autoRelease_;
            case ParamOutput:     return (outputDb_ + 12.0f) / 18.0f;
            case ParamMix:        return mix_;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (index) {
            case ParamThreshold:
                thresholdDb_ = v * 40.0f - 40.0f;
                break;
            case ParamRatio:
                ratio_ = v * 19.0f + 1.0f;
                break;
            case ParamAttack:
                attackMs_ = v * 99.0f + 1.0f;
                computeCoeffs();
                break;
            case ParamRelease:
                releaseMs_ = v * 450.0f + 50.0f;
                computeCoeffs();
                break;
            case ParamKnee:
                knee_ = v;
                break;
            case ParamScHPF:
                scHpfNorm_ = v;
                scHpfActive_ = (v > 0.001f);
                computeScHPF();
                break;
            case ParamAuto:
                autoRelease_ = (v >= 0.5f) ? 1.0f : 0.0f;
                break;
            case ParamOutput:
                outputDb_ = v * 18.0f - 12.0f;
                break;
            case ParamMix:
                mix_ = v;
                break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[48];
        switch (index) {
            case ParamThreshold:
                snprintf(buf, sizeof(buf), "%+.1f dB", thresholdDb_);
                break;
            case ParamRatio:
                snprintf(buf, sizeof(buf), "%.1f:1", ratio_);
                break;
            case ParamAttack:
                snprintf(buf, sizeof(buf), "%.1f ms", attackMs_);
                break;
            case ParamRelease:
                snprintf(buf, sizeof(buf), "%.0f ms", releaseMs_);
                break;
            case ParamKnee:
                if (knee_ < 0.01f)
                    snprintf(buf, sizeof(buf), "Hard");
                else
                    snprintf(buf, sizeof(buf), "OE %.0f%%", knee_ * 100.0f);
                break;
            case ParamScHPF:
                if (!scHpfActive_)
                    snprintf(buf, sizeof(buf), "Off");
                else
                    snprintf(buf, sizeof(buf), "%.0f Hz", mapScHpfFreq());
                break;
            case ParamAuto:
                snprintf(buf, sizeof(buf), "%s",
                         (autoRelease_ >= 0.5f) ? "On" : "Off");
                break;
            case ParamOutput:
                snprintf(buf, sizeof(buf), "%+.1f dB", outputDb_);
                break;
            case ParamMix:
                snprintf(buf, sizeof(buf), "%.0f%%", mix_ * 100.0f);
                break;
            default:
                buf[0] = 0;
        }
        return buf;
    }

private:
    /* ── Parameter storage ──────────────────────────────────────── */

    float thresholdDb_  = -10.0f;  // -40..0 dB
    float ratio_        =   3.85f; // 1:1..20:1
    float attackMs_     =  10.9f;  // 1..100 ms
    float releaseMs_    = 185.0f;  // 50..500 ms
    float knee_         =   1.0f;  // 0=hard, 1=full OverEasy
    float scHpfNorm_    =   0.0f;  // 0..1 normalized HPF control
    float autoRelease_  =   0.0f;  // 0=off, 1=on
    float outputDb_     =   0.006f;// -12..+6 dB
    float mix_          =   1.0f;  // 0..1

    bool  scHpfActive_  = false;

    /* ── True RMS ring buffer ───────────────────────────────────── */

    /* Window size must be power-of-two for fast modulo.
     * At 48 kHz, 512 samples ~ 10.67 ms RMS window. */
    static constexpr int kRmsWindowMax = 512;

    float rmsRing_[kRmsWindowMax] = {};  /* squared samples */
    float rmsSum_       = 0.0f;          /* running sum of ring */
    int   rmsPos_       = 0;             /* write index */
    int   rmsWindowLen_ = 480;           /* actual window length (samples) */

    /* ── Compressor envelope state ──────────────────────────────── */

    float compEnv_          = 1.0f;   /* gain reduction envelope (linear) */

    /* ── Time-constant coefficients ─────────────────────────────── */

    float attackCoeff_      = 0.0f;
    float releaseCoeff_     = 0.0f;
    float fastReleaseCoeff_ = 0.0f;   /* auto-release: ~50 ms */
    float slowReleaseCoeff_ = 0.0f;   /* auto-release: ~500 ms */

    /* ── Sidechain HPF biquad ───────────────────────────────────── */

    struct BQ {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };

    BQ scHpf_ = {};

    static float bqTick(BQ& f, float x) {
        float y = f.b0*x + f.b1*f.x1 + f.b2*f.x2
                - f.a1*f.y1 - f.a2*f.y2;
        f.x2 = f.x1; f.x1 = x;
        f.y2 = f.y1; f.y1 = y;
        return y;
    }

    static void bqClear(BQ& f) {
        f.x1 = f.x2 = f.y1 = f.y2 = 0;
    }

    /* ── Pi constant ────────────────────────────────────────────── */

    static constexpr float kPi = 3.14159265358979323846f;

    /* ── Parameter mapping helpers ──────────────────────────────── */

    float mapScHpfFreq() const {
        /* 0..1 -> 80..300 Hz */
        return 80.0f + scHpfNorm_ * 220.0f;
    }

    /* ── Set defaults from normalized values ────────────────────── */

    void setDefaults() {
        /* Normalized defaults -> real values:
         * Threshold  0.75  -> -10 dB
         * Ratio      0.15  -> ~3.85:1
         * Attack     0.1   -> ~10.9 ms
         * Release    0.3   -> ~185 ms
         * Knee       1.0   -> full OverEasy
         * ScHPF      0.0   -> off
         * Auto       0.0   -> off
         * Output     0.667 -> ~0 dB
         * Mix        1.0   -> 100%
         */
        thresholdDb_  = 0.75f * 40.0f - 40.0f;     // -10 dB
        ratio_        = 0.15f * 19.0f + 1.0f;       //  3.85:1
        attackMs_     = 0.1f  * 99.0f + 1.0f;       //  10.9 ms
        releaseMs_    = 0.3f  * 450.0f + 50.0f;     //  185 ms
        knee_         = 1.0f;                        //  full OverEasy
        scHpfNorm_    = 0.0f;                        //  off
        scHpfActive_  = false;
        autoRelease_  = 0.0f;                        //  off
        outputDb_     = 0.667f * 18.0f - 12.0f;     //  +0.006 dB (~0)
        mix_          = 1.0f;                        //  100%
    }

    /* ── Compute all time-domain coefficients ───────────────────── */

    void computeCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        auto msCoeff = [&](float ms) -> float {
            return 1.0f - std::exp(-1.0f / (ms * 0.001f * sr));
        };

        /* True RMS window length in samples (~10 ms) */
        rmsWindowLen_ = static_cast<int>(0.010f * sr);
        if (rmsWindowLen_ < 1) rmsWindowLen_ = 1;
        if (rmsWindowLen_ > kRmsWindowMax) rmsWindowLen_ = kRmsWindowMax;

        /* Compressor attack/release from user parameters */
        attackCoeff_  = msCoeff(attackMs_);
        releaseCoeff_ = msCoeff(releaseMs_);

        /* Auto-release: fast ~50 ms for transients, slow ~500 ms for sustain */
        fastReleaseCoeff_ = msCoeff(50.0f);
        slowReleaseCoeff_ = msCoeff(500.0f);

        /* Recompute sidechain HPF for new sample rate */
        computeScHPF();
    }

    /* ── RBJ Cookbook: 2nd-order Butterworth highpass ────────────── */

    void computeScHPF() {
        if (!scHpfActive_) return;
        float sr   = static_cast<float>(sampleRate_);
        float freq = mapScHpfFreq();

        float w0    = 2.0f * kPi * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q = sqrt(2)/2 */
        float cosw0 = std::cos(w0);
        float a0    = 1.0f + alpha;

        scHpf_.b0 =  (1.0f + cosw0) / 2.0f / a0;
        scHpf_.b1 = -(1.0f + cosw0) / a0;
        scHpf_.b2 =  (1.0f + cosw0) / 2.0f / a0;
        scHpf_.a1 = -2.0f * cosw0 / a0;
        scHpf_.a2 =  (1.0f - alpha) / a0;
    }
};

} // namespace mc1dsp
