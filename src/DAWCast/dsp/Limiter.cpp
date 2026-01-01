// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Limiter.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

Limiter::Limiter()
{
    m_delayLength = static_cast<int>(m_lookaheadMs * 0.001f * m_sampleRate);
}

Limiter::~Limiter() = default;

void Limiter::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    if (channels != m_currentChannels) {
        resizeDelayLine(channels);
    }

    const float ceilingLin = std::pow(10.0f, m_ceilingDb / 20.0f);
    const float releaseCoeff = std::exp(-1.0f / (m_releaseMs * 0.001f * m_sampleRate));

    for (int f = 0; f < frames; ++f) {
        // Find peak across channels for this frame
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::abs(buffer[f * channels + c]));
        }

        // Compute required gain reduction for this incoming sample.
        // Since we write the input into the delay line and read the
        // delayed output, the gain envelope has m_delayLength samples
        // of look-ahead time to ramp down before the peak arrives at
        // the output.
        float targetGain = (peak > ceilingLin) ? (ceilingLin / peak) : 1.0f;

        // Smooth gain envelope:
        // - Attack is instant (take the minimum immediately) so the
        //   gain is already reduced by the time the peak exits the
        //   delay line.
        // - Release is exponential (slow recovery).
        if (targetGain < m_gainLin) {
            m_gainLin = targetGain;  // instant attack
        } else {
            m_gainLin = releaseCoeff * m_gainLin + (1.0f - releaseCoeff) * targetGain;
        }

        // Write input into delay line, read delayed output and apply gain
        for (int c = 0; c < channels; ++c) {
            if (m_delayLength > 0 && !m_delayBuffer.empty()) {
                int bufSize = static_cast<int>(m_delayBuffer[c].size());
                m_delayBuffer[c][m_delayWritePos] = buffer[f * channels + c];
                int readPos = (m_delayWritePos - m_delayLength + bufSize * 2) % bufSize;
                buffer[f * channels + c] = m_delayBuffer[c][readPos] * m_gainLin;
            } else {
                buffer[f * channels + c] *= m_gainLin;
            }
        }
        if (m_delayLength > 0 && !m_delayBuffer.empty()) {
            m_delayWritePos = (m_delayWritePos + 1) % static_cast<int>(m_delayBuffer[0].size());
        }
    }
}

void Limiter::setParameter(int id, float value)
{
    switch (id) {
    case CeilingDb:   m_ceilingDb   = value; break;
    case ReleaseMs:   m_releaseMs   = std::max(0.1f, value); break;
    case LookaheadMs:
        m_lookaheadMs = std::max(0.0f, value);
        m_delayLength = static_cast<int>(m_lookaheadMs * 0.001f * m_sampleRate);
        resizeDelayLine(m_currentChannels);
        break;
    default: break;
    }
}

float Limiter::parameter(int id) const
{
    switch (id) {
    case CeilingDb:   return m_ceilingDb;
    case ReleaseMs:   return m_releaseMs;
    case LookaheadMs: return m_lookaheadMs;
    default:          return 0.0f;
    }
}

QString Limiter::name() const
{
    return QStringLiteral("Brickwall Limiter");
}

int Limiter::parameterCount() const
{
    return ParamCount;
}

void Limiter::resizeDelayLine(int channels)
{
    m_currentChannels = channels;
    if (m_delayLength <= 0 || channels <= 0) {
        m_delayBuffer.clear();
        return;
    }
    m_delayBuffer.resize(channels);
    for (auto& ch : m_delayBuffer) {
        ch.assign(m_delayLength + 1, 0.0f);
    }
    m_delayWritePos = 0;
}

} // namespace dawcast
