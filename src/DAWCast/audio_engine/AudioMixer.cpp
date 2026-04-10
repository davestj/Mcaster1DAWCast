// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioMixer.h"
#include <cmath>
#include <algorithm>

namespace dawcast {

// pi/2 constant for constant-power pan law
static constexpr float kHalfPi = 1.5707963267948966f;

AudioMixer::AudioMixer(QObject* parent)
    : QObject(parent)
{
}

AudioMixer::~AudioMixer() = default;

int AudioMixer::addStrip()
{
    if (m_strips.size() >= MaxStrips) return -1;
    m_strips.append(Strip{});
    return m_strips.size() - 1;
}

void AudioMixer::removeStrip(int index)
{
    if (index >= 0 && index < m_strips.size()) {
        m_strips.remove(index);
    }
}

int AudioMixer::stripCount() const
{
    return m_strips.size();
}

void AudioMixer::setStripVolume(int strip, float db)
{
    if (strip >= 0 && strip < m_strips.size()) {
        m_strips[strip].volumeDb = db;
    }
}

void AudioMixer::setStripPan(int strip, float pan)
{
    if (strip >= 0 && strip < m_strips.size()) {
        m_strips[strip].pan = std::clamp(pan, -1.0f, 1.0f);
    }
}

void AudioMixer::setStripMuted(int strip, bool muted)
{
    if (strip >= 0 && strip < m_strips.size()) {
        m_strips[strip].muted = muted;
    }
}

void AudioMixer::setStripSolo(int strip, bool solo)
{
    if (strip >= 0 && strip < m_strips.size()) {
        m_strips[strip].solo = solo;
    }
}

void AudioMixer::setStripBuffer(int strip, const AudioBuffer* buffer)
{
    if (strip >= 0 && strip < m_strips.size()) {
        m_strips[strip].inputBuffer = buffer;
    }
}

float AudioMixer::stripVolume(int strip) const
{
    if (strip >= 0 && strip < m_strips.size()) {
        return m_strips[strip].volumeDb;
    }
    return 0.0f;
}

float AudioMixer::stripPan(int strip) const
{
    if (strip >= 0 && strip < m_strips.size()) {
        return m_strips[strip].pan;
    }
    return 0.0f;
}

void AudioMixer::ensureMeterCapacity(int count) const
{
    while (static_cast<int>(m_meters.size()) < count) {
        m_meters.emplace_back(std::make_unique<StripMeters>());
    }
}

float AudioMixer::stripPeakL(int strip) const
{
    if (strip < 0 || strip >= static_cast<int>(m_meters.size()) || !m_meters[strip]) return 0.0f;
    return m_meters[strip]->peakL.load(std::memory_order_relaxed);
}

float AudioMixer::stripPeakR(int strip) const
{
    if (strip < 0 || strip >= static_cast<int>(m_meters.size()) || !m_meters[strip]) return 0.0f;
    return m_meters[strip]->peakR.load(std::memory_order_relaxed);
}

float AudioMixer::masterPeakL() const
{
    return m_masterPeakL.load(std::memory_order_relaxed);
}

float AudioMixer::masterPeakR() const
{
    return m_masterPeakR.load(std::memory_order_relaxed);
}

void AudioMixer::setSoloMode(SoloMode mode)
{
    if (m_soloMode != mode) {
        m_soloMode = mode;
        emit soloModeChanged(mode);
    }
}

AudioMixer::SoloMode AudioMixer::soloMode() const
{
    return m_soloMode;
}

void AudioMixer::setSoloDimDb(float db)
{
    float clamped = std::clamp(db, -96.0f, 0.0f);
    if (m_soloDimDb != clamped) {
        m_soloDimDb = clamped;
        emit soloDimDbChanged(clamped);
    }
}

float AudioMixer::soloDimDb() const
{
    return m_soloDimDb;
}

void AudioMixer::process(AudioBuffer& output)
{
    // Start with silence
    output.silence();

    // Make sure the meter slots match the current strip count. This is
    // technically an allocation, but only when the strip count grows —
    // steady-state processing is allocation-free.
    ensureMeterCapacity(m_strips.size());

    if (m_strips.isEmpty()) {
        m_masterPeakL.store(0.0f, std::memory_order_relaxed);
        m_masterPeakR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Determine if any strip is soloed
    bool anySoloed = false;
    for (const auto& strip : m_strips) {
        if (strip.solo) {
            anySoloed = true;
            break;
        }
    }

    // Pre-compute Solo-in-Front dim gain (linear) if needed
    float dimGain = 0.0f;
    if (anySoloed && m_soloMode == SoloInFront && m_soloDimDb > -96.0f) {
        dimGain = std::pow(10.0f, m_soloDimDb / 20.0f);
    }

    const int frames   = output.frames;
    const int channels = output.channels;

    int stripIdx = -1;
    for (const auto& strip : m_strips) {
        ++stripIdx;

        // Default this strip's meters to silence; overridden below if audible.
        float stripPeakL = 0.0f;
        float stripPeakR = 0.0f;

        // Skip strips with no input buffer
        if (!strip.inputBuffer || !strip.inputBuffer->data) {
            if (stripIdx < static_cast<int>(m_meters.size()) && m_meters[stripIdx]) {
                m_meters[stripIdx]->peakL.store(0.0f, std::memory_order_relaxed);
                m_meters[stripIdx]->peakR.store(0.0f, std::memory_order_relaxed);
            }
            continue;
        }

        // Mute logic — always applies
        if (strip.muted) {
            if (stripIdx < static_cast<int>(m_meters.size()) && m_meters[stripIdx]) {
                m_meters[stripIdx]->peakL.store(0.0f, std::memory_order_relaxed);
                m_meters[stripIdx]->peakR.store(0.0f, std::memory_order_relaxed);
            }
            continue;
        }

        // Solo routing logic
        if (anySoloed && !strip.solo) {
            if (m_soloMode == SoloInPlace) {
                // SIP: non-soloed tracks are fully muted
                if (stripIdx < static_cast<int>(m_meters.size()) && m_meters[stripIdx]) {
                    m_meters[stripIdx]->peakL.store(0.0f, std::memory_order_relaxed);
                    m_meters[stripIdx]->peakR.store(0.0f, std::memory_order_relaxed);
                }
                continue;
            }
            // SIF: non-soloed tracks will be dimmed (handled below via dimGain)
        }

        // Convert dB to linear gain: gain = 10^(dB/20)
        // Treat -inf dB (very low values) as silence
        float linearGain = 0.0f;
        if (strip.volumeDb > -96.0f) {
            linearGain = std::pow(10.0f, strip.volumeDb / 20.0f);
        }

        // Apply Solo-in-Front dim to non-soloed strips
        if (anySoloed && !strip.solo && m_soloMode == SoloInFront) {
            linearGain *= dimGain;
        }

        // Constant-power panning
        // pan: -1.0 (full left) to +1.0 (full right), 0.0 = center
        // Normalize to [0, 1] for the trig calculation
        float panNorm = (strip.pan + 1.0f) * 0.5f; // 0.0 = left, 1.0 = right
        float leftGain  = std::cos(panNorm * kHalfPi) * linearGain;
        float rightGain = std::sin(panNorm * kHalfPi) * linearGain;

        const float* src = strip.inputBuffer->data;
        float*       dst = output.data;
        const int srcFrames = std::min(frames, strip.inputBuffer->frames);
        const int srcChannels = strip.inputBuffer->channels;

        if (channels >= 2 && srcChannels >= 2) {
            // Stereo input -> stereo output
            for (int f = 0; f < srcFrames; ++f) {
                const float inL = src[f * srcChannels];
                const float inR = src[f * srcChannels + 1];
                const float outL = inL * leftGain;
                const float outR = inR * rightGain;
                dst[f * channels]     += outL;
                dst[f * channels + 1] += outR;
                const float aL = std::fabs(outL);
                const float aR = std::fabs(outR);
                if (aL > stripPeakL) stripPeakL = aL;
                if (aR > stripPeakR) stripPeakR = aR;
            }
        } else if (channels >= 2 && srcChannels == 1) {
            // Mono input -> stereo output
            for (int f = 0; f < srcFrames; ++f) {
                const float mono = src[f];
                const float outL = mono * leftGain;
                const float outR = mono * rightGain;
                dst[f * channels]     += outL;
                dst[f * channels + 1] += outR;
                const float aL = std::fabs(outL);
                const float aR = std::fabs(outR);
                if (aL > stripPeakL) stripPeakL = aL;
                if (aR > stripPeakR) stripPeakR = aR;
            }
        } else if (channels == 1) {
            // Mono output — sum with linear gain only (pan has no effect)
            for (int f = 0; f < srcFrames; ++f) {
                float monoIn = 0.0f;
                for (int c = 0; c < srcChannels; ++c) {
                    monoIn += src[f * srcChannels + c];
                }
                monoIn /= static_cast<float>(srcChannels);
                const float outM = monoIn * linearGain;
                dst[f] += outM;
                const float aM = std::fabs(outM);
                if (aM > stripPeakL) stripPeakL = aM;
            }
            // Mirror the mono peak to the right channel for UI consistency
            stripPeakR = stripPeakL;
        }

        if (stripIdx < static_cast<int>(m_meters.size()) && m_meters[stripIdx]) {
            m_meters[stripIdx]->peakL.store(stripPeakL, std::memory_order_relaxed);
            m_meters[stripIdx]->peakR.store(stripPeakR, std::memory_order_relaxed);
        }
    }

    // Master output peak (post-mix). Walk the summed output buffer once.
    float masterPeakL = 0.0f;
    float masterPeakR = 0.0f;
    if (channels >= 2) {
        for (int f = 0; f < frames; ++f) {
            float aL = std::fabs(output.data[f * channels]);
            float aR = std::fabs(output.data[f * channels + 1]);
            if (aL > masterPeakL) masterPeakL = aL;
            if (aR > masterPeakR) masterPeakR = aR;
        }
    } else if (channels == 1) {
        for (int f = 0; f < frames; ++f) {
            float a = std::fabs(output.data[f]);
            if (a > masterPeakL) masterPeakL = a;
        }
        masterPeakR = masterPeakL;
    }
    m_masterPeakL.store(masterPeakL, std::memory_order_relaxed);
    m_masterPeakR.store(masterPeakR, std::memory_order_relaxed);
}

} // namespace dawcast
