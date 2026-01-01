// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVector>
#include <cstdint>

namespace dawcast {

class Automation : public QObject
{
    Q_OBJECT

public:
    struct Point {
        int64_t time  = 0;
        float   value = 0.0f;
    };

    enum class Interpolation {
        Linear,
        Curve
    };

    explicit Automation(QObject* parent = nullptr);
    ~Automation() override;

    void addPoint(int64_t time, float value);
    void removePoint(int index);
    [[nodiscard]] float valueAt(int64_t time) const;
    [[nodiscard]] QVector<Point> points() const;
    [[nodiscard]] int pointCount() const;

    void setInterpolation(Interpolation mode) { m_interpolation = mode; }
    [[nodiscard]] Interpolation interpolation() const { return m_interpolation; }

private:
    QVector<Point>  m_points;
    Interpolation   m_interpolation = Interpolation::Linear;
};

} // namespace dawcast
