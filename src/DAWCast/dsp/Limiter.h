// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"

#include <vector>

namespace dawcast {

class Limiter : public IEffectUnit
{
public:
    enum Param
    {
        CeilingDb = 0,
        ReleaseMs,
        LookaheadMs,
        ParamCount
    };

    Limiter();
    ~Limiter() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    void resizeDelayLine(int channels);

    float m_ceilingDb   = -0.3f;
    float m_releaseMs   = 50.0f;
    float m_lookaheadMs = 5.0f;

    float m_sampleRate  = 48000.0f;
    float m_gainLin     = 1.0f; // current gain envelope

    // Delay line for look-ahead
    std::vector<std::vector<float>> m_delayBuffer; // [channel][sample]
    int m_delayWritePos = 0;
    int m_delayLength   = 0;
    int m_currentChannels = 0;
};

} // namespace dawcast
