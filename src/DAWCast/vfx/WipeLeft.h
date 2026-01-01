// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

namespace dawcast {

class WipeLeft : public IVideoEffect
{
public:
    enum class Direction : int
    {
        Left  = 0,
        Right = 1,
        Up    = 2,
        Down  = 3
    };

    WipeLeft();
    ~WipeLeft() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;

    void setDirection(Direction dir) { m_direction = dir; }
    void setEdgeSoftness(float softness) { m_edgeSoftness = softness; }

private:
    Direction m_direction    = Direction::Left;
    float     m_edgeSoftness = 0.02f; // fraction of image dimension
};

} // namespace dawcast
