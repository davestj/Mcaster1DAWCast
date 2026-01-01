// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Automation.h"
#include <algorithm>
#include <cmath>

namespace dawcast {

Automation::Automation(QObject* parent)
    : QObject(parent)
{
}

Automation::~Automation() = default;

void Automation::addPoint(int64_t time, float value)
{
    Point pt{time, value};

    // Insert in sorted order by time
    auto it = std::lower_bound(m_points.begin(), m_points.end(), pt,
        [](const Point& a, const Point& b) { return a.time < b.time; });
    m_points.insert(it, pt);
}

void Automation::removePoint(int index)
{
    if (index >= 0 && index < m_points.size()) {
        m_points.remove(index);
    }
}

float Automation::valueAt(int64_t time) const
{
    if (m_points.isEmpty()) return 0.0f;
    if (m_points.size() == 1) return m_points.first().value;

    // Before first point
    if (time <= m_points.first().time) return m_points.first().value;

    // After last point
    if (time >= m_points.last().time) return m_points.last().value;

    // Find surrounding points
    for (int i = 0; i < m_points.size() - 1; ++i) {
        const auto& a = m_points[i];
        const auto& b = m_points[i + 1];

        if (time >= a.time && time <= b.time) {
            if (b.time == a.time) return a.value;

            float t = static_cast<float>(time - a.time) / static_cast<float>(b.time - a.time);

            switch (m_interpolation) {
            case Interpolation::Linear:
                return a.value + t * (b.value - a.value);
            case Interpolation::Curve:
                // Hermite/S-curve interpolation
                t = t * t * (3.0f - 2.0f * t);
                return a.value + t * (b.value - a.value);
            }
        }
    }

    return 0.0f;
}

QVector<Automation::Point> Automation::points() const
{
    return m_points;
}

int Automation::pointCount() const
{
    return m_points.size();
}

} // namespace dawcast
