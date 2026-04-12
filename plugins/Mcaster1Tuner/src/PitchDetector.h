/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/PitchDetector.h — YIN pitch detection algorithm
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MCASTER1_PITCH_DETECTOR_H
#define MCASTER1_PITCH_DETECTOR_H

#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

/**
 * Header-only YIN pitch detection algorithm.
 *
 * Based on: de Cheveigne, A. & Kawahara, H. (2002).
 * "YIN, a fundamental frequency estimator for speech and music."
 * Journal of the Acoustical Society of America, 111(4), 1917-1930.
 *
 * Real-time safe: no allocations after construction.
 * Thread-safe for single-producer (audio thread) / single-consumer (UI thread).
 */
class PitchDetector {
public:
    // ---------------------------------------------------------------
    //  Construction
    // ---------------------------------------------------------------

    explicit PitchDetector(int bufferSize = 2048, float threshold = 0.15f)
        : m_bufferSize(bufferSize)
        , m_halfBuffer(bufferSize / 2)
        , m_threshold(threshold)
        , m_sampleRate(44100)
        , m_writePos(0)
        , m_sampleBuffer(static_cast<size_t>(bufferSize), 0.0f)
        , m_diffBuffer(static_cast<size_t>(bufferSize / 2), 0.0f)
        , m_cmndfBuffer(static_cast<size_t>(bufferSize / 2), 0.0f)
    {
        m_detectedPitch.store(0.0f, std::memory_order_relaxed);
        m_confidence.store(0.0f, std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------
    //  Configuration
    // ---------------------------------------------------------------

    void setSampleRate(int sr) noexcept
    {
        m_sampleRate = sr;
    }

    // ---------------------------------------------------------------
    //  Audio-thread entry point
    // ---------------------------------------------------------------

    /**
     * Feed samples from the audio callback.
     * Accumulates into an internal circular buffer.  When a full analysis
     * window is available the YIN algorithm runs and the results are
     * published via atomics so the UI thread can read them lock-free.
     */
    void process(const float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i) {
            m_sampleBuffer[static_cast<size_t>(m_writePos)] = samples[i];
            ++m_writePos;

            if (m_writePos >= m_bufferSize) {
                runYin();
                m_writePos = 0;
            }
        }
    }

    // ---------------------------------------------------------------
    //  UI-thread result accessors
    // ---------------------------------------------------------------

    /** Detected fundamental frequency in Hz (0 if unvoiced / no pitch). */
    float detectedPitch() const noexcept
    {
        return m_detectedPitch.load(std::memory_order_relaxed);
    }

    /** Confidence of the detection, range 0..1 (1 = very strong periodicity). */
    float confidence() const noexcept
    {
        return m_confidence.load(std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------
    //  Static note-name helper
    // ---------------------------------------------------------------

    struct NoteInfo {
        const char* noteName;   // "C", "C#", "D", etc.
        int         octave;     // MIDI octave convention (-1 for note 0)
        float       centDeviation; // -50..+50 cents from nearest semitone
        float       frequency;  // the input Hz
    };

    /**
     * Pure function: map a frequency to the nearest 12-TET note.
     *
     * @param hz           Input frequency (must be > 0).
     * @param concertPitch Reference A4 frequency (default 440 Hz).
     */
    static NoteInfo nearestNote(float hz, float concertPitch = 440.0f) noexcept
    {
        static const char* noteNames[12] = {
            "C", "C#", "D", "D#", "E", "F",
            "F#", "G", "G#", "A", "A#", "B"
        };

        NoteInfo info{};
        info.frequency = hz;

        if (hz <= 0.0f) {
            info.noteName      = "?";
            info.octave        = 0;
            info.centDeviation = 0.0f;
            return info;
        }

        // Continuous MIDI note number (A4 = 69)
        const float midiNote = 12.0f * std::log2(hz / concertPitch) + 69.0f;
        const int   rounded  = static_cast<int>(std::round(midiNote));

        // Cent deviation from the nearest semitone
        info.centDeviation = (midiNote - static_cast<float>(rounded)) * 100.0f;

        // Decompose into note name and octave.
        // MIDI note 0 = C-1 in scientific pitch notation.
        int noteIndex = rounded % 12;
        if (noteIndex < 0)
            noteIndex += 12;

        info.noteName = noteNames[noteIndex];
        info.octave   = (rounded / 12) - 1;

        // Correct for C++ truncation toward zero with negative dividends
        if (rounded < 0 && (rounded % 12 != 0))
            info.octave -= 1;

        return info;
    }

    // ---------------------------------------------------------------
    //  Reset
    // ---------------------------------------------------------------

    void reset() noexcept
    {
        m_writePos = 0;
        std::memset(m_sampleBuffer.data(), 0,
                    m_sampleBuffer.size() * sizeof(float));
        m_detectedPitch.store(0.0f, std::memory_order_relaxed);
        m_confidence.store(0.0f, std::memory_order_relaxed);
    }

private:
    // ---------------------------------------------------------------
    //  YIN core (called on the audio thread when buffer is full)
    // ---------------------------------------------------------------

    void runYin() noexcept
    {
        const int W = m_halfBuffer; // analysis window = half the buffer

        // ----------------------------------------------------------
        // Step 1 — Difference function  d(tau)
        //   d(tau) = sum_{i=0}^{W-1} (x[i] - x[i + tau])^2
        // ----------------------------------------------------------
        m_diffBuffer[0] = 0.0f;
        for (int tau = 1; tau < W; ++tau) {
            float sum = 0.0f;
            for (int i = 0; i < W; ++i) {
                const float delta =
                    m_sampleBuffer[static_cast<size_t>(i)] -
                    m_sampleBuffer[static_cast<size_t>(i + tau)];
                sum += delta * delta;
            }
            m_diffBuffer[static_cast<size_t>(tau)] = sum;
        }

        // ----------------------------------------------------------
        // Step 2 — Cumulative mean normalized difference  d'(tau)
        //   d'(0) = 1
        //   d'(tau) = d(tau) / ((1/tau) * sum_{j=1}^{tau} d(j))
        // ----------------------------------------------------------
        m_cmndfBuffer[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < W; ++tau) {
            runningSum += m_diffBuffer[static_cast<size_t>(tau)];
            if (runningSum < 1e-10f) {
                // Avoid division by zero for perfectly silent input
                m_cmndfBuffer[static_cast<size_t>(tau)] = 0.0f;
            } else {
                m_cmndfBuffer[static_cast<size_t>(tau)] =
                    m_diffBuffer[static_cast<size_t>(tau)] /
                    (runningSum / static_cast<float>(tau));
            }
        }

        // ----------------------------------------------------------
        // Step 3 — Absolute threshold
        //   Find the first tau where d'(tau) < threshold, starting
        //   from tau = 2 (skip 0 and 1 — they are trivially small).
        // ----------------------------------------------------------
        int bestTau = -1;
        for (int tau = 2; tau < W; ++tau) {
            if (m_cmndfBuffer[static_cast<size_t>(tau)] < m_threshold) {
                // Walk forward while the CMNDF is still decreasing
                // to find the local trough.
                while (tau + 1 < W &&
                       m_cmndfBuffer[static_cast<size_t>(tau + 1)] <
                           m_cmndfBuffer[static_cast<size_t>(tau)]) {
                    ++tau;
                }
                bestTau = tau;
                break;
            }
        }

        // No periodic signal found — report silence / unvoiced
        if (bestTau < 1) {
            m_detectedPitch.store(0.0f, std::memory_order_relaxed);
            m_confidence.store(0.0f, std::memory_order_relaxed);
            return;
        }

        // ----------------------------------------------------------
        // Step 4 — Parabolic interpolation for sub-sample accuracy
        //   Fit a parabola through (tau-1, tau, tau+1) and find the
        //   x-coordinate of its vertex.
        // ----------------------------------------------------------
        float refinedTau = static_cast<float>(bestTau);
        if (bestTau > 0 && bestTau < W - 1) {
            const float s0 = m_cmndfBuffer[static_cast<size_t>(bestTau - 1)];
            const float s1 = m_cmndfBuffer[static_cast<size_t>(bestTau)];
            const float s2 = m_cmndfBuffer[static_cast<size_t>(bestTau + 1)];
            const float denom = 2.0f * (2.0f * s1 - s0 - s2);
            if (std::fabs(denom) > 1e-12f) {
                refinedTau += (s0 - s2) / denom;
            }
        }

        // ----------------------------------------------------------
        // Step 5 — Convert lag to frequency
        // ----------------------------------------------------------
        if (refinedTau < 1.0f)
            refinedTau = 1.0f; // safety clamp

        const float pitch =
            static_cast<float>(m_sampleRate) / refinedTau;

        // Confidence: 1 minus the CMNDF value at the chosen lag.
        // A perfect periodic signal yields d'(tau) ~ 0, so conf ~ 1.
        float conf =
            1.0f - m_cmndfBuffer[static_cast<size_t>(bestTau)];
        if (conf < 0.0f) conf = 0.0f;
        if (conf > 1.0f) conf = 1.0f;

        m_detectedPitch.store(pitch, std::memory_order_relaxed);
        m_confidence.store(conf, std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------
    //  Data members
    // ---------------------------------------------------------------

    const int   m_bufferSize;   // total sample buffer length
    const int   m_halfBuffer;   // analysis window = bufferSize / 2
    const float m_threshold;    // CMNDF threshold (default 0.15)
    int         m_sampleRate;   // audio sample rate
    int         m_writePos;     // write cursor into m_sampleBuffer

    // Pre-allocated work buffers (sizes fixed at construction)
    std::vector<float> m_sampleBuffer; // incoming audio ring
    std::vector<float> m_diffBuffer;   // d(tau) — difference function
    std::vector<float> m_cmndfBuffer;  // d'(tau) — cumulative mean normalised diff

    // Results shared between audio and UI threads
    std::atomic<float> m_detectedPitch; // Hz (0 = no pitch)
    std::atomic<float> m_confidence;    // 0..1
};

#endif // MCASTER1_PITCH_DETECTOR_H
