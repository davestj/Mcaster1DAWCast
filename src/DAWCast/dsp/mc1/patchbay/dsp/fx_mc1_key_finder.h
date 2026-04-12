/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_key_finder.h — MC1 Topline Key Finder
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Real-time musical key and scale detection analyzer using the
 * Krumhansl-Schmuckler key-finding algorithm.
 *
 *   - Goertzel-based chromagram extraction (12 pitch classes, octaves 2-5)
 *   - Pearson correlation against 24 Krumhansl-Kessler key profiles
 *   - Exponential smoothing with stability gate (500 ms hold)
 *   - Pure analyzer: audio passes through unmodified (optional gain trim)
 *
 * Real-time safe. No allocations in process(), lock-free atomics for UI.
 */

#pragma once

#include "dsp_effect.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <atomic>

namespace mc1dsp {

class FxKeyFinder : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /* ── Parameters ─────────────────────────────────────────────── */

    enum ParamId {
        ParamSensitivity = 0,   // 0..1 → analysis sensitivity/threshold
        ParamSmoothing,         // 0..1 → temporal smoothing (0=instant, 1=very stable)
        ParamConcertPitch,      // 0..1 → 430..450 Hz
        ParamOutput,            // 0..1 → -12..+6 dB (pass-through gain)
        kParamCount
    };

    /* ── Analysis buffer size ───────────────────────────────────── */

    static constexpr int kAnalysisBufSize = 4096;
    static constexpr int kChromaBins      = 12;
    static constexpr int kNumKeys         = 24;   // 12 major + 12 minor
    static constexpr int kOctaveLo        = 2;
    static constexpr int kOctaveHi        = 5;    // inclusive → C2..C5

    /* ── Krumhansl-Kessler key profiles ─────────────────────────── */

