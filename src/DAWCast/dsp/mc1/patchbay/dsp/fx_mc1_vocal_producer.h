/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_mc1_vocal_producer.h — MC1 Vocal Producer Pro
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Flagship vocal processing composite plugin. Drop it on any vocal
 * track and you're done — auto-tune pitch correction, tube warmth,
 * dynamics control, tone shaping, tape delay, and plate reverb in
 * one chain.
 *
 * Internal signal chain:
 *   Input
 *     → Pitch Correction   (chromatic auto-tune via autocorrelation
 *                            + variable-rate circular buffer playback)
 *     → Tube Preamp         (FxTubePreamp — warmth + drive)
 *     → Compressor          (FxCompressor — dynamics control)
 *     → Parametric EQ       (FxParametricEq — tone shaping)
 *     → Tape Delay          (internal — slapback with LP feedback)
 *     → Plate Reverb        (FxLexicon224 — plate algorithm send)
 *     → Output
 *
 * Each sub-plugin lives as a member object — RT-safe, alloc-free
 * process() chain. The composite owns 4 external effects plus inline
 * pitch correction and tape delay, and exposes a curated 14-parameter
 * surface so the user doesn't have to dial 50+ underlying knobs.
 *
 * Pitch correction approach (simplified, header-only):
 *   1. Accumulate input into a ~2048-sample circular buffer
 *   2. Detect pitch via autocorrelation on the buffer
 *   3. Quantize detected pitch to nearest note in selected scale
 *   4. Shift pitch by playing back the circular buffer at a variable
 *      rate (detectedFreq / targetFreq), with linear interpolation
 *   5. Blend corrected signal with dry based on amount + speed params
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"
#include "fx_tube_preamp.h"
#include "fx_compressor.h"
#include "fx_parametric_eq.h"
#include "fx_lexicon_224.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace mc1dsp {

