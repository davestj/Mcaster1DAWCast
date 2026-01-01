// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

namespace dawcast {

class LumaKey : public IVideoEffect
{
public:
    enum Param
    {
        Threshold = 0,
        Softness,
        Invert,
        ParamCount
    };

    LumaKey();
    ~LumaKey() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;

    void setThreshold(float t) { m_threshold = t; }
    void setSoftness(float s) { m_softness = s; }
    void setInvert(bool inv) { m_invert = inv; }

private:
    float m_threshold = 0.5f;  // 0..1 luminance threshold
    float m_softness  = 0.1f;  // 0..1 edge softness
    bool  m_invert    = false;
};

} // namespace dawcast
