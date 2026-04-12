/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_266xs.h — dbx 266xs Compressor / Gate (Auto-Dynamic)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Lighter sibling of the dbx 166xs with Auto-Dynamic mode.
 * Signal chain: RMS Envelope → OverEasy Compressor → Noise Gate → Output.
 * OverEasy knee switchable between Hard (0 dB) and Auto/Soft (12 dB).
 * Gate uses ratio-based attenuation with smooth attack/release envelope.
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx266xs : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* Parameter indices */
    enum ParamId {
        ParamThreshold = 0,   // 0..1 -> -40..0 dB
        ParamRatio,           // 0..1 -> 1:1..12:1
        ParamAttack,          // 0..1 -> 1..100 ms
        ParamRelease,         // 0..1 -> 50..500 ms
        ParamKnee,            // 0..1 -> hard(0)..auto/soft(1)
        ParamGateThresh,      // 0..1 -> -80..-20 dB
        ParamGateRatio,       // 0..1 -> 1:1..100:1
        ParamOutput,          // 0..1 -> -12..+6 dB
        ParamMix,             // 0..1 -> 0..100%
        kParamCount
    };

    FxDbx266xs() {
        setDefaults();
        computeCoeffs();
    }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "dbx 266xs Compressor/Gate"; }
    const char* id()       const override { return "mc1.dbx.266xs"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        const float outputGainLin = std::pow(10.0f, outputDb_ / 20.0f);
        const float mix    = mix_;
        const float dryAmt = 1.0f - mix;
        const float kneeWidthDb = (knee_ >= 0.5f) ? 12.0f : 0.0f;
        const float ratio  = ratio_;
        const float threshDb = thresholdDb_;
        const float gateThreshDb = gateThreshDb_;
        const float gateRatio    = gateRatio_;

        const int useCh = (channels < MAX_CH) ? channels : MAX_CH;

        for (size_t f = 0; f < frames; ++f) {
            /* ── RMS envelope detection across channels ──────────── */
            float peak = 0.0f;
            for (int ch = 0; ch < useCh; ++ch) {
                float absVal = std::fabs(pcm[f * channels + ch]);
                if (absVal > peak) peak = absVal;
            }

            /* One-pole RMS envelope smoother (~5 ms) */
            rmsEnv_ += rmsCoeff_ * (peak - rmsEnv_);
            float envLevel = rmsEnv_;

            float envDb = (envLevel > 1e-10f)
                        ? 20.0f * std::log10(envLevel) : -96.0f;

            /* Input metering */
            meterInputPeak_.store(envDb, std::memory_order_relaxed);

            /* ── OverEasy Compressor section ──────────────────────── */
            float compGrDb = 0.0f;

            if (kneeWidthDb < 0.01f) {
                /* Hard knee (Knee = 0 / hard mode) */
                if (envDb > threshDb) {
                    float over = envDb - threshDb;
                    compGrDb = over * (1.0f - 1.0f / ratio);
                }
            } else {
                /* OverEasy soft knee — quadratic interpolation in knee region */
                float halfKnee  = kneeWidthDb * 0.5f;
                float kneeBottom = threshDb - halfKnee;
                float kneeTop    = threshDb + halfKnee;

                if (envDb >= kneeTop) {
                    /* Above knee — full compression */
                    float over = envDb - threshDb;
                    compGrDb = over * (1.0f - 1.0f / ratio);
                } else if (envDb > kneeBottom) {
                    /* Inside knee region — quadratic interpolation */
                    float x = envDb - kneeBottom;
                    float range = kneeWidthDb;  /* = 2 * halfKnee */
                    compGrDb = (1.0f - 1.0f / ratio) * x * x / (2.0f * range);
                }
                /* Below kneeBottom: no compression */
            }

            float compTarget = std::pow(10.0f, -compGrDb / 20.0f);

            /* Compressor envelope follower — attack/release smoothing */
            float compCoeff = (compTarget < compEnv_)
                            ? compAttackCoeff_ : compReleaseCoeff_;
            compEnv_ += compCoeff * (compTarget - compEnv_);

            /* ── Noise Gate section ──────────────────────────────── */
            float gateGainLin = 1.0f;
            if (envDb < gateThreshDb) {
                /* Below gate threshold — attenuate by gate ratio */
                float belowDb = gateThreshDb - envDb;
                float attenDb = belowDb * (1.0f - 1.0f / gateRatio);
                /* Clamp to reasonable range */
                if (attenDb > 80.0f) attenDb = 80.0f;
                gateGainLin = std::pow(10.0f, -attenDb / 20.0f);
            }

            /* Gate envelope follower — fast attack, slow release */
            float gateCoeff = (gateGainLin < gateEnv_)
                            ? gateAttackCoeff_ : gateReleaseCoeff_;
            gateEnv_ += gateCoeff * (gateGainLin - gateEnv_);

            /* ── Apply to all channels ───────────────────────────── */
            float totalGainReduction = compGrDb;
            meterGainReduction_.store(totalGainReduction, std::memory_order_relaxed);

            for (int ch = 0; ch < useCh; ++ch) {
                float dry = pcm[f * channels + ch];
                float wet = dry * compEnv_ * gateEnv_ * outputGainLin;
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
            if (outPeak > 1.0f) {
                clipping_.store(true, std::memory_order_relaxed);
            }
        }
    }

    void reset() override {
        rmsEnv_  = 0.0f;
        compEnv_ = 1.0f;
        gateEnv_ = 1.0f;
        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
        clipping_.store(false);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        computeCoeffs();
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Threshold", "Ratio", "Attack", "Release",
            "Knee", "Gate Threshold", "Gate Ratio",
            "Output", "Mix"
        };
        return (index >= 0 && index < kParamCount) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dB", ":1", "ms", "ms",
            "", "dB", ":1",
            "dB", "%"
        };
        return (index >= 0 && index < kParamCount) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case ParamThreshold:  return (thresholdDb_ + 40.0f) / 40.0f;
            case ParamRatio:      return (ratio_ - 1.0f) / 11.0f;
            case ParamAttack:     return (attackMs_ - 1.0f) / 99.0f;
            case ParamRelease:    return (releaseMs_ - 50.0f) / 450.0f;
            case ParamKnee:       return knee_;
            case ParamGateThresh: return (gateThreshDb_ + 80.0f) / 60.0f;
            case ParamGateRatio:  return (gateRatio_ - 1.0f) / 99.0f;
            case ParamOutput:     return (outputDb_ + 12.0f) / 18.0f;
            case ParamMix:        return mix_;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        v = std::max(0.0f, std::min(1.0f, v));
        switch (index) {
            case ParamThreshold:  thresholdDb_  = v * 40.0f - 40.0f;    break;
            case ParamRatio:      ratio_        = v * 11.0f + 1.0f;     break;
            case ParamAttack:     attackMs_     = v * 99.0f + 1.0f;     computeCoeffs(); break;
            case ParamRelease:    releaseMs_    = v * 450.0f + 50.0f;   computeCoeffs(); break;
            case ParamKnee:       knee_         = v;                     break;
            case ParamGateThresh: gateThreshDb_ = v * 60.0f - 80.0f;    break;
            case ParamGateRatio:  gateRatio_    = v * 99.0f + 1.0f;     break;
            case ParamOutput:     outputDb_     = v * 18.0f - 12.0f;    break;
            case ParamMix:        mix_          = v;                     break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case ParamThreshold:
                snprintf(buf, 32, "%+.1f dB", thresholdDb_);
                break;
            case ParamRatio:
                snprintf(buf, 32, "%.1f:1", ratio_);
                break;
            case ParamAttack:
                snprintf(buf, 32, "%.1f ms", attackMs_);
                break;
            case ParamRelease:
                snprintf(buf, 32, "%.0f ms", releaseMs_);
                break;
            case ParamKnee:
                snprintf(buf, 32, "%s", (knee_ >= 0.5f) ? "Auto" : "Hard");
                break;
            case ParamGateThresh:
                snprintf(buf, 32, "%+.1f dB", gateThreshDb_);
                break;
            case ParamGateRatio:
                snprintf(buf, 32, "%.1f:1", gateRatio_);
                break;
            case ParamOutput:
                snprintf(buf, 32, "%+.1f dB", outputDb_);
                break;
            case ParamMix:
                snprintf(buf, 32, "%.0f%%", mix_ * 100.0f);
                break;
            default:
                buf[0] = 0;
        }
        return buf;
    }

