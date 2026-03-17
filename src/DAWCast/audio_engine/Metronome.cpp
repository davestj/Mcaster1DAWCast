// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Metronome.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

static constexpr float kPi = 3.14159265358979323846f;

// Click sound parameters
static constexpr float kDownbeatFreqHz = 880.0f;   // A5 — bright accent
static constexpr float kBeatFreqHz     = 440.0f;   // A4 — normal beat
static constexpr float kDownbeatDurSec = 0.020f;   // 20 ms
static constexpr float kBeatDurSec     = 0.015f;   // 15 ms
static constexpr float kDecayRate      = 150.0f;    // exponential decay rate

// dB to linear gain
static float dbToLinear(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

// ── Construction ────────────────────────────────────────────────────────────

Metronome::Metronome(QObject* parent)
    : QObject(parent)
{
    m_volumeDb = -6.0f;
    m_volumeLinear.store(dbToLinear(m_volumeDb), std::memory_order_release);
}

Metronome::~Metronome() = default;

// ── Setters (GUI thread) ────────────────────────────────────────────────────

void Metronome::setTempo(double bpm)
{
    bpm = std::clamp(bpm, 20.0, 300.0);
    m_bpm.store(bpm, std::memory_order_release);
}

void Metronome::setTimeSignature(int numerator, int denominator)
{
    if (numerator < 1)  numerator  = 1;
    if (denominator < 1) denominator = 1;
    m_numerator   = numerator;
    m_denominator = denominator;
}

void Metronome::setVolume(float db)
{
    db = std::clamp(db, -60.0f, 0.0f);
    m_volumeDb = db;
    m_volumeLinear.store(dbToLinear(db), std::memory_order_release);
}

void Metronome::setEnabled(bool enabled)
{
    m_enabled.store(enabled, std::memory_order_release);
}

void Metronome::setCountIn(int bars)
{
    m_countInBars = std::max(0, bars);
}

// ── Getters ─────────────────────────────────────────────────────────────────

bool Metronome::isEnabled() const
{
    return m_enabled.load(std::memory_order_acquire);
}

double Metronome::tempo() const
{
    return m_bpm.load(std::memory_order_acquire);
}

float Metronome::volumeDb() const
{
    return m_volumeDb;
}

// ── Click sample generation ─────────────────────────────────────────────────

void Metronome::generateClickSamples(int sampleRate)
{
    if (sampleRate == m_clickSampleRate) return;
    m_clickSampleRate = sampleRate;

    const float sr = static_cast<float>(sampleRate);

    // ── Downbeat click: 880 Hz sine, 20 ms, exponential decay ──────────
    {
        int numSamples = static_cast<int>(kDownbeatDurSec * sr);
        m_downbeatClick.resize(static_cast<size_t>(numSamples));

        for (int i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / sr;
            float envelope = std::exp(-kDecayRate * t);
            float sine = std::sin(2.0f * kPi * kDownbeatFreqHz * t);
            m_downbeatClick[static_cast<size_t>(i)] = sine * envelope;
        }
    }

    // ── Regular beat click: 440 Hz sine, 15 ms, exponential decay ──────
    {
        int numSamples = static_cast<int>(kBeatDurSec * sr);
        m_beatClick.resize(static_cast<size_t>(numSamples));

        for (int i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / sr;
            float envelope = std::exp(-kDecayRate * t);
            float sine = std::sin(2.0f * kPi * kBeatFreqHz * t);
            m_beatClick[static_cast<size_t>(i)] = sine * envelope;
        }
    }
}

// ── Audio thread click generation (RT-safe) ─────────────────────────────────

void Metronome::generateClick(float* buffer, int frames, int channels,
                              int sampleRate, int64_t playheadSamples)
{
    if (!m_enabled.load(std::memory_order_acquire)) return;
    if (!buffer || frames <= 0 || channels <= 0 || sampleRate <= 0) return;

    // Regenerate click samples if sample rate changed
    // (This does allocate, but only on the very first call or rate change —
    //  effectively a one-time cost, not per-buffer.)
    if (sampleRate != m_clickSampleRate) {
        generateClickSamples(sampleRate);
    }

    const double bpm = m_bpm.load(std::memory_order_acquire);
    const float  vol = m_volumeLinear.load(std::memory_order_acquire);
    const int    num = m_numerator;

    // Samples per beat = (60 / bpm) * sampleRate
    const double samplesPerBeat = (60.0 / bpm) * static_cast<double>(sampleRate);

    // For each frame in the buffer, check if it falls within a click window
    for (int f = 0; f < frames; ++f) {
        int64_t currentSample = playheadSamples + f;
        if (currentSample < 0) continue;

        // Which beat are we on?
        double beatPosition = static_cast<double>(currentSample) / samplesPerBeat;
        int    beatIndex    = static_cast<int>(std::floor(beatPosition));
        int    beatInBar    = beatIndex % num;
        bool   isDownbeat   = (beatInBar == 0);

        // Sample position within the current beat
        double sampleInBeat = static_cast<double>(currentSample)
                              - beatIndex * samplesPerBeat;
        int sampleOffset = static_cast<int>(sampleInBeat);

        // Select the appropriate click buffer
        const std::vector<float>& clickBuf =
            isDownbeat ? m_downbeatClick : m_beatClick;

        if (sampleOffset < 0 || sampleOffset >= static_cast<int>(clickBuf.size()))
            continue;

        // Mix the click sample into all channels
        float clickSample = clickBuf[static_cast<size_t>(sampleOffset)] * vol;
        for (int ch = 0; ch < channels; ++ch) {
            buffer[f * channels + ch] += clickSample;
        }

        // Store beat info atomically — GUI polls this via timer
        // (NEVER emit signals from the audio callback thread)
        if (sampleOffset == 0) {
            m_lastBeatIndex.store(beatIndex, std::memory_order_relaxed);
            m_lastBeatWasDownbeat.store(isDownbeat, std::memory_order_relaxed);
        }
    }
}

} // namespace dawcast
