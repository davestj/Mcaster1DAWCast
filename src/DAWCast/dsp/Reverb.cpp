// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Reverb.h"

#include <cmath>
#include <algorithm>
#include <numeric>

namespace dawcast {

// ---------- AllPass ----------

void Reverb::AllPass::init(int delaySamples, float fb)
{
    buffer.assign(std::max(1, delaySamples), 0.0f);
    writePos = 0;
    feedback = fb;
}

float Reverb::AllPass::process(float input)
{
    int size = static_cast<int>(buffer.size());
    float delayed = buffer[writePos];
    float out = -input + delayed;
    buffer[writePos] = input + delayed * feedback;
    writePos = (writePos + 1) % size;
    return out;
}

// ---------- DelayLine ----------

void Reverb::DelayLine::init(int delaySamples)
{
    buffer.assign(std::max(1, delaySamples), 0.0f);
    writePos = 0;
}

void Reverb::DelayLine::write(float sample)
{
    int size = static_cast<int>(buffer.size());
    buffer[writePos] = sample;
    writePos = (writePos + 1) % size;
}

float Reverb::DelayLine::read(int delaySamples) const
{
    int size = static_cast<int>(buffer.size());
    int readPos = (writePos - delaySamples + size * 2) % size;
    return buffer[readPos];
}

// ---------- Reverb ----------

Reverb::Reverb()
{
    buildTopology();
}

Reverb::~Reverb() = default;

void Reverb::process(float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    for (int f = 0; f < frames; ++f) {
        // Sum to mono
        float mono = 0.0f;
        for (int c = 0; c < channels; ++c) {
            mono += buffer[f * channels + c];
        }
        mono /= static_cast<float>(channels);

        // Pre-delay
        m_predelay.write(mono);
        int predelaySamples = static_cast<int>(m_predelayMs * 0.001f * m_sampleRate);
        predelaySamples = std::min(predelaySamples, static_cast<int>(m_predelay.buffer.size()) - 1);
        float pdOut = m_predelay.read(std::max(1, predelaySamples));

        // Input diffusion
        float diffused = pdOut;
        for (auto& ap : m_inputDiffusion) {
            diffused = ap.process(diffused);
        }

        // Tank — left branch
        float tankInL = diffused + m_tankDelayR.read(static_cast<int>(m_tankDelayR.buffer.size()) - 1) * m_decay;
        float apOutL = m_tankAPL.process(tankInL);
        // Damping (one-pole low-pass)
        m_dampStateL = m_dampStateL + m_damping * (apOutL - m_dampStateL);
        m_tankDelayL.write(m_dampStateL);

        // Tank — right branch
        float tankInR = diffused + m_tankDelayL.read(static_cast<int>(m_tankDelayL.buffer.size()) - 1) * m_decay;
        float apOutR = m_tankAPR.process(tankInR);
        m_dampStateR = m_dampStateR + m_damping * (apOutR - m_dampStateR);
        m_tankDelayR.write(m_dampStateR);

        // Tap stereo output from tank
        float wetL = m_tankDelayL.read(static_cast<int>(m_tankDelayL.buffer.size()) / 3);
        float wetR = m_tankDelayR.read(static_cast<int>(m_tankDelayR.buffer.size()) / 3);

        // Mix wet/dry and write back to interleaved buffer
        for (int c = 0; c < channels; ++c) {
            float dry = buffer[f * channels + c];
            float wet = (c % 2 == 0) ? wetL : wetR;
            buffer[f * channels + c] = dry * (1.0f - m_wetDryMix) + wet * m_wetDryMix;
        }
    }
}

void Reverb::setParameter(int id, float value)
{
    switch (id) {
    case PredelayMs: m_predelayMs = std::max(0.0f, value); break;
    case Decay:      m_decay      = std::clamp(value, 0.0f, 0.999f); break;
    case Damping:    m_damping    = std::clamp(value, 0.0f, 1.0f); break;
    case Diffusion:
        m_diffusion = std::clamp(value, 0.0f, 1.0f);
        // Update allpass feedback
        for (auto& ap : m_inputDiffusion) ap.feedback = m_diffusion * 0.75f;
        m_tankAPL.feedback = m_diffusion * 0.5f;
        m_tankAPR.feedback = m_diffusion * 0.5f;
        break;
    case WetDryMix:  m_wetDryMix  = std::clamp(value, 0.0f, 1.0f); break;
    default: break;
    }
}

float Reverb::parameter(int id) const
{
    switch (id) {
    case PredelayMs: return m_predelayMs;
    case Decay:      return m_decay;
    case Damping:    return m_damping;
    case Diffusion:  return m_diffusion;
    case WetDryMix:  return m_wetDryMix;
    default:         return 0.0f;
    }
}

QString Reverb::name() const
{
    return QStringLiteral("Plate Reverb");
}

int Reverb::parameterCount() const
{
    return ParamCount;
}

void Reverb::buildTopology()
{
    // Dattorro suggested delay lengths (at 29761 Hz sample rate), scaled to m_sampleRate
    const float scale = m_sampleRate / 29761.0f;

    // Pre-delay: up to 500ms
    m_predelay.init(static_cast<int>(0.5f * m_sampleRate));

    // Input diffusion all-passes
    int inputDelays[] = { 142, 107, 379, 277 };
    for (int i = 0; i < 4; ++i) {
        m_inputDiffusion[i].init(static_cast<int>(inputDelays[i] * scale), m_diffusion * 0.75f);
    }

    // Tank delays and all-passes
    m_tankDelayL.init(static_cast<int>(4453 * scale));
    m_tankAPL.init(static_cast<int>(672 * scale), m_diffusion * 0.5f);

    m_tankDelayR.init(static_cast<int>(4217 * scale));
    m_tankAPR.init(static_cast<int>(908 * scale), m_diffusion * 0.5f);
}

} // namespace dawcast
