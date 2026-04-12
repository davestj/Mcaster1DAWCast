/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_broadcast_agc.h — Broadcast-Grade AGC (Automatic Gain Control)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * RMS-based automatic gain control for consistent broadcast levels.
 * Maintains a target loudness by smoothly adjusting gain up or down.
 * Includes silence gate to prevent boosting noise floor.
 *
 * Typical use: last in chain before output, to ensure consistent
 * loudness across songs, voice segments, and transitions.
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace mc1dsp {

class FxBroadcastAgc : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /* Parameter indices */
    enum Param {
        P_TARGET_LEVEL = 0, // -20 to -6 dBFS
        P_MAX_GAIN,         // 0 to +30 dB
        P_MAX_REDUCTION,    // 0 to -30 dB
        P_ATTACK,           // 1 to 500 ms
        P_RELEASE,          // 50 to 5000 ms
        P_GATE_THRESHOLD,   // -80 to -20 dBFS
        P_RMS_WINDOW,       // 10 to 500 ms
        P_COUNT
    };

    FxBroadcastAgc() {
        updateCoeffs();
        rebuildRmsBuffer();
    }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "Broadcast AGC"; }
    const char* id()       const override { return "mc1.dynamics.agc"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Dynamics; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        float inputPeak = 0.0f;
        float outputPeak = 0.0f;

        for (size_t f = 0; f < frames; ++f) {
            /* Find peak across channels for input metering */
            float framePeak = 0.0f;
            float sumSq = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                float s = pcm[f * channels + ch];
                float absS = std::fabs(s);
                if (absS > framePeak) framePeak = absS;
                sumSq += s * s;
            }
            if (framePeak > inputPeak) inputPeak = framePeak;

            /* Average squared value across channels */
            float sqAvg = sumSq / static_cast<float>(channels);

            /* Update RMS circular buffer */
            rmsSum_ -= rmsBuf_[rmsPos_];
            rmsBuf_[rmsPos_] = sqAvg;
            rmsSum_ += sqAvg;
            rmsPos_ = (rmsPos_ + 1) % rmsBuf_.size();

            /* Compute RMS level */
            float rms = std::sqrt(rmsSum_ / static_cast<float>(rmsBuf_.size()));
            float rmsDb = (rms > 1e-10f) ? 20.0f * std::log10(rms) : -96.0f;

            /* Desired gain to reach target */
            float desiredGainDb = targetDb_ - rmsDb;

            /* Clamp gain to [maxReductionDb, maxGainDb] */
            if (desiredGainDb < maxReductionDb_) desiredGainDb = maxReductionDb_;
            else if (desiredGainDb > maxGainDb_) desiredGainDb = maxGainDb_;

            /* Silence gate: don't boost noise floor */
            if (rmsDb < gateThresholdDb_) {
                desiredGainDb = 0.0f;
            }

            /* Envelope follower: attack for reduction, release for boost */
            float coeff = (desiredGainDb < currentGainDb_) ? attackCoeff_ : releaseCoeff_;
            currentGainDb_ += coeff * (desiredGainDb - currentGainDb_);

            /* Apply gain as linear multiplier */
            float gainLin = std::pow(10.0f, currentGainDb_ / 20.0f);

            for (int ch = 0; ch < channels; ++ch) {
                float s = pcm[f * channels + ch] * gainLin;
                pcm[f * channels + ch] = s;
                float absS = std::fabs(s);
                if (absS > outputPeak) outputPeak = absS;
            }
        }

        /* Metering (atomic for UI thread) */
        meterGainReduction_.store(currentGainDb_, std::memory_order_relaxed);
        meterInputPeak_.store(
            (inputPeak > 1e-10f) ? 20.0f * std::log10(inputPeak) : -96.0f,
            std::memory_order_relaxed);
        meterOutputPeak_.store(
            (outputPeak > 1e-10f) ? 20.0f * std::log10(outputPeak) : -96.0f,
            std::memory_order_relaxed);
    }

    void reset() override {
        currentGainDb_ = 0.0f;
        rmsSum_ = 0.0f;
        rmsPos_ = 0;
        std::fill(rmsBuf_.begin(), rmsBuf_.end(), 0.0f);
        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        updateCoeffs();
        rebuildRmsBuffer();
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Target Level", "Max Gain", "Max Reduction",
            "Attack", "Release", "Gate Threshold", "RMS Window"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dBFS", "dB", "dB", "ms", "ms", "dBFS", "ms"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_TARGET_LEVEL:   return (targetDb_ + 20.0f) / 14.0f;
            case P_MAX_GAIN:       return maxGainDb_ / 30.0f;
            case P_MAX_REDUCTION:  return -maxReductionDb_ / 30.0f;
            case P_ATTACK:         return (attackMs_ - 1.0f) / 499.0f;
            case P_RELEASE:        return (releaseMs_ - 50.0f) / 4950.0f;
            case P_GATE_THRESHOLD: return (gateThresholdDb_ + 80.0f) / 60.0f;
            case P_RMS_WINDOW:     return (rmsWindowMs_ - 10.0f) / 490.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        switch (index) {
            case P_TARGET_LEVEL:   targetDb_ = v * 14.0f - 20.0f; break;
            case P_MAX_GAIN:       maxGainDb_ = v * 30.0f; break;
            case P_MAX_REDUCTION:  maxReductionDb_ = -(v * 30.0f); break;
            case P_ATTACK:         attackMs_ = v * 499.0f + 1.0f; updateCoeffs(); break;
            case P_RELEASE:        releaseMs_ = v * 4950.0f + 50.0f; updateCoeffs(); break;
            case P_GATE_THRESHOLD: gateThresholdDb_ = v * 60.0f - 80.0f; break;
            case P_RMS_WINDOW:     rmsWindowMs_ = v * 490.0f + 10.0f; rebuildRmsBuffer(); break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case P_TARGET_LEVEL:   snprintf(buf, 32, "%.1f dBFS", targetDb_); break;
            case P_MAX_GAIN:       snprintf(buf, 32, "+%.1f dB", maxGainDb_); break;
            case P_MAX_REDUCTION:  snprintf(buf, 32, "%.1f dB", maxReductionDb_); break;
            case P_ATTACK:         snprintf(buf, 32, "%.0f ms", attackMs_); break;
            case P_RELEASE:        snprintf(buf, 32, "%.0f ms", releaseMs_); break;
            case P_GATE_THRESHOLD: snprintf(buf, 32, "%.1f dBFS", gateThresholdDb_); break;
            case P_RMS_WINDOW:     snprintf(buf, 32, "%.0f ms", rmsWindowMs_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* Parameters */
    float targetDb_         = -14.0f;
    float maxGainDb_        =  20.0f;
    float maxReductionDb_   = -20.0f;
    float attackMs_         =  50.0f;
    float releaseMs_        = 500.0f;
    float gateThresholdDb_  = -50.0f;
    float rmsWindowMs_      = 100.0f;

    /* Envelope state */
    float currentGainDb_    = 0.0f;
    float attackCoeff_      = 0.0f;
    float releaseCoeff_     = 0.0f;

    /* RMS circular buffer (allocated once, not in hot path) */
    std::vector<float> rmsBuf_;
    size_t rmsPos_          = 0;
    float  rmsSum_          = 0.0f;

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);
        attackCoeff_  = 1.0f - std::exp(-1.0f / (attackMs_ * 0.001f * sr));
        releaseCoeff_ = 1.0f - std::exp(-1.0f / (releaseMs_ * 0.001f * sr));
    }

    void rebuildRmsBuffer() {
        size_t newSize = static_cast<size_t>(rmsWindowMs_ * 0.001f *
                         static_cast<float>(sampleRate_));
        if (newSize < 1) newSize = 1;
        rmsBuf_.assign(newSize, 0.0f);
        rmsPos_ = 0;
        rmsSum_ = 0.0f;
    }
};

} // namespace mc1dsp
