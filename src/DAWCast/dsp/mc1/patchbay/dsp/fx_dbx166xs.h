/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx166xs.h — DBX 166xs Compressor / Gate (OverEasy)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Dual-section dynamics processor inspired by the DBX 166xs.
 * Signal chain: Gate/Expander → OverEasy Compressor → Output Gain
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbx166xs : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /* Parameter indices */
    enum Param {
        P_COMP_THRESHOLD = 0,   // -40 to +20 dBu
        P_COMP_RATIO,           // 1:1 to 20:1
        P_COMP_ATTACK,          // 1 to 200 ms
        P_COMP_RELEASE,         // 50 to 1200 ms
        P_COMP_OUTPUT_GAIN,     // -20 to +20 dB
        P_OVEREASY,             // 0 to 10 (0=hard knee)
        P_GATE_THRESHOLD,       // -60 to 0 dBFS
        P_GATE_RATIO,           // 1:1 to 10:1
        P_GATE_ATTACK,          // 0.1 to 25 ms
        P_GATE_HOLD,            // 5 to 2000 ms
        P_COUNT
    };

    FxDbx166xs() { updateCoeffs(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "DBX 166xs Comp/Gate"; }
    const char* id()       const override { return "mc1.dynamics.dbx166xs"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        float outputGainLin = std::pow(10.0f, compOutputGainDb_ / 20.0f);

        for (size_t f = 0; f < frames; ++f) {
            /* Find peak across channels */
            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                float s = std::fabs(pcm[f * channels + ch]);
                if (s > peak) peak = s;
            }

            float peakDb = (peak > 1e-10f) ? 20.0f * std::log10(peak) : -96.0f;

            /* Store input peak for metering */
            meterInputPeak_.store(peakDb, std::memory_order_relaxed);

            /* ── Gate / Expander section ──────────────────────────── */
            float gateGainDb = 0.0f;
            if (peakDb < gateThresholdDb_) {
                /* Below threshold: expand (attenuate) by gate ratio */
                float below = gateThresholdDb_ - peakDb;
                gateGainDb = below * (1.0f - 1.0f / gateRatio_);
            }

            float gateTarget = std::pow(10.0f, -gateGainDb / 20.0f);

            /* Gate envelope with hold timer */
            if (gateTarget >= 1.0f) {
                /* Signal above gate threshold — open gate immediately */
                gateHoldCounter_ = gateHoldSamples_;
                float coeff = gateAttackCoeff_;
                gateEnv_ += coeff * (1.0f - gateEnv_);
            } else if (gateHoldCounter_ > 0) {
                /* Signal below threshold but hold timer active — keep gate open */
                --gateHoldCounter_;
            } else {
                /* Hold expired — release toward target attenuation */
                float coeff = gateReleaseCoeff_;
                gateEnv_ += coeff * (gateTarget - gateEnv_);
            }

            /* ── OverEasy Compressor section ──────────────────────── */
            float compGainReductionDb = 0.0f;
            float kneeWidthDb = overEasy_ * 2.0f;  /* 0-10 → 0-20 dB knee */

            if (kneeWidthDb < 0.01f) {
                /* Hard knee (OverEasy = 0) */
                if (peakDb > compThresholdDb_) {
                    float over = peakDb - compThresholdDb_;
                    compGainReductionDb = over * (1.0f - 1.0f / compRatio_);
                }
            } else {
                /* OverEasy soft knee — quadratic interpolation in knee region */
                float kneeBottom = compThresholdDb_ - kneeWidthDb;
                float kneeTop    = compThresholdDb_ + kneeWidthDb;

                if (peakDb >= kneeTop) {
                    /* Above knee — full compression */
                    float over = peakDb - compThresholdDb_;
                    compGainReductionDb = over * (1.0f - 1.0f / compRatio_);
                } else if (peakDb > kneeBottom) {
                    /* Inside knee region — quadratic interpolation */
                    float x = peakDb - kneeBottom;
                    float range = 2.0f * kneeWidthDb;
                    compGainReductionDb = (1.0f - 1.0f / compRatio_) * x * x / (2.0f * range);
                }
                /* Below kneeBottom: no compression (GR stays 0) */
            }

            float compTarget = std::pow(10.0f, -compGainReductionDb / 20.0f);

            /* Compressor envelope follower */
            float compCoeff = (compTarget < compEnv_) ? compAttackCoeff_ : compReleaseCoeff_;
            compEnv_ += compCoeff * (compTarget - compEnv_);

            /* ── Apply both sections ──────────────────────────────── */
            for (int ch = 0; ch < channels; ++ch) {
                float s = pcm[f * channels + ch];
                s *= gateEnv_;            /* gate/expander */
                s *= compEnv_;            /* compressor */
                s *= outputGainLin;       /* output gain */
                pcm[f * channels + ch] = s;
            }

            /* Metering (compressor GR is the primary meter) */
            meterGainReduction_.store(compGainReductionDb, std::memory_order_relaxed);
        }
    }

    void reset() override {
        compEnv_ = 1.0f;
        gateEnv_ = 1.0f;
        gateHoldCounter_ = 0;
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
            "Comp Threshold", "Comp Ratio", "Comp Attack", "Comp Release",
            "Comp Output Gain", "OverEasy",
            "Gate Threshold", "Gate Ratio", "Gate Attack", "Gate Hold"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dBu", ":1", "ms", "ms", "dB", "", "dBFS", ":1", "ms", "ms"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_COMP_THRESHOLD:   return (compThresholdDb_ + 40.0f) / 60.0f;
            case P_COMP_RATIO:       return (compRatio_ - 1.0f) / 19.0f;
            case P_COMP_ATTACK:      return (compAttackMs_ - 1.0f) / 199.0f;
            case P_COMP_RELEASE:     return (compReleaseMs_ - 50.0f) / 1150.0f;
            case P_COMP_OUTPUT_GAIN: return (compOutputGainDb_ + 20.0f) / 40.0f;
            case P_OVEREASY:         return overEasy_ / 10.0f;
            case P_GATE_THRESHOLD:   return (gateThresholdDb_ + 60.0f) / 60.0f;
            case P_GATE_RATIO:       return (gateRatio_ - 1.0f) / 9.0f;
            case P_GATE_ATTACK:      return (gateAttackMs_ - 0.1f) / 24.9f;
            case P_GATE_HOLD:        return (gateHoldMs_ - 5.0f) / 1995.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        switch (index) {
            case P_COMP_THRESHOLD:   compThresholdDb_ = v * 60.0f - 40.0f; break;
            case P_COMP_RATIO:       compRatio_ = v * 19.0f + 1.0f; break;
            case P_COMP_ATTACK:      compAttackMs_ = v * 199.0f + 1.0f; updateCoeffs(); break;
            case P_COMP_RELEASE:     compReleaseMs_ = v * 1150.0f + 50.0f; updateCoeffs(); break;
            case P_COMP_OUTPUT_GAIN: compOutputGainDb_ = v * 40.0f - 20.0f; break;
            case P_OVEREASY:         overEasy_ = v * 10.0f; break;
            case P_GATE_THRESHOLD:   gateThresholdDb_ = v * 60.0f - 60.0f; break;
            case P_GATE_RATIO:       gateRatio_ = v * 9.0f + 1.0f; break;
            case P_GATE_ATTACK:      gateAttackMs_ = v * 24.9f + 0.1f; updateCoeffs(); break;
            case P_GATE_HOLD:        gateHoldMs_ = v * 1995.0f + 5.0f; updateCoeffs(); break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case P_COMP_THRESHOLD:   snprintf(buf, 32, "%+.1f dBu", compThresholdDb_); break;
            case P_COMP_RATIO:       snprintf(buf, 32, "%.1f:1", compRatio_); break;
            case P_COMP_ATTACK:      snprintf(buf, 32, "%.1f ms", compAttackMs_); break;
            case P_COMP_RELEASE:     snprintf(buf, 32, "%.0f ms", compReleaseMs_); break;
            case P_COMP_OUTPUT_GAIN: snprintf(buf, 32, "%+.1f dB", compOutputGainDb_); break;
            case P_OVEREASY:         snprintf(buf, 32, "%.0f", overEasy_); break;
            case P_GATE_THRESHOLD:   snprintf(buf, 32, "%.1f dBFS", gateThresholdDb_); break;
            case P_GATE_RATIO:       snprintf(buf, 32, "%.1f:1", gateRatio_); break;
            case P_GATE_ATTACK:      snprintf(buf, 32, "%.1f ms", gateAttackMs_); break;
            case P_GATE_HOLD:        snprintf(buf, 32, "%.0f ms", gateHoldMs_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Compressor parameters */
    float compThresholdDb_  = -10.0f;
    float compRatio_        =   4.0f;
    float compAttackMs_     =  15.0f;
    float compReleaseMs_    = 200.0f;
    float compOutputGainDb_ =   0.0f;
    float overEasy_         =   5.0f;   /* 0=hard knee, 10=max soft */

    /* Gate parameters */
    float gateThresholdDb_  = -40.0f;
    float gateRatio_        =   4.0f;
    float gateAttackMs_     =   1.0f;
    float gateHoldMs_       =  50.0f;

    /* Compressor state */
    float compEnv_          = 1.0f;
    float compAttackCoeff_  = 0.0f;
    float compReleaseCoeff_ = 0.0f;

    /* Gate state */
    float gateEnv_          = 1.0f;
    float gateAttackCoeff_  = 0.0f;
    float gateReleaseCoeff_ = 0.0f;
    int   gateHoldSamples_  = 0;
    int   gateHoldCounter_  = 0;

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        /* Compressor envelope coefficients */
        compAttackCoeff_  = 1.0f - std::exp(-1.0f / (compAttackMs_ * 0.001f * sr));
        compReleaseCoeff_ = 1.0f - std::exp(-1.0f / (compReleaseMs_ * 0.001f * sr));

        /* Gate envelope coefficients (release = 2x hold time, minimum 20ms) */
        gateAttackCoeff_  = 1.0f - std::exp(-1.0f / (gateAttackMs_ * 0.001f * sr));
        float gateReleaseMs = std::max(20.0f, gateHoldMs_ * 2.0f);
        gateReleaseCoeff_ = 1.0f - std::exp(-1.0f / (gateReleaseMs * 0.001f * sr));

        /* Hold time in samples */
        gateHoldSamples_ = static_cast<int>(gateHoldMs_ * 0.001f * sr);
    }
};

} // namespace mc1dsp
