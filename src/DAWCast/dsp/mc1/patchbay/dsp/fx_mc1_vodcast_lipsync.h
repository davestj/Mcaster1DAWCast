/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * dsp/fx_mc1_vodcast_lipsync.h — MC1 Vodcast Lipsync
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Audio-to-video drift corrector for vodcast (video podcast) sessions.
 *
 * When a vodcaster records audio on a separate device from video
 * (USB mic + camera, for example), the two devices have slightly
 * different sample clocks. Over a 2-hour show the audio can drift
 * by 50–500 ms relative to video. This plugin applies a tiny,
 * continuously varying delay (or pull-up / pull-down via fractional
 * resampling) to the audio so it stays locked to the video frame
 * grid.
 *
 * Two modes:
 *   Static Offset — fixed audio delay/advance (manual ms value)
 *   Drift Correct — slow continuous resampling that compensates
 *                    a per-second drift rate (e.g. -3 ms/min)
 *
 * The drift rate is applied as a tiny fractional sample-rate
 * adjustment using linear interpolation between input samples.
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxVodcastLipsync : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum Mode { StaticOffset = 0, DriftCorrect };

    enum ParamId {
        ParamMode = 0,        // 0..1
        ParamOffset,          // 0..1 → -250..+250 ms static offset
        ParamDriftRate,       // 0..1 → -10..+10 ms/min drift rate
        ParamSmoothTime,      // 0..1 → 50..1000 ms smoothing time
        ParamMix,             // 0..1
        kParamCount
    };

    FxVodcastLipsync()
    {
        m_params[ParamMode]       = 0.0f;   // Static
        m_params[ParamOffset]     = 0.5f;   // 0 ms
        m_params[ParamDriftRate]  = 0.5f;   // 0 ms/min
        m_params[ParamSmoothTime] = 0.5f;
        m_params[ParamMix]        = 1.0f;
    }

    const char* name()    const override { return "MC1 Vodcast Lipsync"; }
    const char* id()      const override { return "mc1.podcast.vodcast_lipsync"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Utility; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        // Buffer holds ±300 ms of audio so we can advance or delay
        int bufLen = static_cast<int>(0.6 * sr) + 64;
        m_bufL.assign(bufLen, 0.0f);
        m_bufR.assign(bufLen, 0.0f);
        m_writeIdx = 0;
        m_readPos = static_cast<double>(bufLen) * 0.5;
        m_currentDelay = static_cast<double>(bufLen) * 0.5;
        m_targetDelay = m_currentDelay;
        recompute();
    }

    void reset() override
    {
        std::fill(m_bufL.begin(), m_bufL.end(), 0.0f);
        std::fill(m_bufR.begin(), m_bufR.end(), 0.0f);
        m_writeIdx = 0;
        m_readPos = static_cast<double>(m_bufL.size()) * 0.5;
        m_currentDelay = m_readPos;
        m_targetDelay = m_currentDelay;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamMode:        return "Mode";
            case ParamOffset:      return "Offset";
            case ParamDriftRate:   return "Drift Rate";
            case ParamSmoothTime:  return "Smooth Time";
            case ParamMix:         return "Mix";
            default:               return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamOffset:     return "ms";
            case ParamDriftRate:  return "ms/min";
            case ParamSmoothTime: return "ms";
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
            case ParamMode:
                return (m_params[idx] >= 0.5f) ? "Drift Correct" : "Static Offset";
            case ParamOffset:
                std::snprintf(buf, sizeof(buf), "%+.0f ms", -250.0f + m_params[idx] * 500.0f);
                return buf;
            case ParamDriftRate:
                std::snprintf(buf, sizeof(buf), "%+.1f ms/min", -10.0f + m_params[idx] * 20.0f);
                return buf;
            case ParamSmoothTime:
                std::snprintf(buf, sizeof(buf), "%.0f ms", 50.0f + m_params[idx] * 950.0f);
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
        if (channels < 1 || frames == 0 || m_bufL.empty()) return;

        const bool   drift     = m_params[ParamMode] >= 0.5f;
        const float  smoothC   = m_smoothCoef;
        const double driftStep = m_driftStepSamples;
        const float  mix       = m_params[ParamMix];
        const int    bufLen    = static_cast<int>(m_bufL.size());

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            // Write input
            m_bufL[m_writeIdx] = inL;
            m_bufR[m_writeIdx] = inR;

            // Smooth current delay toward target
            m_currentDelay += (m_targetDelay - m_currentDelay) * smoothC;
            if (drift) {
                m_targetDelay += driftStep;
            }

            // Read with linear interpolation
            double rp = static_cast<double>(m_writeIdx) - m_currentDelay;
            while (rp < 0)        rp += bufLen;
            while (rp >= bufLen)  rp -= bufLen;
            int    i0 = static_cast<int>(rp);
            int    i1 = (i0 + 1) % bufLen;
            float  fr = static_cast<float>(rp - i0);
            float  outL = m_bufL[i0] * (1.0f - fr) + m_bufL[i1] * fr;
            float  outR = m_bufR[i0] * (1.0f - fr) + m_bufR[i1] * fr;

            if (++m_writeIdx >= bufLen) m_writeIdx = 0;

            pcm[f * channels + 0] = inL * (1.0f - mix) + outL * mix;
            if (channels > 1)
                pcm[f * channels + 1] = inR * (1.0f - mix) + outR * mix;
        }
    }

private:
    void recompute()
    {
        if (sampleRate_ <= 0) return;
        const float fs = static_cast<float>(sampleRate_);
        const int   bufLen = static_cast<int>(m_bufL.size());

        // Static offset target (signed)
        float offsetMs = -250.0f + m_params[ParamOffset] * 500.0f;
        double offsetSamples = static_cast<double>(offsetMs) * 0.001 * static_cast<double>(fs);
        m_targetDelay = static_cast<double>(bufLen) * 0.5 + offsetSamples;
        if (m_targetDelay < 1.0)            m_targetDelay = 1.0;
        if (m_targetDelay > bufLen - 2.0)   m_targetDelay = bufLen - 2.0;

        // Drift rate (samples per sample)
        float driftMsPerMin = -10.0f + m_params[ParamDriftRate] * 20.0f;
        double driftSamplesPerMin = static_cast<double>(driftMsPerMin) * 0.001 * static_cast<double>(fs);
        m_driftStepSamples = driftSamplesPerMin / (60.0 * static_cast<double>(fs));

        float smoothMs = 50.0f + m_params[ParamSmoothTime] * 950.0f;
        m_smoothCoef = 1.0f - std::exp(-1.0f / (smoothMs * 0.001f * fs));
    }

    float m_params[kParamCount] = {};
    std::vector<float> m_bufL, m_bufR;
    int    m_writeIdx = 0;
    double m_readPos = 0.0;
    double m_currentDelay = 0.0;
    double m_targetDelay = 0.0;
    double m_driftStepSamples = 0.0;
    float  m_smoothCoef = 0.001f;
};

} // namespace mc1dsp
