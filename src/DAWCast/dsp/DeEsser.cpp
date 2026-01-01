// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DeEsser.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

DeEsser::DeEsser()
{
    recalcFilters();
}

DeEsser::~DeEsser() = default;

void DeEsser::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const int ch = std::min(channels, MaxChannels);
    const float threshLin = std::pow(10.0f, m_thresholdDb / 20.0f);
    const float reductionLin = std::pow(10.0f, m_reductionDb / 20.0f);

    // Fast attack / moderate release envelope
    const float attackCoeff  = std::exp(-1.0f / (0.5f * 0.001f * m_sampleRate));
    const float releaseCoeff = std::exp(-1.0f / (20.0f * 0.001f * m_sampleRate));

    for (int f = 0; f < frames; ++f) {
        // Detect sibilance level across channels
        float detectPeak = 0.0f;
        for (int c = 0; c < ch; ++c) {
            float detected = m_detectFilter[c].process(buffer[f * channels + c]);
            detectPeak = std::max(detectPeak, std::abs(detected));
        }

        // Envelope follower
        float coeff = (detectPeak > std::pow(10.0f, m_envDb / 20.0f))
                      ? attackCoeff : releaseCoeff;
        float detectDb = (detectPeak > 1e-12f)
                         ? 20.0f * std::log10(detectPeak) : -120.0f;
        m_envDb = coeff * m_envDb + (1.0f - coeff) * detectDb;

        // Compute gain reduction for sibilant band
        float gain = 1.0f;
        if (m_envDb > m_thresholdDb) {
            float excess = m_envDb - m_thresholdDb;
            float maxReduction = -m_reductionDb; // positive dB amount
            float reductionAmount = std::min(excess, maxReduction);
            gain = std::pow(10.0f, -reductionAmount / 20.0f);
        }

        // Apply split-band attenuation: reduce only the sibilant band
        for (int c = 0; c < ch; ++c) {
            float& sample = buffer[f * channels + c];
            float bandSignal = m_reduceFilter[c].process(sample);
            // Subtract band, add back attenuated band
            sample = sample - bandSignal + bandSignal * gain;
        }
    }
}

void DeEsser::setParameter(int id, float value)
{
    switch (id) {
    case FrequencyHz: m_frequencyHz = std::max(500.0f, value); recalcFilters(); break;
    case Bandwidth:   m_bandwidth   = std::max(0.1f, value);   recalcFilters(); break;
    case ThresholdDb: m_thresholdDb = value; break;
    case ReductionDb: m_reductionDb = value; break;
    default: break;
    }
}

float DeEsser::parameter(int id) const
{
    switch (id) {
    case FrequencyHz: return m_frequencyHz;
    case Bandwidth:   return m_bandwidth;
    case ThresholdDb: return m_thresholdDb;
    case ReductionDb: return m_reductionDb;
    default:          return 0.0f;
    }
}

QString DeEsser::name() const
{
    return QStringLiteral("De-Esser");
}

int DeEsser::parameterCount() const
{
    return ParamCount;
}

void DeEsser::recalcFilters()
{
    auto bpCoeffs = Biquad::bandpass(m_frequencyHz, m_bandwidth, m_sampleRate);
    for (int c = 0; c < MaxChannels; ++c) {
        m_detectFilter[c].setCoeffs(bpCoeffs);
        m_detectFilter[c].reset();
        m_reduceFilter[c].setCoeffs(bpCoeffs);
        m_reduceFilter[c].reset();
    }
}

} // namespace dawcast