    static constexpr float kMajorProfile[kChromaBins] = {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    static constexpr float kMinorProfile[kChromaBins] = {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    /* ── Pitch class names (for display helpers) ────────────────── */

    static constexpr const char* kNoteNames[kChromaBins] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };

    /* ── Constructor ────────────────────────────────────────────── */

    FxKeyFinder()
    {
        m_params[ParamSensitivity] = 0.6f;
        m_params[ParamSmoothing]   = 0.5f;
        m_params[ParamConcertPitch]= 0.5f;   // 440 Hz
        m_params[ParamOutput]      = 0.667f;  // 0 dB

        std::memset(m_analysisBuf, 0, sizeof(m_analysisBuf));
        std::memset(m_chromaBins,  0, sizeof(m_chromaBins));
        std::memset(m_smoothChroma,0, sizeof(m_smoothChroma));
        std::memset(m_hannWindow,  0, sizeof(m_hannWindow));

        m_keyIndex.store(-1, std::memory_order_relaxed);
        m_isMinor.store(false, std::memory_order_relaxed);
        m_confidence.store(0.0f, std::memory_order_relaxed);

        buildHannWindow();
    }

    /* ── Identity ───────────────────────────────────────────────── */

    const char* name()    const override { return "MC1 Topline Key Finder"; }
    const char* id()      const override { return "mc1.analyzer.key_finder"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    /* ── Configuration ──────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;
        m_analysisBufIdx = 0;
        std::memset(m_analysisBuf, 0, sizeof(m_analysisBuf));
        std::memset(m_chromaBins,  0, sizeof(m_chromaBins));
        std::memset(m_smoothChroma,0, sizeof(m_smoothChroma));
        m_stableKeyIndex   = -1;
        m_candidateKey     = -1;
        m_candidateMinor   = false;
        m_candidateHoldSmp = 0;
        recompute();
    }

    void reset() override
    {
        m_analysisBufIdx = 0;
        std::memset(m_analysisBuf, 0, sizeof(m_analysisBuf));
        std::memset(m_chromaBins,  0, sizeof(m_chromaBins));
        std::memset(m_smoothChroma,0, sizeof(m_smoothChroma));
        m_keyIndex.store(-1, std::memory_order_relaxed);
        m_isMinor.store(false, std::memory_order_relaxed);
        m_confidence.store(0.0f, std::memory_order_relaxed);
        m_stableKeyIndex   = -1;
        m_candidateKey     = -1;
        m_candidateMinor   = false;
        m_candidateHoldSmp = 0;
    }

    /* ── Parameter interface ────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamSensitivity:  return "Sensitivity";
            case ParamSmoothing:    return "Smoothing";
            case ParamConcertPitch: return "Concert Pitch";
            case ParamOutput:       return "Output";
            default:                return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamConcertPitch: return "Hz";
            case ParamOutput:       return "dB";
            default:                return "";
        }
    }

    float paramValue(int idx) const override
    {
        return (idx >= 0 && idx < kParamCount) ? m_params[idx] : 0.0f;
    }

    void setParamValue(int idx, float v) override
    {
        if (idx < 0 || idx >= kParamCount) return;
        m_params[idx] = std::max(0.0f, std::min(1.0f, v));
        recompute();
    }

    std::string paramDisplayValue(int idx) const override
    {
        char buf[48];
        switch (idx) {
            case ParamSensitivity:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamSmoothing:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamConcertPitch: {
                float hz = 430.0f + m_params[idx] * 20.0f;
                std::snprintf(buf, sizeof(buf), "%.1f Hz", hz);
                return buf;
            }
            case ParamOutput: {
                float dB = -12.0f + m_params[idx] * 18.0f;
                std::snprintf(buf, sizeof(buf), "%+.1f dB", dB);
                return buf;
            }
        }
        return "";
    }

    /* ── UI accessors (thread-safe, polled from GUI) ────────────── */

    /// Detected key index: 0=C, 1=C#, 2=D, ..., 11=B.  -1 if none.
    int detectedKeyIndex() const
    {
        return m_keyIndex.load(std::memory_order_relaxed);
    }

    /// True if the detected key is minor, false if major.
    bool detectedIsMinor() const
    {
        return m_isMinor.load(std::memory_order_relaxed);
    }

    /// Confidence of detection, 0..1.
    float detectedConfidence() const
    {
        return m_confidence.load(std::memory_order_relaxed);
    }

    /// Pointer to 12 normalized chroma bins (C..B) for visualization.
    /// Values are valid between process() calls; read from UI timer.
    const float* chromagram() const
    {
        return m_smoothChroma;
    }

    /* ── Audio processing (pass-through analyzer) ───────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float outputGain = m_outputLin;
        const float sensitivity = m_params[ParamSensitivity];
        const float smoothCoef  = m_smoothCoef;
        const float sr = static_cast<float>(sampleRate_);

        for (size_t f = 0; f < frames; ++f) {

            /* ── Accumulate mono into analysis buffer ───────────── */
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            float mono = 0.5f * (inL + inR);

            m_analysisBuf[m_analysisBufIdx] = mono;
            ++m_analysisBufIdx;

            /* ── When buffer is full, run key analysis ──────────── */
            if (m_analysisBufIdx >= kAnalysisBufSize) {
                m_analysisBufIdx = 0;
                analyzeKey(sr, sensitivity, smoothCoef);
            }

            /* ── Pass-through with output gain ─────────────────── */
            pcm[f * channels + 0] = inL * outputGain;
            if (channels > 1)
                pcm[f * channels + 1] = inR * outputGain;
        }
    }

private:

    /* ── Goertzel single-frequency power estimator ──────────────── */

