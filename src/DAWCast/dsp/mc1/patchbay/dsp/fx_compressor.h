/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_compressor.h — Compressor / Gate / Limiter (feedforward peak)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Feedforward peak compressor with soft knee, noise gate, and hard limiter.
 * Inspired by DBX 266xs dual-channel compressor/gate.
 *
 * Signal chain: Input Gain → Gate → Compressor (soft knee) → Makeup → Limiter
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxCompressor : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /* Parameter indices */
    enum Param {
        P_INPUT_GAIN = 0,   // -24 to +24 dB
        P_THRESHOLD,        // -60 to 0 dBFS
        P_RATIO,            // 1:1 to 20:1
        P_ATTACK,           // 0.1 to 100 ms
        P_RELEASE,          // 10 to 1000 ms
        P_KNEE,             // 0 to 12 dB
        P_MAKEUP,           // -12 to +24 dB
        P_GATE_THRESHOLD,   // -80 to 0 dBFS
        P_LIMITER_CEILING,  // -6 to 0 dBFS
        P_COUNT
    };

    FxCompressor() { updateCoeffs(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "Compressor / Gate / Limiter"; }
    const char* id()       const override { return "mc1.dynamics.compressor"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        float inputGainLin = std::pow(10.0f, inputGainDb_ / 20.0f);
        float makeupLin    = std::pow(10.0f, makeupDb_ / 20.0f);
        float limiterLin   = std::pow(10.0f, limiterDb_ / 20.0f);

        for (size_t f = 0; f < frames; ++f) {
            /* Find peak across channels */
            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                float s = std::fabs(pcm[f * channels + ch] * inputGainLin);
                if (s > peak) peak = s;
            }

            float peakDb = (peak > 1e-10f) ? 20.0f * std::log10(peak) : -96.0f;

            /* Gate */
            float gateGain = 1.0f;
            if (peakDb < gateThresholdDb_) {
                gateGain = 0.0f;  // hard gate
            }

            /* Compressor with soft knee */
            float gainReductionDb = 0.0f;
            if (peakDb > thresholdDb_) {
                float over = peakDb - thresholdDb_;
                gainReductionDb = over * (1.0f - 1.0f / ratio_);
            } else if (kneeDb_ > 0.0f && peakDb > thresholdDb_ - kneeDb_) {
                /* Soft knee region */
                float x = peakDb - thresholdDb_ + kneeDb_;
                gainReductionDb = (1.0f - 1.0f / ratio_) * x * x / (4.0f * kneeDb_);
            }

            float targetGain = std::pow(10.0f, -gainReductionDb / 20.0f);

            /* Envelope follower */
            float coeff = (targetGain < gainEnv_) ? attackCoeff_ : releaseCoeff_;
            gainEnv_ = gainEnv_ + coeff * (targetGain - gainEnv_);

            /* Apply */
            for (int ch = 0; ch < channels; ++ch) {
                float s = pcm[f * channels + ch] * inputGainLin;
                s *= gainEnv_ * gateGain * makeupLin;

                /* Hard limiter */
                if (s > limiterLin) s = limiterLin;
                else if (s < -limiterLin) s = -limiterLin;

                pcm[f * channels + ch] = s;
            }

            /* Metering (atomic for UI) */
            meterGainReduction_.store(gainReductionDb, std::memory_order_relaxed);
            meterInputPeak_.store(peakDb, std::memory_order_relaxed);
        }
    }

    void reset() override {
        gainEnv_ = 1.0f;
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
            "Input Gain", "Threshold", "Ratio", "Attack", "Release",
            "Knee", "Makeup Gain", "Gate Threshold", "Limiter Ceiling"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dB", "dBFS", ":1", "ms", "ms", "dB", "dB", "dBFS", "dBFS"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_INPUT_GAIN:     return (inputGainDb_ + 24.0f) / 48.0f;
            case P_THRESHOLD:      return (thresholdDb_ + 60.0f) / 60.0f;
            case P_RATIO:          return (ratio_ - 1.0f) / 19.0f;
            case P_ATTACK:         return (attackMs_ - 0.1f) / 99.9f;
            case P_RELEASE:        return (releaseMs_ - 10.0f) / 990.0f;
            case P_KNEE:           return kneeDb_ / 12.0f;
            case P_MAKEUP:         return (makeupDb_ + 12.0f) / 36.0f;
            case P_GATE_THRESHOLD: return (gateThresholdDb_ + 80.0f) / 80.0f;
            case P_LIMITER_CEILING:return (limiterDb_ + 6.0f) / 6.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        v = std::clamp(v, 0.0f, 1.0f);
        switch (index) {
            case P_INPUT_GAIN:     inputGainDb_ = v * 48.0f - 24.0f; break;
            case P_THRESHOLD:      thresholdDb_ = v * 60.0f - 60.0f; break;
            case P_RATIO:          ratio_ = v * 19.0f + 1.0f; break;
            case P_ATTACK:         attackMs_ = v * 99.9f + 0.1f; updateCoeffs(); break;
            case P_RELEASE:        releaseMs_ = v * 990.0f + 10.0f; updateCoeffs(); break;
            case P_KNEE:           kneeDb_ = v * 12.0f; break;
            case P_MAKEUP:         makeupDb_ = v * 36.0f - 12.0f; break;
            case P_GATE_THRESHOLD: gateThresholdDb_ = v * 80.0f - 80.0f; break;
            case P_LIMITER_CEILING:limiterDb_ = v * 6.0f - 6.0f; break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case P_INPUT_GAIN:     snprintf(buf, 32, "%+.1f dB", inputGainDb_); break;
            case P_THRESHOLD:      snprintf(buf, 32, "%.1f dBFS", thresholdDb_); break;
            case P_RATIO:          snprintf(buf, 32, "%.1f:1", ratio_); break;
            case P_ATTACK:         snprintf(buf, 32, "%.1f ms", attackMs_); break;
            case P_RELEASE:        snprintf(buf, 32, "%.0f ms", releaseMs_); break;
            case P_KNEE:           snprintf(buf, 32, "%.1f dB", kneeDb_); break;
            case P_MAKEUP:         snprintf(buf, 32, "%+.1f dB", makeupDb_); break;
            case P_GATE_THRESHOLD: snprintf(buf, 32, "%.1f dBFS", gateThresholdDb_); break;
            case P_LIMITER_CEILING:snprintf(buf, 32, "%.1f dBFS", limiterDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Parameters */
    float inputGainDb_     =   0.0f;
    float thresholdDb_     = -18.0f;
    float ratio_           =   4.0f;
    float attackMs_        =  10.0f;
    float releaseMs_       = 200.0f;
    float kneeDb_          =   6.0f;
    float makeupDb_        =   0.0f;
    float gateThresholdDb_ = -60.0f;
    float limiterDb_       =  -1.0f;

    /* State */
    float gainEnv_         = 1.0f;
    float attackCoeff_     = 0.0f;
    float releaseCoeff_    = 0.0f;

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);
        attackCoeff_  = 1.0f - std::exp(-1.0f / (attackMs_ * 0.001f * sr));
        releaseCoeff_ = 1.0f - std::exp(-1.0f / (releaseMs_ * 0.001f * sr));
    }
};

} // namespace mc1dsp
