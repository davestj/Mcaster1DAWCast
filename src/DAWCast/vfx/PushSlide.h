// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

namespace dawcast {

class PushSlide : public IVideoEffect
{
public:
    enum class Direction : int
    {
        Left  = 0,
        Right = 1,
        Up    = 2,
        Down  = 3
    };

    enum class Easing : int
    {
        Linear    = 0,
        EaseIn    = 1,
        EaseOut   = 2,
        EaseInOut = 3
    };

    PushSlide();
    ~PushSlide() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;

    void setDirection(Direction dir) { m_direction = dir; }
    void setEasing(Easing e) { m_easing = e; }

private:
    float applyEasing(float t) const;

    Direction m_direction = Direction::Left;
    Easing    m_easing    = Easing::EaseInOut;
};

} // namespace dawcast
