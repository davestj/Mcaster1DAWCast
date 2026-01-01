// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

#include <QColor>

namespace dawcast {

class ChromaKey : public IVideoEffect
{
public:
    enum Param
    {
        KeyColorR = 0,       // QColor split into R,G,B for parameterised control
        KeyColorG,
        KeyColorB,
        HueRange,            // degrees of hue tolerance
        SaturationThreshold, // minimum saturation to key
        SpillSuppression,    // 0..1
        ParamCount
    };

    ChromaKey();
    ~ChromaKey() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;

    void setKeyColor(const QColor& color) { m_keyColor = color; }
    void setHueRange(float degrees) { m_hueRange = degrees; }
    void setSaturationThreshold(float sat) { m_satThreshold = sat; }
    void setSpillSuppression(float amount) { m_spillSuppression = amount; }

private:
    QColor m_keyColor         = QColor(0, 255, 0); // default green screen
    float  m_hueRange         = 40.0f;   // +/- degrees from key hue
    float  m_satThreshold     = 0.2f;    // min saturation for keying
    float  m_spillSuppression = 0.5f;    // 0..1
};

} // namespace dawcast
