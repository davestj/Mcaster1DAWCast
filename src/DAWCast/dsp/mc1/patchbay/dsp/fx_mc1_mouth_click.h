/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_mouth_click.h — MC1 Mouth Click Remover
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Real-time mouth click / lip smack / saliva noise remover.
 *
 * Mouth clicks are short (typically 1–4 ms) high-frequency transients
 * with very fast attack and decay, distinct from speech transients
 * because their spectral centroid sits above 4 kHz with no fundamental.
 *
 * Algorithm:
 *   1. Lookahead delay (configurable 5–30 ms) so we can catch a click
 *      before it hits the output.
 *   2. High-pass detector (4 kHz +) tracks instantaneous energy in the
 *      click band.
 *   3. Speech-band tracker (300–2 kHz) gives a reference for "this is
 *      speech, not a click" via spectral ratio.
 *   4. When the click-band energy spikes faster than the speech band
 *      can follow AND the ratio exceeds threshold, we apply a short
 *      gain duck (typically 2–6 ms) to the delayed sample stream.
 *   5. The duck has a smooth raised-cosine shape so the cut is
 *      inaudible.
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxMouthClickRemover : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ParamId {
        ParamSensitivity = 0,  // 0..1 → click detection ratio threshold
        ParamDepth,            // 0..1 → 0..30 dB reduction
        ParamLookahead,        // 0..1 → 5..30 ms lookahead
        ParamDuckLength,       // 0..1 → 1..8 ms duck length
        ParamHfBand,           // 0..1 → 3..8 kHz click detection center
        ParamMix,              // 0..1
        kParamCount
    };

    FxMouthClickRemover()
    {
        m_params[ParamSensitivity] = 0.55f;
        m_params[ParamDepth]       = 0.70f;
        m_params[ParamLookahead]   = 0.40f;
        m_params[ParamDuckLength]  = 0.40f;
        m_params[ParamHfBand]      = 0.40f;
        m_params[ParamMix]         = 1.0f;
    }

    const char* name()    const override { return "MC1 Mouth Click Remover"; }
    const char* id()      const override { return "mc1.podcast.mouth_click"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        // Lookahead buffer (max 30 ms × 2 channels)
        int laMax = static_cast<int>(0.030 * sr) + 16;
        m_laBufL.assign(laMax, 0.0f);
        m_laBufR.assign(laMax, 0.0f);
        m_laWriteIdx = 0;

        m_clickEnvL = 0.0f;
        m_speechEnvL = 0.0f;
        m_clickHpStateL = 0.0f;
        m_speechBpStateL = 0.0f;
        m_duckGain = 1.0f;
        m_duckSamplesRemaining = 0;
        recompute();
    }

    void reset() override
    {
        std::fill(m_laBufL.begin(), m_laBufL.end(), 0.0f);
        std::fill(m_laBufR.begin(), m_laBufR.end(), 0.0f);
        m_laWriteIdx = 0;
        m_clickEnvL = m_speechEnvL = 0.0f;
        m_clickHpStateL = m_speechBpStateL = 0.0f;
        m_duckGain = 1.0f;
        m_duckSamplesRemaining = 0;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamSensitivity: return "Sensitivity";
            case ParamDepth:       return "Depth";
            case ParamLookahead:   return "Lookahead";
            case ParamDuckLength:  return "Duck Length";
            case ParamHfBand:      return "HF Band";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamDepth:      return "dB";
            case ParamLookahead:  return "ms";
            case ParamDuckLength: return "ms";
            case ParamHfBand:     return "kHz";
            default:              return "%";
        }
    }

    float paramValue(int idx) const override
    {
        return (idx >= 0 && idx < kParamCount) ? m_params[idx] : 0.0f;
    }

    void setParamValue(int idx, float v) override
    {
        if (idx < 0 || idx >= kParamCount) return;
        m_params[idx] = std::max(0.0f, std::min(1.0f, v));
        recompute();
    }

    std::string paramDisplayValue(int idx) const override
    {
        char buf[32];
        switch (idx) {
            case ParamSensitivity:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
            case ParamDepth:
                std::snprintf(buf, sizeof(buf), "-%.1f dB", m_params[idx] * 30.0f);
                return buf;
            case ParamLookahead:
                std::snprintf(buf, sizeof(buf), "%.1f ms", 5.0f + m_params[idx] * 25.0f);
                return buf;
            case ParamDuckLength:
                std::snprintf(buf, sizeof(buf), "%.1f ms", 1.0f + m_params[idx] * 7.0f);
                return buf;
            case ParamHfBand:
                std::snprintf(buf, sizeof(buf), "%.1f kHz", 3.0f + m_params[idx] * 5.0f);
                return buf;
            case ParamMix:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
        return "";
    }

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0 || m_laBufL.empty()) return;

        const float threshold = 1.5f + (1.0f - m_params[ParamSensitivity]) * 4.0f;
        const float duckMin   = std::pow(10.0f, -m_params[ParamDepth] * 30.0f / 20.0f);
        const float clickHpC  = m_clickHpCoef;
        const float speechBpC = m_speechBpCoef;
        const float mix       = m_params[ParamMix];
        const int   laLen     = m_laSamples;
        const int   laBuflen  = static_cast<int>(m_laBufL.size());

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            float mono = 0.5f * (inL + inR);

            // Click-band detector (high-pass)
            m_clickHpStateL = m_clickHpStateL * (1.0f - clickHpC) + mono * clickHpC;
            float clickBand = mono - m_clickHpStateL;
            float clickMag  = std::fabs(clickBand);

            // Speech-band detector (band-pass-ish via leaky integrator)
            m_speechBpStateL = m_speechBpStateL * (1.0f - speechBpC) + mono * speechBpC;
            float speechMag  = std::fabs(m_speechBpStateL);

            // Fast attack envelopes
            if (clickMag > m_clickEnvL)
                m_clickEnvL += (clickMag - m_clickEnvL) * 0.50f;
            else
                m_clickEnvL += (clickMag - m_clickEnvL) * 0.10f;
            if (speechMag > m_speechEnvL)
                m_speechEnvL += (speechMag - m_speechEnvL) * 0.05f;
            else
                m_speechEnvL += (speechMag - m_speechEnvL) * 0.005f;

            // Ratio: click energy >> speech energy = mouth click
            float ratio = m_clickEnvL / (m_speechEnvL + 1e-6f);
            if (m_duckSamplesRemaining == 0 && ratio > threshold && m_clickEnvL > 0.005f) {
                m_duckSamplesRemaining = m_duckLengthSamples;
            }

            // Compute duck gain envelope (raised cosine fade)
            float gain = 1.0f;
            if (m_duckSamplesRemaining > 0) {
                float t = 1.0f - (static_cast<float>(m_duckSamplesRemaining) /
                                  static_cast<float>(m_duckLengthSamples));
                // Raised cosine: full duck at center, ramps in/out
                float win = 0.5f * (1.0f - std::cos(t * 6.28318530718f));
                gain = 1.0f - (1.0f - duckMin) * win;
                m_duckSamplesRemaining--;
            }

            // Write to lookahead buffer
            m_laBufL[m_laWriteIdx] = inL;
            m_laBufR[m_laWriteIdx] = inR;

            // Read delayed sample and apply duck gain
            int readIdx = m_laWriteIdx - laLen;
            if (readIdx < 0) readIdx += laBuflen;
            float dL = m_laBufL[readIdx] * gain;
            float dR = m_laBufR[readIdx] * gain;

            if (++m_laWriteIdx >= laBuflen) m_laWriteIdx = 0;

            pcm[f * channels + 0] = inL * (1.0f - mix) + dL * mix;
            if (channels > 1)
                pcm[f * channels + 1] = inR * (1.0f - mix) + dR * mix;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);

        m_laSamples = static_cast<int>((5.0f + m_params[ParamLookahead] * 25.0f) * 0.001f * fs);
        if (m_laSamples < 1) m_laSamples = 1;
        if (m_laSamples >= static_cast<int>(m_laBufL.size()))
            m_laSamples = static_cast<int>(m_laBufL.size()) - 1;

        m_duckLengthSamples = static_cast<int>((1.0f + m_params[ParamDuckLength] * 7.0f) * 0.001f * fs);
        if (m_duckLengthSamples < 1) m_duckLengthSamples = 1;

        float clickHz  = (3.0f + m_params[ParamHfBand] * 5.0f) * 1000.0f;
        float speechHz = 1500.0f;
        m_clickHpCoef  = 1.0f - std::exp(-2.0f * 3.14159265359f * clickHz / fs);
        m_speechBpCoef = 1.0f - std::exp(-2.0f * 3.14159265359f * speechHz / fs);
    }

    float m_params[kParamCount] = {};

    std::vector<float> m_laBufL, m_laBufR;
    int   m_laWriteIdx = 0;
    int   m_laSamples = 1;

    float m_clickHpStateL = 0.0f;
    float m_speechBpStateL = 0.0f;
    float m_clickEnvL = 0.0f;
    float m_speechEnvL = 0.0f;
    float m_clickHpCoef = 0.0f;
    float m_speechBpCoef = 0.0f;

    float m_duckGain = 1.0f;
    int   m_duckLengthSamples = 1;
    int   m_duckSamplesRemaining = 0;
};

} // namespace mc1dsp
