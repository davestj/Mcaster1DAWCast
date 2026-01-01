// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "NoiseGate.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

NoiseGate::NoiseGate() = default;
NoiseGate::~NoiseGate() = default;

void NoiseGate::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const float attackCoeff  = std::exp(-1.0f / (m_attackMs  * 0.001f * m_sampleRate));
    const float releaseCoeff = std::exp(-1.0f / (m_releaseMs * 0.001f * m_sampleRate));
    const int   holdSamples  = static_cast<int>(m_holdMs * 0.001f * m_sampleRate);
    const float rangeLin     = std::pow(10.0f, m_rangeDb / 20.0f);
    const float openThresh   = std::pow(10.0f, m_thresholdDb / 20.0f);
    const float closeThresh  = std::pow(10.0f, (m_thresholdDb - HysteresisDb) / 20.0f);

    for (int f = 0; f < frames; ++f) {
        // Peak detection across channels
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::abs(buffer[f * channels + c]));
        }

        // State machine with hysteresis
        switch (m_state) {
        case State::Closed:
            if (peak >= openThresh) {
                m_state = State::Attack;
            }
            break;
        case State::Attack:
            m_envelope = attackCoeff * m_envelope + (1.0f - attackCoeff) * 1.0f;
            if (m_envelope >= 0.999f) {
                m_envelope = 1.0f;
                m_state = State::Open;
            }
            break;
        case State::Open:
            if (peak < closeThresh) {
                m_state = State::Hold;
                m_holdCounter = holdSamples;
            }
            break;
        case State::Hold:
            --m_holdCounter;
            if (peak >= openThresh) {
                m_state = State::Open;
            } else if (m_holdCounter <= 0) {
                m_state = State::Release;
            }
            break;
        case State::Release:
            m_envelope = releaseCoeff * m_envelope + (1.0f - releaseCoeff) * 0.0f;
            if (peak >= openThresh) {
                m_state = State::Attack;
            } else if (m_envelope <= 0.001f) {
                m_envelope = 0.0f;
                m_state = State::Closed;
            }
            break;
        }

        // Apply gate attenuation: at minimum, apply rangeLin (not full silence)
        float gain = rangeLin + (1.0f - rangeLin) * m_envelope;
        for (int c = 0; c < channels; ++c) {
            buffer[f * channels + c] *= gain;
        }
    }
}

void NoiseGate::setParameter(int id, float value)
{
    switch (id) {
    case ThresholdDb: m_thresholdDb = value; break;
    case AttackMs:    m_attackMs    = std::max(0.01f, value); break;
    case HoldMs:      m_holdMs      = std::max(0.0f, value); break;
    case ReleaseMs:   m_releaseMs   = std::max(0.01f, value); break;
    case RangeDb:     m_rangeDb     = value; break;
    default: break;
    }
}

float NoiseGate::parameter(int id) const
{
    switch (id) {
    case ThresholdDb: return m_thresholdDb;
    case AttackMs:    return m_attackMs;
    case HoldMs:      return m_holdMs;
    case ReleaseMs:   return m_releaseMs;
    case RangeDb:     return m_rangeDb;
    default:          return 0.0f;
    }
}

QString NoiseGate::name() const
{
    return QStringLiteral("Noise Gate");
}

int NoiseGate::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