    static float goertzel(const float* buf, int N, float targetFreq, float sr)
    {
        float k = 0.5f + (static_cast<float>(N) * targetFreq / sr);
        float w = (2.0f * 3.14159265358979f / static_cast<float>(N)) * k;
        float coeff = 2.0f * std::cos(w);
        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < N; ++i) {
            s0 = buf[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return s0 * s0 + s1 * s1 - coeff * s0 * s1;   // power
    }

    /* ── Build Hann window (called once in ctor) ────────────────── */

    void buildHannWindow()
    {
        for (int i = 0; i < kAnalysisBufSize; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(kAnalysisBufSize - 1);
            m_hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979f * t));
        }
    }

    /* ── Chromagram + key correlation ───────────────────────────── */

    void analyzeKey(float sr, float sensitivity, float smoothCoef)
    {
        /*  Concert pitch (A4) from parameter  */
        const float concertA = 430.0f + m_params[ParamConcertPitch] * 20.0f;

        /*  Base frequencies for each pitch class at octave 0.
         *  noteFreq(class, octave) = baseFreq[class] * 2^octave
         *  Using A4 = concertA → C0 = concertA * 2^(-4) / 2^(9/12)
         *  Simpler: freq(midi) = concertA * 2^((midi-69)/12)
         *  C2 = midi 36, C#2 = midi 37 ... B2 = midi 47              */

        /* ── Apply Hann window to a scratch copy ───────────────── */
        float windowed[kAnalysisBufSize];
        for (int i = 0; i < kAnalysisBufSize; ++i)
            windowed[i] = m_analysisBuf[i] * m_hannWindow[i];

        /* ── Compute chroma energy via Goertzel ────────────────── */
        float rawChroma[kChromaBins];
        std::memset(rawChroma, 0, sizeof(rawChroma));

        for (int pc = 0; pc < kChromaBins; ++pc) {
            float energy = 0.0f;
            for (int oct = kOctaveLo; oct <= kOctaveHi; ++oct) {
                // MIDI note for pitch class pc at octave oct
                int midi = (oct + 1) * 12 + pc;    // C2=36 when oct=2, pc=0
                float freq = concertA * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
                if (freq < 20.0f || freq > sr * 0.45f) continue;
                energy += goertzel(windowed, kAnalysisBufSize, freq, sr);
            }
            rawChroma[pc] = energy;
        }

        /* ── Normalize raw chroma to sum=1 ─────────────────────── */
        float chromaSum = 0.0f;
        for (int i = 0; i < kChromaBins; ++i)
            chromaSum += rawChroma[i];

        if (chromaSum < 1e-12f) {
            // Silence — no update, fade confidence
            float curConf = m_confidence.load(std::memory_order_relaxed);
            m_confidence.store(curConf * 0.9f, std::memory_order_relaxed);
            return;
        }

        float invSum = 1.0f / chromaSum;
        for (int i = 0; i < kChromaBins; ++i)
            rawChroma[i] *= invSum;

        /* ── Smooth chroma bins (EMA) ──────────────────────────── */
        for (int i = 0; i < kChromaBins; ++i)
            m_smoothChroma[i] += (rawChroma[i] - m_smoothChroma[i]) * smoothCoef;

        /* ── Sensitivity gate: skip if max chroma bin is below threshold */
        float maxBin = 0.0f;
        for (int i = 0; i < kChromaBins; ++i)
            if (m_smoothChroma[i] > maxBin) maxBin = m_smoothChroma[i];

        float threshold = (1.0f - sensitivity) * 0.2f;   // sens=1 → thr=0, sens=0 → thr=0.2
        if (maxBin < threshold) {
            float curConf = m_confidence.load(std::memory_order_relaxed);
            m_confidence.store(curConf * 0.9f, std::memory_order_relaxed);
            return;
        }

        /* ── Correlate against 24 key profiles ─────────────────── */
        float bestCorr = -2.0f;
        int   bestKey  = 0;
        bool  bestMinor = false;

        for (int k = 0; k < kChromaBins; ++k) {
            float corrMaj = pearson(m_smoothChroma, kMajorProfile, k);
            float corrMin = pearson(m_smoothChroma, kMinorProfile, k);

            if (corrMaj > bestCorr) {
                bestCorr  = corrMaj;
                bestKey   = k;
                bestMinor = false;
            }
            if (corrMin > bestCorr) {
                bestCorr  = corrMin;
                bestKey   = k;
                bestMinor = true;
            }
        }

        /* ── Confidence = best correlation mapped to 0..1 ──────── */
        float conf = std::max(0.0f, std::min(1.0f, (bestCorr + 1.0f) * 0.5f));

        /* ── Stability gate: require same key for ~500 ms ──────── */
        int holdThreshold = static_cast<int>(0.5f * sr);  // 500 ms in samples

        if (conf > 0.5f) {
            if (bestKey == m_candidateKey && bestMinor == m_candidateMinor) {
                m_candidateHoldSmp += kAnalysisBufSize;
            } else {
                // New candidate — reset hold timer
                m_candidateKey     = bestKey;
                m_candidateMinor   = bestMinor;
                m_candidateHoldSmp = 0;
            }

            if (m_candidateHoldSmp >= holdThreshold) {
                m_stableKeyIndex = bestKey;
                m_keyIndex.store(bestKey, std::memory_order_relaxed);
                m_isMinor.store(bestMinor, std::memory_order_relaxed);
                m_confidence.store(conf, std::memory_order_relaxed);
            } else {
                // Still holding — keep previous stable key, update confidence
                m_confidence.store(conf * 0.7f, std::memory_order_relaxed);
            }
        } else {
            // Low confidence — decay toward "no key"
            m_candidateHoldSmp = 0;
            float curConf = m_confidence.load(std::memory_order_relaxed);
            float decayed = curConf * 0.85f;
            m_confidence.store(decayed, std::memory_order_relaxed);
            if (decayed < 0.05f) {
                m_keyIndex.store(-1, std::memory_order_relaxed);
                m_stableKeyIndex = -1;
            }
        }
    }

    /* ── Pearson correlation with rotated profile ───────────────── */

    static float pearson(const float* chroma, const float* profile, int rotation)
    {
        /*  Compute Pearson r between chroma[0..11] and profile rotated
         *  by 'rotation' semitones. Rotation means: for key=rotation,
         *  the tonic is at chroma[rotation], so we align profile[0]
         *  with chroma[rotation].                                      */

        float meanX = 0.0f, meanY = 0.0f;
        for (int i = 0; i < kChromaBins; ++i) {
            meanX += chroma[i];
            meanY += profile[i];
        }
        meanX /= static_cast<float>(kChromaBins);
        meanY /= static_cast<float>(kChromaBins);

        float num = 0.0f, denX = 0.0f, denY = 0.0f;
        for (int i = 0; i < kChromaBins; ++i) {
            int ci = (i + rotation) % kChromaBins;
            float dx = chroma[ci] - meanX;
            float dy = profile[i] - meanY;
            num  += dx * dy;
            denX += dx * dx;
            denY += dy * dy;
        }

        float den = std::sqrt(denX * denY);
        if (den < 1e-12f) return 0.0f;
        return num / den;
    }

    /* ── Recompute derived coefficients ─────────────────────────── */

    void recompute()
    {
        if (sampleRate_ <= 0) return;

        // Output gain: -12..+6 dB
        float dB = -12.0f + m_params[ParamOutput] * 18.0f;
        m_outputLin = std::pow(10.0f, dB / 20.0f);

        // Smoothing coefficient for chroma EMA.
        // ParamSmoothing 0 → fast (coef ~0.8), 1 → very stable (coef ~0.02)
        float rawSmooth = m_params[ParamSmoothing];
        m_smoothCoef = 0.8f * std::pow(0.025f, rawSmooth);  // exp decay from 0.8 to 0.02
    }

    /* ── Member data ────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    // Analysis ring buffer (non-overlapping blocks of kAnalysisBufSize)
    float m_analysisBuf[kAnalysisBufSize] = {};
    int   m_analysisBufIdx = 0;

    // Hann window (precomputed)
    float m_hannWindow[kAnalysisBufSize] = {};

    // Chromagram (raw from latest analysis frame)
    float m_chromaBins[kChromaBins] = {};

    // Smoothed chromagram (EMA across analysis frames, also read by UI)
    float m_smoothChroma[kChromaBins] = {};

    // Derived coefficients
    float m_outputLin  = 1.0f;
    float m_smoothCoef = 0.1f;

    // Stability tracking (audio thread only)
    int  m_stableKeyIndex   = -1;
    int  m_candidateKey     = -1;
    bool m_candidateMinor   = false;
    int  m_candidateHoldSmp = 0;

    // Atomic UI-readable results
    std::atomic<int>   m_keyIndex{-1};
    std::atomic<bool>  m_isMinor{false};
    std::atomic<float> m_confidence{0.0f};
};

} // namespace mc1dsp
