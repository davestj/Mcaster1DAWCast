// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GraphicEQ31.h"

namespace dawcast {

GraphicEQ31::GraphicEQ31()
{
    m_gains.fill(0.0f);
    for (int i = 0; i < NumBands; ++i) {
        recalcBand(i);
    }
}

GraphicEQ31::~GraphicEQ31() = default;

void GraphicEQ31::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const int ch = std::min(channels, MaxChannels);

    for (int band = 0; band < NumBands; ++band) {
        if (m_gains[band] == 0.0f) continue; // skip unity bands
        for (int f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float& sample = buffer[f * channels + c];
                sample = m_filters[band][c].process(sample);
            }
        }
    }
}

void GraphicEQ31::setParameter(int id, float value)
{
    if (id < 0 || id >= NumBands) return;
    m_gains[id] = value;
    recalcBand(id);
}

float GraphicEQ31::parameter(int id) const
{
    if (id < 0 || id >= NumBands) return 0.0f;
    return m_gains[id];
}

QString GraphicEQ31::name() const
{
    return QStringLiteral("31-Band Graphic EQ");
}

int GraphicEQ31::parameterCount() const
{
    return NumBands;
}

void GraphicEQ31::recalcBand(int bandIndex)
{
    auto c = Biquad::peaking(CenterFreqs[bandIndex], DefaultQ,
                             m_gains[bandIndex], m_sampleRate);
    for (int ch = 0; ch < MaxChannels; ++ch) {
        m_filters[bandIndex][ch].setCoeffs(c);
    }
}

} // namespace dawcast
