// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

namespace dawcast {

// Digital glitch / artifact VJ-style transition effect.

class GlitchCut : public IVideoEffect
{
public:
    enum Param
    {
        Intensity = 0,   // 0..1 — how severe the glitch
        GlitchFrames,    // number of glitch slices
        ArtifactMix,     // 0..1 — blend of artifact overlay
        ParamCount
    };

    GlitchCut();
    ~GlitchCut() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;

    void setIntensity(float v) { m_intensity = v; }
    void setGlitchFrames(int n) { m_glitchFrames = n; }
    void setArtifactMix(float v) { m_artifactMix = v; }

private:
    float m_intensity    = 0.5f;
    int   m_glitchFrames = 8;
    float m_artifactMix  = 0.3f;

    // Simple pseudo-random for deterministic glitch patterns
    unsigned int m_seed = 42;
    unsigned int nextRand();
};

} // namespace dawcast