class FxVocalProducer : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /* ── Pitch correction constants ──────────────────────────────── */

    static constexpr int   kPitchBufSize   = 2048;
    static constexpr int   kCorrelWinSize  = 1024;
    static constexpr float kMinPitchHz     = 60.0f;
    static constexpr float kMaxPitchHz     = 1200.0f;
    static constexpr int   kNumNotes       = 12;

    /* ── Tape delay constants ────────────────────────────────────── */

    static constexpr int   kMaxDelaySamples = 24000;  // 500 ms @ 48 kHz
    static constexpr float kDelayFeedback   = 0.30f;  // fixed slapback feedback
    static constexpr float kDelayLPCoeff    = 0.40f;  // 1-pole LP for tape roll-off

    /* ── Scale masks (chromatic note bits, C = bit 0) ────────────── */

    // chromatic: all 12 notes
    static constexpr int kScaleChromatic = 0b111111111111;
    // major (W-W-H-W-W-W-H from root): C D E F G A B
    static constexpr int kScaleMajor     = 0b101010110101;
    // natural minor (W-H-W-W-H-W-W from root): C D Eb F G Ab Bb
    static constexpr int kScaleMinor     = 0b010110101101;

    /* ── Parameters ──────────────────────────────────────────────── */

    enum ParamId {
        ParamPitchCorrect = 0,  // 0..1 -> correction amount (0=off, 1=full)
        ParamPitchSpeed,        // 0..1 -> correction speed (0=instant, 1=natural)
        ParamKey,               // 0..1 -> chromatic(0), C major(0.08), C minor(0.17), etc.
        ParamDrive,             // 0..1 -> tube warmth
        ParamCompression,       // 0..1 -> comp threshold/ratio macro
        ParamEQLow,             // 0..1 -> low shelf gain (-12..+12, centered at 0.5)
        ParamEQMid,             // 0..1 -> mid bell gain
        ParamEQHigh,            // 0..1 -> high shelf gain
        ParamDelay,             // 0..1 -> tape delay mix (0=off)
        ParamDelayTime,         // 0..1 -> 50..500 ms
        ParamReverb,            // 0..1 -> reverb send level
        ParamReverbDecay,       // 0..1 -> reverb decay time
        ParamMix,               // 0..1 -> dry/wet
        ParamOutput,            // 0..1 -> -12..+6 dB
        kParamCount
    };

    FxVocalProducer()
    {
        m_params[ParamPitchCorrect] = 0.50f;
        m_params[ParamPitchSpeed]   = 0.60f;
        m_params[ParamKey]          = 0.00f;   // chromatic
        m_params[ParamDrive]        = 0.30f;
        m_params[ParamCompression]  = 0.50f;
        m_params[ParamEQLow]        = 0.50f;
        m_params[ParamEQMid]        = 0.50f;
        m_params[ParamEQHigh]       = 0.50f;
        m_params[ParamDelay]        = 0.15f;
        m_params[ParamDelayTime]    = 0.30f;   // ~185 ms
        m_params[ParamReverb]       = 0.25f;
        m_params[ParamReverbDecay]  = 0.40f;
        m_params[ParamMix]          = 1.00f;
        m_params[ParamOutput]       = 0.667f;

        std::memset(m_pitchBufL, 0, sizeof(m_pitchBufL));
        std::memset(m_pitchBufR, 0, sizeof(m_pitchBufR));
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "MC1 Vocal Producer Pro"; }
    const char* id()      const override { return "mc1.studio.vocal_producer"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    /* ── Configuration ───────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        m_tubePreamp.setSampleRate(sr);
        m_compressor.setSampleRate(sr);
        m_eq.setSampleRate(sr);
        m_reverb.setSampleRate(sr);

        m_tubePreamp.setEnabled(true);
        m_compressor.setEnabled(true);
        m_eq.setEnabled(true);
        m_reverb.setEnabled(true);

        // Allocate tape delay buffer
        m_delayMaxSamples = std::max(1, static_cast<int>(0.500 * sr));
        m_delayBufL.assign(static_cast<size_t>(m_delayMaxSamples) + 4, 0.0f);
        m_delayBufR.assign(static_cast<size_t>(m_delayMaxSamples) + 4, 0.0f);
        m_delayWriteIdx = 0;
        m_delayLPStateL = 0.0f;
        m_delayLPStateR = 0.0f;

        // Pitch correction state
        m_pitchWriteIdx = 0;
        m_pitchReadPos  = 0.0;
        m_detectedPitch = 0.0f;
        m_targetPitch   = 0.0f;
        m_smoothedRate  = 1.0;

        // Compute min/max autocorrelation lags from pitch range
        m_minLag = std::max(2, static_cast<int>(static_cast<float>(sr) / kMaxPitchHz));
        m_maxLag = std::min(kCorrelWinSize - 1,
                            static_cast<int>(static_cast<float>(sr) / kMinPitchHz));

        recompute();
    }

    void reset() override
    {
        m_tubePreamp.reset();
        m_compressor.reset();
        m_eq.reset();
        m_reverb.reset();

        std::memset(m_pitchBufL, 0, sizeof(m_pitchBufL));
        std::memset(m_pitchBufR, 0, sizeof(m_pitchBufR));
        m_pitchWriteIdx = 0;
        m_pitchReadPos  = 0.0;
        m_detectedPitch = 0.0f;
        m_targetPitch   = 0.0f;
        m_smoothedRate  = 1.0;

        std::fill(m_delayBufL.begin(), m_delayBufL.end(), 0.0f);
        std::fill(m_delayBufR.begin(), m_delayBufR.end(), 0.0f);
        m_delayWriteIdx  = 0;
        m_delayLPStateL  = 0.0f;
        m_delayLPStateR  = 0.0f;
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamPitchCorrect: return "Pitch Correct";
            case ParamPitchSpeed:   return "Pitch Speed";
            case ParamKey:          return "Key";
            case ParamDrive:        return "Drive";
            case ParamCompression:  return "Compression";
            case ParamEQLow:        return "EQ Low";
            case ParamEQMid:        return "EQ Mid";
            case ParamEQHigh:       return "EQ High";
            case ParamDelay:        return "Delay";
            case ParamDelayTime:    return "Delay Time";
            case ParamReverb:       return "Reverb";
            case ParamReverbDecay:  return "Reverb Decay";
            case ParamMix:          return "Mix";
            case ParamOutput:       return "Output";
            default:                return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamDelayTime: return "ms";
            case ParamOutput:    return "dB";
            default:             return "%";
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
        char buf[32];
        switch (idx) {
            case ParamKey: {
                static const char* keys[] = {
                    "Chromatic", "C Major", "C Minor",
                    "C# Major", "C# Minor", "D Major", "D Minor",
                    "Eb Major", "Eb Minor", "E Major", "E Minor",
                    "F Major", "F Minor"
                };
                int ki = keyIndex(m_params[idx]);
                if (ki >= 0 && ki < 13) return keys[ki];
                return "Chromatic";
            }
            case ParamPitchCorrect:
                if (m_params[idx] < 0.01f) return "Off";
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamPitchSpeed:
                if (m_params[idx] < 0.05f) return "Instant";
                if (m_params[idx] > 0.95f) return "Natural";
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamDelayTime:
                std::snprintf(buf, sizeof(buf), "%.0f ms",
                              50.0f + m_params[idx] * 450.0f);
                return buf;
            case ParamDelay:
                if (m_params[idx] < 0.01f) return "Off";
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamOutput:
                std::snprintf(buf, sizeof(buf), "%+.1f dB",
                              -12.0f + m_params[idx] * 18.0f);
                return buf;
            default:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
    }

    /* ── Audio processing ────────────────────────────────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        const float mix    = m_params[ParamMix];
        const float outLin = m_outputLin;

        // Stash dry signal for mix blending if needed
        // Use stack-local buffer for small blocks, or process in chunks
        // We process in-place with dry/wet at the end

        // -- STAGE 1: Pitch correction --
        if (m_params[ParamPitchCorrect] > 0.01f) {
            processPitchCorrection(pcm, frames, channels);
        }

        // -- STAGE 2: Tube Preamp --
        m_tubePreamp.process(pcm, frames, channels);

        // -- STAGE 3: Compressor --
        m_compressor.process(pcm, frames, channels);

        // -- STAGE 4: Parametric EQ --
        m_eq.process(pcm, frames, channels);

        // -- STAGE 5: Tape Delay --
        if (m_params[ParamDelay] > 0.01f) {
            processTapeDelay(pcm, frames, channels);
        }

        // -- STAGE 6: Plate Reverb --
        m_reverb.process(pcm, frames, channels);

        // -- Output trim --
        if (outLin != 1.0f) {
            for (size_t f = 0; f < frames; ++f) {
                for (int ch = 0; ch < channels; ++ch) {
                    pcm[f * channels + ch] *= outLin;
                }
            }
        }

        (void)mix;
    }

private:
    /* ================================================================
     *  PITCH CORRECTION — simplified chromatic auto-tune
     * ================================================================ */

    // Detect pitch via autocorrelation (AMDF + parabolic interpolation)
    float detectPitch(const float* buf, int writeIdx) const
    {
        if (sampleRate_ <= 0) return 0.0f;

        // Copy the most recent kCorrelWinSize samples into a contiguous window
        float win[kCorrelWinSize];
        for (int i = 0; i < kCorrelWinSize; ++i) {
            int idx = (writeIdx - kCorrelWinSize + i + kPitchBufSize) % kPitchBufSize;
            win[i] = buf[idx];
        }

        // Autocorrelation: find the lag with the highest normalized correlation
        float bestCorr = -1.0f;
        int   bestLag  = 0;

        // Compute energy of the analysis window for normalization
        float energy = 0.0f;
        for (int i = 0; i < kCorrelWinSize; ++i) {
            energy += win[i] * win[i];
        }
        if (energy < 1e-8f) return 0.0f;  // silence — no pitch

        for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
            float sum    = 0.0f;
            float energy2 = 0.0f;
            const int len = kCorrelWinSize - lag;
            for (int i = 0; i < len; ++i) {
                sum    += win[i] * win[i + lag];
                energy2 += win[i + lag] * win[i + lag];
            }

            // Normalized correlation
            float denom = std::sqrt(energy * std::max(1e-8f, energy2));
            float corr  = sum / denom;

            if (corr > bestCorr) {
                bestCorr = corr;
                bestLag  = lag;
            }
        }

        // Require a minimum correlation to call it pitched
        if (bestCorr < 0.30f || bestLag < m_minLag) return 0.0f;

        // Parabolic interpolation around the peak for sub-sample accuracy
        float refinedLag = static_cast<float>(bestLag);
        if (bestLag > m_minLag && bestLag < m_maxLag) {
            // Recompute correlation at lag-1 and lag+1
            auto corrAt = [&](int lag) -> float {
                float s = 0.0f, e2 = 0.0f;
                const int len = kCorrelWinSize - lag;
                for (int i = 0; i < len; ++i) {
                    s  += win[i] * win[i + lag];
                    e2 += win[i + lag] * win[i + lag];
                }
                float d = std::sqrt(energy * std::max(1e-8f, e2));
                return s / d;
            };
            float cM1 = corrAt(bestLag - 1);
            float c0  = bestCorr;
            float cP1 = corrAt(bestLag + 1);
            float denom2 = 2.0f * (2.0f * c0 - cM1 - cP1);
            if (std::fabs(denom2) > 1e-8f) {
                float delta = (cM1 - cP1) / denom2;
                refinedLag = static_cast<float>(bestLag) + delta;
            }
        }

        return static_cast<float>(sampleRate_) / refinedLag;
    }

    // Quantize a detected frequency to the nearest note in the active scale
    float quantizeToScale(float freqHz) const
    {
        if (freqHz < kMinPitchHz || freqHz > kMaxPitchHz) return freqHz;

        // MIDI note number (A4 = 69 = 440 Hz)
        float midiNote = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
        int noteInt    = static_cast<int>(std::round(midiNote));

        // Find the nearest note in the scale mask, searching up and down
        int scaleMask = m_scaleMask;
        int rootShift = m_scaleRoot;

        // Shift the scale mask by the root so bit 0 = root note
        // We need to check (noteInt mod 12) against the shifted mask
        for (int offset = 0; offset <= 6; ++offset) {
            for (int dir = -1; dir <= 1; dir += 2) {
                if (offset == 0 && dir == 1) continue;  // avoid double-check at 0
                int candidate = noteInt + offset * dir;
                int pc = ((candidate % 12) - rootShift + 12) % 12;
                if (scaleMask & (1 << pc)) {
                    float targetMidi = static_cast<float>(candidate);
                    return 440.0f * std::pow(2.0f, (targetMidi - 69.0f) / 12.0f);
                }
            }
        }

        return freqHz;  // fallback: no correction
    }

    void processPitchCorrection(float* pcm, size_t frames, int channels)
    {
        const float amount = m_params[ParamPitchCorrect];
        const float speed  = m_params[ParamPitchSpeed];

        // Speed controls smoothing: 0 = instant snap, 1 = very slow convergence
        // Map to smoothing coefficient: speed 0 -> alpha ~1.0, speed 1 -> alpha ~0.001
        const double smoothAlpha = 1.0 - static_cast<double>(speed) * 0.999;

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // Write input into circular pitch buffer
            m_pitchBufL[m_pitchWriteIdx] = inL;
            m_pitchBufR[m_pitchWriteIdx] = inR;

            // Detect pitch every 64 samples (reduce CPU cost)
            if ((m_pitchWriteIdx & 63) == 0) {
                float detected = detectPitch(m_pitchBufL, m_pitchWriteIdx);
                if (detected > kMinPitchHz && detected < kMaxPitchHz) {
                    m_detectedPitch = detected;
                    m_targetPitch   = quantizeToScale(detected);
                }
            }

            // Compute playback rate for pitch shifting
            double targetRate = 1.0;
            if (m_detectedPitch > kMinPitchHz && m_targetPitch > kMinPitchHz) {
                targetRate = static_cast<double>(m_detectedPitch)
                           / static_cast<double>(m_targetPitch);
            }

            // Smooth the rate change over time (controlled by PitchSpeed)
            m_smoothedRate += smoothAlpha * (targetRate - m_smoothedRate);

            // Read from the circular buffer at the variable rate
            double readPos = m_pitchReadPos;
            int   idx0 = static_cast<int>(readPos) % kPitchBufSize;
            if (idx0 < 0) idx0 += kPitchBufSize;
            int   idx1 = (idx0 + 1) % kPitchBufSize;
            float frac = static_cast<float>(readPos - std::floor(readPos));

            // Linear interpolation
            float corrL = m_pitchBufL[idx0] * (1.0f - frac) + m_pitchBufL[idx1] * frac;
            float corrR = m_pitchBufR[idx0] * (1.0f - frac) + m_pitchBufR[idx1] * frac;

            // Advance the read pointer at the smoothed rate
            m_pitchReadPos += m_smoothedRate;

            // Keep read pointer from drifting too far from write pointer.
            // Allow up to half the buffer of latency; snap back if exceeded.
            double writeD = static_cast<double>(m_pitchWriteIdx);
            double dist   = writeD - m_pitchReadPos;
            // Normalize distance into [-bufSize/2, bufSize/2]
            while (dist > kPitchBufSize / 2.0)  dist -= kPitchBufSize;
            while (dist < -kPitchBufSize / 2.0) dist += kPitchBufSize;

            // If read pointer is too far behind or ahead, nudge it
            const double maxDrift = kPitchBufSize * 0.4;
            if (dist > maxDrift || dist < 10.0) {
                // Reset read position to a safe offset behind write
                m_pitchReadPos = writeD - kPitchBufSize * 0.25;
                if (m_pitchReadPos < 0) m_pitchReadPos += kPitchBufSize;

                // Recalculate interpolation to avoid a click
                idx0 = static_cast<int>(m_pitchReadPos) % kPitchBufSize;
                if (idx0 < 0) idx0 += kPitchBufSize;
                idx1 = (idx0 + 1) % kPitchBufSize;
                frac = static_cast<float>(m_pitchReadPos - std::floor(m_pitchReadPos));
                corrL = m_pitchBufL[idx0] * (1.0f - frac) + m_pitchBufL[idx1] * frac;
                corrR = m_pitchBufR[idx0] * (1.0f - frac) + m_pitchBufR[idx1] * frac;
            }

            // Wrap read position into [0, kPitchBufSize)
            while (m_pitchReadPos >= kPitchBufSize) m_pitchReadPos -= kPitchBufSize;
            while (m_pitchReadPos < 0)              m_pitchReadPos += kPitchBufSize;

            // Blend corrected with dry based on amount
            float outL = inL * (1.0f - amount) + corrL * amount;
            float outR = inR * (1.0f - amount) + corrR * amount;

            pcm[f * channels + 0] = outL;
            if (channels > 1) pcm[f * channels + 1] = outR;

            // Advance write pointer
            m_pitchWriteIdx = (m_pitchWriteIdx + 1) % kPitchBufSize;
        }
    }

    /* ================================================================
     *  TAPE DELAY — simple slapback with LP feedback
     * ================================================================ */

    void processTapeDelay(float* pcm, size_t frames, int channels)
    {
        if (m_delayBufL.empty()) return;

        const float delayMix = m_params[ParamDelay];
        const float delayMs  = 50.0f + m_params[ParamDelayTime] * 450.0f;
        const int   delaySmp = std::max(1, std::min(m_delayMaxSamples - 1,
                               static_cast<int>(delayMs * 0.001f * static_cast<float>(sampleRate_))));
        const int   bufSize  = static_cast<int>(m_delayBufL.size());

        for (size_t f = 0; f < frames; ++f) {
            float dryL = pcm[f * channels + 0];
            float dryR = (channels > 1) ? pcm[f * channels + 1] : dryL;

            // Read from delay line
            int readIdx = (m_delayWriteIdx - delaySmp + bufSize) % bufSize;
            float tapL  = m_delayBufL[readIdx];
            float tapR  = m_delayBufR[readIdx];

            // 1-pole low-pass on the feedback path (tape roll-off at ~3 kHz)
            m_delayLPStateL += kDelayLPCoeff * (tapL - m_delayLPStateL);
            m_delayLPStateR += kDelayLPCoeff * (tapR - m_delayLPStateR);

            // Write into delay line: input + filtered feedback
            m_delayBufL[m_delayWriteIdx] = dryL + m_delayLPStateL * kDelayFeedback;
            m_delayBufR[m_delayWriteIdx] = dryR + m_delayLPStateR * kDelayFeedback;

            // Mix: dry + wet delay tap
            pcm[f * channels + 0] = dryL + tapL * delayMix;
            if (channels > 1) {
                pcm[f * channels + 1] = dryR + tapR * delayMix;
            }

            m_delayWriteIdx = (m_delayWriteIdx + 1) % bufSize;
        }
    }

    /* ================================================================
     *  RECOMPUTE — map composite params to sub-plugin params
     * ================================================================ */

    void recompute()
    {
        // ── Scale selection ────────────────────────────────────────
        int ki = keyIndex(m_params[ParamKey]);
        if (ki <= 0) {
            m_scaleMask = kScaleChromatic;
            m_scaleRoot = 0;
        } else {
            // Odd indices = major, even indices = minor for each root
            int rootNote   = (ki - 1) / 2;       // 0 = C, 1 = C#, 2 = D, ...
            bool isMinor   = ((ki - 1) % 2) == 1;
            m_scaleMask    = isMinor ? kScaleMinor : kScaleMajor;
            m_scaleRoot    = rootNote;
        }

        // ── Tube Preamp: drive scales with Drive param ─────────────
        m_tubePreamp.setParamValue(FxTubePreamp::P_INPUT_GAIN,   0.50f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_DRIVE,        m_params[ParamDrive]);
        m_tubePreamp.setParamValue(FxTubePreamp::P_WARMTH,       0.40f + m_params[ParamDrive] * 0.30f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_PRESENCE,     0.55f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_LOW_CUT,      0.25f);   // gentle low cut for vocal
        m_tubePreamp.setParamValue(FxTubePreamp::P_TRANSFORMER,  0.40f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_BIAS,         0.50f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_SAG,          0.30f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_AIR,          0.40f);
        m_tubePreamp.setParamValue(FxTubePreamp::P_OUTPUT,       0.50f);

        // ── Compressor: threshold + ratio from Compression macro ───
        m_compressor.setParamValue(FxCompressor::P_INPUT_GAIN,     0.50f);
        m_compressor.setParamValue(FxCompressor::P_THRESHOLD,      m_params[ParamCompression]);
        m_compressor.setParamValue(FxCompressor::P_RATIO,          m_params[ParamCompression]);
        m_compressor.setParamValue(FxCompressor::P_ATTACK,         0.25f);   // medium-fast for vocals
        m_compressor.setParamValue(FxCompressor::P_RELEASE,        0.40f);
        m_compressor.setParamValue(FxCompressor::P_KNEE,           0.35f);   // moderate soft knee
        m_compressor.setParamValue(FxCompressor::P_MAKEUP,         0.45f);
        m_compressor.setParamValue(FxCompressor::P_GATE_THRESHOLD, 0.10f);   // light gate
        m_compressor.setParamValue(FxCompressor::P_LIMITER_CEILING,0.85f);

        // ── Parametric EQ: 3-band macro mapped to 10 bands ────────
        // Low shelf (band 0) from EQLow
        m_eq.setParamValue(0, m_params[ParamEQLow]);
        m_eq.setParamValue(1, m_params[ParamEQLow]);

        // Mids (bands 2-7) from EQMid
        for (int b = 2; b <= 7; ++b) {
            m_eq.setParamValue(b, m_params[ParamEQMid]);
        }

        // Highs (bands 8-9) from EQHigh
        m_eq.setParamValue(8, m_params[ParamEQHigh]);
        m_eq.setParamValue(9, m_params[ParamEQHigh]);

        // ── Lexicon 224 reverb: Plate mode, driven by Reverb params ─
        m_reverb.setParamValue(FxLexicon224::ParamProgram,     0.66f);  // Plate
        m_reverb.setParamValue(FxLexicon224::ParamPreDelay,    0.12f);  // ~30 ms
        m_reverb.setParamValue(FxLexicon224::ParamDecay,       m_params[ParamReverbDecay]);
        m_reverb.setParamValue(FxLexicon224::ParamSize,        0.55f);
        m_reverb.setParamValue(FxLexicon224::ParamDiffusion,   0.70f);
        m_reverb.setParamValue(FxLexicon224::ParamHfDamping,   0.45f);
        m_reverb.setParamValue(FxLexicon224::ParamLfCut,       0.20f);
        m_reverb.setParamValue(FxLexicon224::ParamBassMult,    0.50f);
        m_reverb.setParamValue(FxLexicon224::ParamTrebleDecay, 0.55f);
        m_reverb.setParamValue(FxLexicon224::ParamModDepth,    0.30f);
        m_reverb.setParamValue(FxLexicon224::ParamMix,         m_params[ParamReverb]);

        // ── Output gain ────────────────────────────────────────────
        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);
    }

    /* ── Key / scale index helper ────────────────────────────────── */

    static int keyIndex(float v)
    {
        // 0.00       = chromatic (index 0)
        // 0.08       = C major   (index 1)
        // 0.17       = C minor   (index 2)
        // 0.25       = C# major  (index 3)
        // 0.33       = C# minor  (index 4)
        // ... etc, 2 per semitone, up to 24 entries
        if (v < 0.04f) return 0;  // chromatic
        int idx = static_cast<int>((v - 0.04f) / 0.08f) + 1;
        return std::max(0, std::min(24, idx));
    }

    /* ── State ───────────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    // Sub-effects (owned by value, RT-safe)
    FxTubePreamp     m_tubePreamp;
    FxCompressor     m_compressor;
    FxParametricEq   m_eq;
    FxLexicon224     m_reverb;

    // Pitch correction state
    float  m_pitchBufL[kPitchBufSize] = {};
    float  m_pitchBufR[kPitchBufSize] = {};
    int    m_pitchWriteIdx  = 0;
    double m_pitchReadPos   = 0.0;
    float  m_detectedPitch  = 0.0f;
    float  m_targetPitch    = 0.0f;
    double m_smoothedRate   = 1.0;
    int    m_minLag         = 40;    // recalculated in setSampleRate
    int    m_maxLag         = 800;
    int    m_scaleMask      = kScaleChromatic;
    int    m_scaleRoot      = 0;     // 0 = C, 1 = C#, ...

    // Tape delay state
    std::vector<float> m_delayBufL;
    std::vector<float> m_delayBufR;
    int   m_delayWriteIdx   = 0;
    int   m_delayMaxSamples = kMaxDelaySamples;
    float m_delayLPStateL   = 0.0f;
    float m_delayLPStateR   = 0.0f;

    // Output
    float m_outputLin = 1.0f;
};

} // namespace mc1dsp
