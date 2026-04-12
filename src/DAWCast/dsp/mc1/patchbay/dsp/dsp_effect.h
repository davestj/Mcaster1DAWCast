/*
 * Mcaster1PatchBay — DSP Effects Engine
 * dsp/dsp_effect.h — Base class for all DSP effect plugins
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Every DSP effect inherits from DspEffect and implements:
 *   - process()    — real-time audio processing (lock-free, no alloc)
 *   - reset()      — clear internal state on discontinuity
 *   - name()       — human-readable effect name
 *   - id()         — unique identifier string
 *   - version()    — independent version per plugin
 *   - paramCount() — number of adjustable parameters
 *   - paramName()  — parameter name by index
 *   - paramValue() — get/set parameter by index (0.0–1.0 normalized)
 *
 * Effects are real-time safe: process() must NEVER allocate memory,
 * lock mutexes, or do I/O. Use atomic types for UI-thread metering.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <atomic>

namespace mc1dsp {

/* ── Effect categories for UI grouping ───────────────────────────── */
enum class EffectCategory {
    Dynamics,       // Compressor, gate, limiter, de-esser, AGC
    EQ,             // Parametric, graphic, shelving, filters
    Enhancer,       // BBE, exciter, saturation, stereo imager
    ChannelStrip,   // Combined multi-section processors (DBX 286S)
    Utility,        // Crossfader, PTT duck, phase rotator, gain
    Analyzer,       // FFT, spectrogram, LUFS meter (pass-through)
    Feedback,       // AFS2 adaptive notch filters
};

/* ── Base class ──────────────────────────────────────────────────── */

class DspEffect {
public:
    virtual ~DspEffect() = default;

    /* ── Identity ────────────────────────────────────────────────── */

    virtual const char* name() const = 0;       // "10-Band Parametric EQ"
    virtual const char* id() const = 0;         // "mc1.eq.parametric10"
    virtual const char* version() const = 0;    // "1.0.0"
    virtual EffectCategory category() const = 0;

    /* ── Audio processing ────────────────────────────────────────── */

    // Process interleaved PCM in-place. MUST be real-time safe.
    // frames = number of sample frames (not total samples).
    // channels = 1 (mono) or 2 (stereo interleaved).
    virtual void process(float* pcm, size_t frames, int channels) = 0;

    // Reset internal state (call on startup, seek, or discontinuity)
    virtual void reset() = 0;

    /* ── Configuration ───────────────────────────────────────────── */

    virtual void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }
    int sampleRate() const { return sampleRate_; }

    void setEnabled(bool on) { enabled_.store(on, std::memory_order_relaxed); }
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }

    void setBypassed(bool on) { bypassed_.store(on, std::memory_order_relaxed); }
    bool isBypassed() const { return bypassed_.load(std::memory_order_relaxed); }

    /* ── Parameters (normalized 0.0–1.0) ─────────────────────────── */

    virtual int paramCount() const { return 0; }
    virtual const char* paramName(int /*index*/) const { return ""; }
    virtual const char* paramUnit(int /*index*/) const { return ""; }
    virtual float paramValue(int /*index*/) const { return 0.0f; }
    virtual void setParamValue(int /*index*/, float /*value*/) {}

    // Display string for current value (e.g. "-12.5 dB", "4:1")
    virtual std::string paramDisplayValue(int index) const {
        return std::to_string(paramValue(index));
    }

    /* ── Metering (atomic, safe to read from UI thread) ──────────── */

    // Gain reduction in dB (for compressor/limiter/gate meters)
    float meterGainReduction() const {
        return meterGainReduction_.load(std::memory_order_relaxed);
    }

    // Input peak level in dBFS
    float meterInputPeak() const {
        return meterInputPeak_.load(std::memory_order_relaxed);
    }

    // Output peak level in dBFS
    float meterOutputPeak() const {
        return meterOutputPeak_.load(std::memory_order_relaxed);
    }

    /* ── Clipping detection (for EQ and dynamics) ───────────────── */

    bool isClipping() const {
        return clipping_.load(std::memory_order_relaxed);
    }
    int clipBand() const {
        return clipBand_.load(std::memory_order_relaxed);
    }
    void resetClip() {
        clipping_.store(false, std::memory_order_relaxed);
        clipBand_.store(-1, std::memory_order_relaxed);
    }

protected:
    int sampleRate_ = 48000;
    std::atomic<bool>  enabled_{false};
    std::atomic<bool>  bypassed_{false};
    std::atomic<float> meterGainReduction_{0.0f};
    std::atomic<float> meterInputPeak_{-96.0f};
    std::atomic<float> meterOutputPeak_{-96.0f};
    std::atomic<bool>  clipping_{false};
    std::atomic<int>   clipBand_{-1};
};

} // namespace mc1dsp
