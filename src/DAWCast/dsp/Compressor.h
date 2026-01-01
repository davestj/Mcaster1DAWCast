// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"

namespace dawcast {

class Compressor : public IEffectUnit
{
public:
    enum Param
    {
        ThresholdDb = 0,
        Ratio,
        AttackMs,
        ReleaseMs,
        MakeupDb,
        KneeDb,
        ParamCount
    };

    Compressor();
    ~Compressor() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    float computeGain(float levelDb) const;

    float m_thresholdDb = -20.0f;
    float m_ratio       = 4.0f;
    float m_attackMs    = 10.0f;
    float m_releaseMs   = 100.0f;
    float m_makeupDb    = 0.0f;
    float m_kneeDb      = 6.0f;

    float m_sampleRate  = 48000.0f;
    float m_envDb       = -120.0f; // RMS envelope state (dB)
};

} // namespace dawcast