private:
    /* ── Parameter storage ───────────────────────────────────────── */
    float thresholdDb_  = -10.0f;  // -40..0 dB
    float ratio_        =   3.75f; // 1:1..12:1
    float attackMs_     =  10.8f;  // 1..100 ms
    float releaseMs_    = 185.0f;  // 50..500 ms
    float knee_         =   1.0f;  // 0=hard, 1=auto/soft
    float gateThreshDb_ = -80.0f;  // -80..-20 dB
    float gateRatio_    =  50.5f;  // 1:1..100:1
    float outputDb_     =   0.0f;  // -12..+6 dB
    float mix_          =   1.0f;  // 0..1

    /* ── Envelope state ──────────────────────────────────────────── */
    float rmsEnv_  = 0.0f;   // RMS envelope follower
    float compEnv_ = 1.0f;   // compressor gain envelope (linear)
    float gateEnv_ = 1.0f;   // gate gain envelope (linear)

    /* ── Time-constant coefficients ──────────────────────────────── */
    float rmsCoeff_          = 0.0f;  // ~5 ms RMS smoothing
    float compAttackCoeff_   = 0.0f;
    float compReleaseCoeff_  = 0.0f;
    float gateAttackCoeff_   = 0.0f;  // 0.5 ms fixed
    float gateReleaseCoeff_  = 0.0f;  // 50 ms fixed

    void setDefaults() {
        /* Normalized defaults -> real values:
         * Threshold  0.75 -> -10 dB
         * Ratio      0.25 -> 3.75:1
         * Attack     0.1  -> ~10.9 ms
         * Release    0.3  -> ~185 ms
         * Knee       1.0  -> Auto/Soft
         * GateThresh 0.0  -> -80 dB (off)
         * GateRatio  0.5  -> 50.5:1
         * Output     0.667 -> ~0.006 dB (~0 dB)
         * Mix        1.0  -> 100%
         */
        thresholdDb_  = 0.75f * 40.0f - 40.0f;   // -10 dB
        ratio_        = 0.25f * 11.0f + 1.0f;     //  3.75:1
        attackMs_     = 0.1f  * 99.0f + 1.0f;     //  10.9 ms
        releaseMs_    = 0.3f  * 450.0f + 50.0f;   //  185 ms
        knee_         = 1.0f;                      //  Auto/Soft
        gateThreshDb_ = 0.0f  * 60.0f - 80.0f;   // -80 dB
        gateRatio_    = 0.5f  * 99.0f + 1.0f;     //  50.5:1
        outputDb_     = 0.667f * 18.0f - 12.0f;   //  +0.006 dB
        mix_          = 1.0f;                      //  100%
    }

    void computeCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        auto msCoeff = [&](float ms) -> float {
            return 1.0f - std::exp(-1.0f / (ms * 0.001f * sr));
        };

        /* ~5 ms RMS envelope smoothing */
        rmsCoeff_ = msCoeff(5.0f);

        /* Compressor attack/release from user parameters */
        compAttackCoeff_  = msCoeff(attackMs_);
        compReleaseCoeff_ = msCoeff(releaseMs_);

        /* Gate: fixed fast attack (0.5 ms), slow release (50 ms) */
        gateAttackCoeff_  = msCoeff(0.5f);
        gateReleaseCoeff_ = msCoeff(50.0f);
    }
};

} // namespace mc1dsp
