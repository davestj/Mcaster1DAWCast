// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"
#include "dsp/Biquad.h"

#include <array>

namespace dawcast {

class DeEsser : public IEffectUnit
{
public:
    enum Param
    {
        FrequencyHz = 0,
        Bandwidth,
        ThresholdDb,
        ReductionDb,
        ParamCount
    };

    DeEsser();
    ~DeEsser() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    void recalcFilters();

    float m_frequencyHz = 6500.0f;
    float m_bandwidth   = 2.0f;  // Q
    float m_thresholdDb = -20.0f;
    float m_reductionDb = -12.0f;

    float m_sampleRate  = 48000.0f;

    // Split-band: bandpass for detection, same biquad for attenuation
    static constexpr int MaxChannels = 16;
    std::array<Biquad, MaxChannels> m_detectFilter;  // bandpass for sibilance detection
    std::array<Biquad, MaxChannels> m_reduceFilter;  // bandpass for gain reduction path

    float m_envDb = -120.0f; // envelope follower
};

} // namespace dawcast
