// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AGC.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

AGC::AGC() = default;
AGC::~AGC() = default;

void AGC::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const float attackCoeff  = std::exp(-1.0f / (m_attackMs  * 0.001f * m_sampleRate));
    const float releaseCoeff = std::exp(-1.0f / (m_releaseMs * 0.001f * m_sampleRate));

    for (int f = 0; f < frames; ++f) {
        // Measure RMS across channels
        float sumSq = 0.0f;
        for (int c = 0; c < channels; ++c) {
            float s = buffer[f * channels + c];
            sumSq += s * s;
        }
        float rms = std::sqrt(sumSq / static_cast<float>(channels));
        float inputDb = (rms > 1e-12f) ? 20.0f * std::log10(rms) : -120.0f;

        // Desired gain
        float desiredGainDb = m_targetDb - inputDb;
        desiredGainDb = std::clamp(desiredGainDb, -60.0f, m_maxGainDb);

        // Slow envelope follower
        float coeff = (desiredGainDb < m_currentGainDb) ? attackCoeff : releaseCoeff;
        m_currentGainDb = coeff * m_currentGainDb + (1.0f - coeff) * desiredGainDb;

        float gainLin = std::pow(10.0f, m_currentGainDb / 20.0f);
        for (int c = 0; c < channels; ++c) {
            buffer[f * channels + c] *= gainLin;
        }
    }
}

void AGC::setParameter(int id, float value)
{
    switch (id) {
    case TargetDb:  m_targetDb  = value; break;
    case AttackMs:  m_attackMs  = std::max(1.0f, value); break;
    case ReleaseMs: m_releaseMs = std::max(1.0f, value); break;
    case MaxGainDb: m_maxGainDb = std::max(0.0f, value); break;
    default: break;
    }
}

float AGC::parameter(int id) const
{
    switch (id) {
    case TargetDb:  return m_targetDb;
    case AttackMs:  return m_attackMs;
    case ReleaseMs: return m_releaseMs;
    case MaxGainDb: return m_maxGainDb;
    default:        return 0.0f;
    }
}

QString AGC::name() const
{
    return QStringLiteral("AGC");
}

int AGC::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
