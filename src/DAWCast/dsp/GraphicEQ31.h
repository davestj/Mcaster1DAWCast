// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"
#include "dsp/Biquad.h"

#include <array>

namespace dawcast {

class GraphicEQ31 : public IEffectUnit
{
public:
    static constexpr int NumBands = 31;

    GraphicEQ31();
    ~GraphicEQ31() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    void recalcBand(int bandIndex);

    // ISO 1/3-octave centre frequencies from 20 Hz to 20 kHz
    static constexpr float CenterFreqs[NumBands] = {
        20.0f,    25.0f,    31.5f,   40.0f,    50.0f,    63.0f,    80.0f,
        100.0f,   125.0f,   160.0f,  200.0f,   250.0f,   315.0f,   400.0f,
        500.0f,   630.0f,   800.0f,  1000.0f,  1250.0f,  1600.0f,  2000.0f,
        2500.0f,  3150.0f,  4000.0f, 5000.0f,  6300.0f,  8000.0f, 10000.0f,
        12500.0f, 16000.0f, 20000.0f
    };

    static constexpr float DefaultQ = 4.318f; // ~1/3-octave bandwidth

    std::array<float, NumBands> m_gains{}; // dB per band

    static constexpr int MaxChannels = 16;
    std::array<std::array<Biquad, MaxChannels>, NumBands> m_filters;
    float m_sampleRate = 48000.0f;
};

} // namespace dawcast
