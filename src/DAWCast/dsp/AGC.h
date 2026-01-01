// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"

namespace dawcast {

// Slow automatic gain control — adjusts gain to maintain a target RMS level.

class AGC : public IEffectUnit
{
public:
    enum Param
    {
        TargetDb = 0,
        AttackMs,
        ReleaseMs,
        MaxGainDb,
        ParamCount
    };

    AGC();
    ~AGC() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    float m_targetDb  = -18.0f;
    float m_attackMs  = 500.0f;
    float m_releaseMs = 2000.0f;
    float m_maxGainDb = 30.0f;

    float m_sampleRate = 48000.0f;
    float m_currentGainDb = 0.0f; // current applied gain in dB
};

} // namespace dawcast
