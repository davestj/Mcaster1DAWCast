// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"
#include "dsp/Biquad.h"

#include <array>
#include <vector>

namespace dawcast {

class ParametricEQ : public IEffectUnit
{
public:
    static constexpr int NumBands = 10;
    static constexpr int ParamsPerBand = 4; // freq, Q, gain, type
    static constexpr int TotalParams = NumBands * ParamsPerBand;

    enum class BandType : int
    {
        LowShelf  = 0,
        Peaking   = 1,
        HighShelf = 2,
        HighPass  = 3,
        LowPass   = 4
    };

    ParametricEQ();
    ~ParametricEQ() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    struct Band
    {
        float    freq    = 1000.0f;
        float    q       = 0.707f;
        float    gainDb  = 0.0f;
        BandType type    = BandType::Peaking;
    };

    void recalcBand(int bandIndex);

    std::array<Band, NumBands> m_bands;
    // One biquad per band per channel (support up to 16 channels)
    static constexpr int MaxChannels = 16;
    std::array<std::array<Biquad, MaxChannels>, NumBands> m_filters;
    float m_sampleRate = 48000.0f;
};

} // namespace dawcast
