// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"

namespace dawcast {

class NoiseGate : public IEffectUnit
{
public:
    enum Param
    {
        ThresholdDb = 0,
        AttackMs,
        HoldMs,
        ReleaseMs,
        RangeDb,
        ParamCount
    };

    NoiseGate();
    ~NoiseGate() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    enum class State { Closed, Attack, Open, Hold, Release };

    float m_thresholdDb = -40.0f;
    float m_attackMs    = 1.0f;
    float m_holdMs      = 50.0f;
    float m_releaseMs   = 100.0f;
    float m_rangeDb     = -80.0f;

    float m_sampleRate  = 48000.0f;
    State m_state       = State::Closed;
    float m_envelope    = 0.0f; // 0..1 gate attenuation
    int   m_holdCounter = 0;

    // Hysteresis: open threshold is m_thresholdDb,
    // close threshold is m_thresholdDb - hysteresisDb
    static constexpr float HysteresisDb = 3.0f;
};

} // namespace dawcast
