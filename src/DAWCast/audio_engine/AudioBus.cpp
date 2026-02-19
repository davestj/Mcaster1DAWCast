// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioBus.h"
#include "../dsp/DspChain.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dawcast {

// pi/2 for constant-power pan law (matches AudioMixer)
static constexpr float kHalfPi = 1.5707963267948966f;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AudioBus::AudioBus(const QString& name, BusType type, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_type(type)
{
}

AudioBus::~AudioBus() = default;

// ---------------------------------------------------------------------------
// GUI-thread setters
// ---------------------------------------------------------------------------

void AudioBus::setVolume(float db)  { m_volumeDb = db; }
void AudioBus::setPan(float pan)    { m_pan = std::clamp(pan, -1.0f, 1.0f); }
void AudioBus::setMuted(bool muted) { m_muted = muted; }
void AudioBus::setSolo(bool solo)   { m_solo = solo; }

DspChain* AudioBus::effectChain()
{
    if (!m_effectChain) {
        m_effectChain = new DspChain(this);
    }
    return m_effectChain;
}

// ---------------------------------------------------------------------------
// Audio-thread mixing (RT-safe)
// ---------------------------------------------------------------------------

void AudioBus::addInput(const float* buffer, int frames, int channels, float sendLevel)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const auto totalSamples = static_cast<size_t>(frames * channels);

    // Ensure internal buffer is large enough (resize is safe because
    // clearBuffer() is always called first at the start of each block,
    // guaranteeing the buffer is already the right size).
    if (m_buffer.size() < totalSamples) {
        m_buffer.resize(totalSamples, 0.0f);
    }

    // Accumulate scaled input
    for (size_t i = 0; i < totalSamples; ++i) {
        m_buffer[i] += buffer[i] * sendLevel;
    }
}

void AudioBus::process(int frames, int channels)
{
    if (frames <= 0 || channels <= 0) return;

    const auto totalSamples = static_cast<size_t>(frames * channels);
    if (m_buffer.size() < totalSamples) return;

    // Apply insert effects
    if (m_effectChain && m_effectChain->effectCount() > 0) {
        m_effectChain->process(m_buffer.data(), frames, channels);
    }

    // If muted, silence the output and return
    if (m_muted) {
        std::memset(m_buffer.data(), 0, totalSamples * sizeof(float));
        m_peakLevel = 0.0f;
        return;
    }

    // Convert dB to linear gain
    float linearGain = 0.0f;
    if (m_volumeDb > -96.0f) {
        linearGain = std::pow(10.0f, m_volumeDb / 20.0f);
    }

    // Apply volume and constant-power panning
    float panNorm  = (m_pan + 1.0f) * 0.5f;
    float leftGain  = std::cos(panNorm * kHalfPi) * linearGain;
    float rightGain = std::sin(panNorm * kHalfPi) * linearGain;

    float peak = 0.0f;

    if (channels >= 2) {
        for (int f = 0; f < frames; ++f) {
            float l = m_buffer[static_cast<size_t>(f * channels)]     * leftGain;
            float r = m_buffer[static_cast<size_t>(f * channels + 1)] * rightGain;
            m_buffer[static_cast<size_t>(f * channels)]     = l;
            m_buffer[static_cast<size_t>(f * channels + 1)] = r;
            peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
        }
    } else {
        // Mono: just apply linear gain
        for (int f = 0; f < frames; ++f) {
            float s = m_buffer[static_cast<size_t>(f)] * linearGain;
            m_buffer[static_cast<size_t>(f)] = s;
            peak = std::max(peak, std::fabs(s));
        }
    }

    m_peakLevel = peak;
}

const float* AudioBus::output() const
{
    return m_buffer.empty() ? nullptr : m_buffer.data();
}

void AudioBus::clearBuffer(int frames, int channels)
{
    const auto totalSamples = static_cast<size_t>(frames * channels);

    if (m_buffer.size() < totalSamples) {
        m_buffer.resize(totalSamples, 0.0f);
    } else {
        std::memset(m_buffer.data(), 0, totalSamples * sizeof(float));
    }

    m_peakLevel = 0.0f;
}

} // namespace dawcast
