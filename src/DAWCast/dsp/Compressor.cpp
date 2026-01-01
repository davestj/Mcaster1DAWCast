// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Compressor.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

Compressor::Compressor() = default;
Compressor::~Compressor() = default;

void Compressor::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const float attackCoeff  = std::exp(-1.0f / (m_attackMs  * 0.001f * m_sampleRate));
    const float releaseCoeff = std::exp(-1.0f / (m_releaseMs * 0.001f * m_sampleRate));
    const float makeupLin    = std::pow(10.0f, m_makeupDb / 20.0f);

    for (int f = 0; f < frames; ++f) {
        // RMS level detection across channels
        float sumSq = 0.0f;
        for (int c = 0; c < channels; ++c) {
            float s = buffer[f * channels + c];
            sumSq += s * s;
        }
        float rms = std::sqrt(sumSq / static_cast<float>(channels));
        float inputDb = (rms > 1e-12f) ? 20.0f * std::log10(rms) : -120.0f;

        // Smooth envelope
        float coeff = (inputDb > m_envDb) ? attackCoeff : releaseCoeff;
        m_envDb = coeff * m_envDb + (1.0f - coeff) * inputDb;

        // Gain computer with soft knee
        float gainDb = computeGain(m_envDb) - m_envDb;
        float gainLin = std::pow(10.0f, gainDb / 20.0f) * makeupLin;

        for (int c = 0; c < channels; ++c) {
            buffer[f * channels + c] *= gainLin;
        }
    }
}

float Compressor::computeGain(float levelDb) const
{
    // Soft-knee gain computer
    const float halfKnee = m_kneeDb * 0.5f;

    if (levelDb <= m_thresholdDb - halfKnee) {
        // Below knee — no compression
        return levelDb;
    }
    if (levelDb >= m_thresholdDb + halfKnee) {
        // Above knee — full compression
        return m_thresholdDb + (levelDb - m_thresholdDb) / m_ratio;
    }
    // Inside knee — quadratic interpolation
    float x = levelDb - m_thresholdDb + halfKnee;
    return levelDb + ((1.0f / m_ratio) - 1.0f) * x * x / (2.0f * m_kneeDb);
}

void Compressor::setParameter(int id, float value)
{
    switch (id) {
    case ThresholdDb: m_thresholdDb = value; break;
    case Ratio:       m_ratio       = std::max(1.0f, value); break;
    case AttackMs:    m_attackMs    = std::max(0.01f, value); break;
    case ReleaseMs:   m_releaseMs   = std::max(0.01f, value); break;
    case MakeupDb:    m_makeupDb    = value; break;
    case KneeDb:      m_kneeDb      = std::max(0.0f, value); break;
    default: break;
    }
}

float Compressor::parameter(int id) const
{
    switch (id) {
    case ThresholdDb: return m_thresholdDb;
    case Ratio:       return m_ratio;
    case AttackMs:    return m_attackMs;
    case ReleaseMs:   return m_releaseMs;
    case MakeupDb:    return m_makeupDb;
    case KneeDb:      return m_kneeDb;
    default:          return 0.0f;
    }
}

QString Compressor::name() const
{
    return QStringLiteral("Compressor");
}

int Compressor::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
