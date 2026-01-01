// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ParametricEQ.h"

namespace dawcast {

ParametricEQ::ParametricEQ()
{
    // Default centre frequencies spread across the spectrum
    const float defaultFreqs[NumBands] = {
        31.0f, 63.0f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };
    for (int i = 0; i < NumBands; ++i) {
        m_bands[i].freq = defaultFreqs[i];
        recalcBand(i);
    }
}

ParametricEQ::~ParametricEQ() = default;

void ParametricEQ::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    const int ch = std::min(channels, MaxChannels);

    for (int band = 0; band < NumBands; ++band) {
        for (int f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float& sample = buffer[f * channels + c];
                sample = m_filters[band][c].process(sample);
            }
        }
    }
}

void ParametricEQ::setParameter(int id, float value)
{
    if (id < 0 || id >= TotalParams) return;

    const int band  = id / ParamsPerBand;
    const int param = id % ParamsPerBand;

    switch (param) {
    case 0: m_bands[band].freq   = value; break;
    case 1: m_bands[band].q      = value; break;
    case 2: m_bands[band].gainDb = value; break;
    case 3: m_bands[band].type   = static_cast<BandType>(static_cast<int>(value)); break;
    }
    recalcBand(band);
}

float ParametricEQ::parameter(int id) const
{
    if (id < 0 || id >= TotalParams) return 0.0f;

    const int band  = id / ParamsPerBand;
    const int param = id % ParamsPerBand;

    switch (param) {
    case 0: return m_bands[band].freq;
    case 1: return m_bands[band].q;
    case 2: return m_bands[band].gainDb;
    case 3: return static_cast<float>(static_cast<int>(m_bands[band].type));
    }
    return 0.0f;
}

QString ParametricEQ::name() const
{
    return QStringLiteral("Parametric EQ");
}

int ParametricEQ::parameterCount() const
{
    return TotalParams;
}

void ParametricEQ::recalcBand(int bandIndex)
{
    const auto& b = m_bands[bandIndex];
    BiquadCoeffs c;

    switch (b.type) {
    case BandType::LowShelf:
        c = Biquad::lowshelf(b.freq, b.q, b.gainDb, m_sampleRate);
        break;
    case BandType::Peaking:
        c = Biquad::peaking(b.freq, b.q, b.gainDb, m_sampleRate);
        break;
    case BandType::HighShelf:
        c = Biquad::highshelf(b.freq, b.q, b.gainDb, m_sampleRate);
        break;
    case BandType::HighPass:
        c = Biquad::highpass(b.freq, b.q, m_sampleRate);
        break;
    case BandType::LowPass:
        c = Biquad::lowpass(b.freq, b.q, m_sampleRate);
        break;
    }

    for (int ch = 0; ch < MaxChannels; ++ch) {
        m_filters[bandIndex][ch].setCoeffs(c);
    }
}

} // namespace dawcast
