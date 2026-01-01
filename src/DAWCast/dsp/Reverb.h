// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IEffectUnit.h"

#include <vector>
#include <array>

namespace dawcast {

// Dattorro plate reverb algorithm.
// Mono-in / stereo-out internally; multi-channel input is summed to mono,
// then the stereo reverb output is distributed across output channels.

class Reverb : public IEffectUnit
{
public:
    enum Param
    {
        PredelayMs = 0,
        Decay,
        Damping,
        Diffusion,
        WetDryMix,
        ParamCount
    };

    Reverb();
    ~Reverb() override;

    // IEffectUnit
    void   process(float* buffer, int frames, int channels) override;
    void   setParameter(int id, float value) override;
    float  parameter(int id) const override;
    QString name() const override;
    int    parameterCount() const override;

private:
    // --- Dattorro topology delay lines and state ---

    // All-pass diffusor
    struct AllPass
    {
        std::vector<float> buffer;
        int writePos = 0;
        float feedback = 0.0f;

        void init(int delaySamples, float fb);
        float process(float input);
    };

    // Simple delay line
    struct DelayLine
    {
        std::vector<float> buffer;
        int writePos = 0;

        void init(int delaySamples);
        void write(float sample);
        float read(int delaySamples) const;
    };

    void buildTopology();

    float m_predelayMs = 20.0f;
    float m_decay      = 0.5f;  // 0..1
    float m_damping    = 0.5f;  // 0..1
    float m_diffusion  = 0.7f;  // 0..1
    float m_wetDryMix  = 0.3f;  // 0 = fully dry, 1 = fully wet

    float m_sampleRate = 48000.0f;

    // Pre-delay
    DelayLine m_predelay;

    // Input diffusion (4 all-passes)
    std::array<AllPass, 4> m_inputDiffusion;

    // Tank: two parallel branches, each with delay + allpass + damping
    // Left branch
    DelayLine m_tankDelayL;
    AllPass   m_tankAPL;
    float     m_dampStateL = 0.0f;

    // Right branch
    DelayLine m_tankDelayR;
    AllPass   m_tankAPR;
    float     m_dampStateR = 0.0f;
};

} // namespace dawcast
