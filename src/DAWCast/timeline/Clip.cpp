// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Clip.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dawcast {

Clip::Clip(QObject* parent)
    : QObject(parent)
{
}

Clip::~Clip() = default;

void Clip::setSourcePath(const QString& path)
{
    m_sourcePath = path;
}

void Clip::setSourceIn(int64_t samples)
{
    m_sourceIn = samples;
}

void Clip::setSourceOut(int64_t samples)
{
    m_sourceOut = samples;
}

void Clip::setTimelinePosition(int64_t samples)
{
    m_timelinePosition = samples;
}

void Clip::setGain(float gain)
{
    m_gain = gain;
}

void Clip::setFadeIn(int64_t samples)
{
    m_fadeIn = samples;
}

void Clip::setFadeOut(int64_t samples)
{
    m_fadeOut = samples;
}

// ── Clip Gain Envelope ──────────────────────────────────────────────────────

void Clip::addGainPoint(int64_t offsetSamples, float gainDb)
{
    GainPoint pt;
    pt.offsetSamples = offsetSamples;
    pt.gainDb = gainDb;

    // Insert in sorted order by offsetSamples
    auto it = std::lower_bound(m_gainEnvelope.begin(), m_gainEnvelope.end(), pt,
        [](const GainPoint& a, const GainPoint& b) {
            return a.offsetSamples < b.offsetSamples;
        });
    m_gainEnvelope.insert(it, pt);
}

void Clip::removeGainPoint(int index)
{
    if (index >= 0 && index < m_gainEnvelope.size())
        m_gainEnvelope.removeAt(index);
}

void Clip::moveGainPoint(int index, int64_t offsetSamples, float gainDb)
{
    if (index < 0 || index >= m_gainEnvelope.size())
        return;

    // Remove and re-add to maintain sort order
    m_gainEnvelope.removeAt(index);

    GainPoint pt;
    pt.offsetSamples = offsetSamples;
    pt.gainDb = gainDb;
    auto it = std::lower_bound(m_gainEnvelope.begin(), m_gainEnvelope.end(), pt,
        [](const GainPoint& a, const GainPoint& b) {
            return a.offsetSamples < b.offsetSamples;
        });
    m_gainEnvelope.insert(it, pt);
}

void Clip::setGainEnvelope(const QList<GainPoint>& envelope)
{
    m_gainEnvelope = envelope;
    // Ensure sorted
    std::sort(m_gainEnvelope.begin(), m_gainEnvelope.end(),
        [](const GainPoint& a, const GainPoint& b) {
            return a.offsetSamples < b.offsetSamples;
        });
}

float Clip::gainAt(int64_t offsetSamples) const
{
    // If no envelope points, return the base gain (already linear)
    if (m_gainEnvelope.isEmpty())
        return m_gain;

    // Helper: convert dB to linear gain
    auto dbToLinear = [](float db) -> float {
        if (db <= -120.0f) return 0.0f;  // treat -120 dB as silence
        return std::pow(10.0f, db / 20.0f);
    };

    // Before the first point: use the first point's value
    if (offsetSamples <= m_gainEnvelope.first().offsetSamples)
        return dbToLinear(m_gainEnvelope.first().gainDb);

    // After the last point: use the last point's value
    if (offsetSamples >= m_gainEnvelope.last().offsetSamples)
        return dbToLinear(m_gainEnvelope.last().gainDb);

    // Find the surrounding pair and linearly interpolate in dB
    for (int i = 0; i < m_gainEnvelope.size() - 1; ++i) {
        const GainPoint& a = m_gainEnvelope[i];
        const GainPoint& b = m_gainEnvelope[i + 1];

        if (offsetSamples >= a.offsetSamples && offsetSamples <= b.offsetSamples) {
            int64_t span = b.offsetSamples - a.offsetSamples;
            if (span == 0)
                return dbToLinear(a.gainDb);

            float frac = static_cast<float>(offsetSamples - a.offsetSamples)
                       / static_cast<float>(span);
            float interpDb = a.gainDb + frac * (b.gainDb - a.gainDb);
            return dbToLinear(interpDb);
        }
    }

    // Fallback (should not reach here)
    return dbToLinear(m_gainEnvelope.last().gainDb);
}

} // namespace dawcast
