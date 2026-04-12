/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_mc1_tidemark_vault.h — MC1 Tidemark Vault Chambers
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Tidemark's underground brick reverb chambers — three physical
 * chambers under the studio that are sent to via tape during
 * recording. Each chamber has its own dimensions, brick coverage,
 * and damping characteristics.
 *
 * This is a *pure reverb* plugin (no console, no mic stage). Drop
 * it on a send bus or as the only effect on a track to recreate
 * the famous Putnam-style chamber sound.
 *
 * Internally drives the Lexicon 224 plate algorithm with three
 * baked profile presets — Chamber A (long bright), Chamber B
 * (short dark), Chamber C (medium balanced) — plus a multi-tap
 * pre-delay that approximates the tape send delay used to feed the
 * physical rooms.
 */

#pragma once

#include "dsp_effect.h"
#include "fx_lexicon_224.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxTidemarkVault : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum ChamberType {
        ChamberA = 0,    // long bright (vocal favorite)
        ChamberB,        // short dark (drums / percussion)
        ChamberC,        // medium balanced (mix bus)
    };

    enum ParamId {
        ParamChamber = 0,    // 0..1 → A/B/C
        ParamDecay,          // 0..1 → 0.5..6 s
        ParamPreDelay,       // 0..1 → 0..200 ms
        ParamSize,           // 0..1 → chamber dimensions
        ParamDamping,        // 0..1 → HF damping
        ParamLowCut,         // 0..1 → 20..400 Hz
        ParamMix,            // 0..1
        ParamOutput,         // 0..1 → -12..+6 dB
        kParamCount
    };

    FxTidemarkVault()
    {
        m_params[ParamChamber]   = 0.0f;
        m_params[ParamDecay]     = 0.55f;
        m_params[ParamPreDelay]  = 0.10f;
        m_params[ParamSize]      = 0.55f;
        m_params[ParamDamping]   = 0.40f;
        m_params[ParamLowCut]    = 0.15f;
        m_params[ParamMix]       = 0.35f;
        m_params[ParamOutput]    = 0.667f;
    }

    const char* name()    const override { return "MC1 Tidemark Vault Chambers"; }
    const char* id()      const override { return "mc1.studio.tidemark_vault"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        m_chamber.setSampleRate(sr);
        m_chamber.setEnabled(true);
        recompute();
    }

    void reset() override { m_chamber.reset(); }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamChamber:  return "Chamber";
            case ParamDecay:    return "Decay";
            case ParamPreDelay: return "Pre Delay";
            case ParamSize:     return "Size";
            case ParamDamping:  return "Damping";
            case ParamLowCut:   return "Low Cut";
            case ParamMix:      return "Mix";
            case ParamOutput:   return "Output";
            default:            return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamDecay:    return "s";
            case ParamPreDelay: return "ms";
            case ParamLowCut:   return "Hz";
            case ParamOutput:   return "dB";
            default:            return "%";
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
            case ParamChamber: {
                static const char* names[3] = {
                    "Chamber A — Long Bright",
                    "Chamber B — Short Dark",
                    "Chamber C — Medium",
                };
                int i = std::max(0, std::min(2, static_cast<int>(m_params[idx] * 2.999f)));
                return names[i];
            }
            case ParamDecay:
                std::snprintf(buf, sizeof(buf), "%.2f s", 0.5f + m_params[idx] * 5.5f);
                return buf;
            case ParamPreDelay:
                std::snprintf(buf, sizeof(buf), "%.1f ms", m_params[idx] * 200.0f);
                return buf;
            case ParamLowCut:
                std::snprintf(buf, sizeof(buf), "%.0f Hz", 20.0f + m_params[idx] * 380.0f);
                return buf;
            case ParamOutput:
                std::snprintf(buf, sizeof(buf), "%+.1f dB", -12.0f + m_params[idx] * 18.0f);
                return buf;
            default:
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
        }
    }

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;
        m_chamber.process(pcm, frames, channels);

        const float outLin = m_outputLin;
        if (outLin != 1.0f) {
            for (size_t f = 0; f < frames; ++f)
                for (int ch = 0; ch < channels; ++ch)
                    pcm[f * channels + ch] *= outLin;
        }
    }

private:
    void recompute()
    {
        int chamberIdx = std::max(0, std::min(2, static_cast<int>(m_params[ParamChamber] * 2.999f)));

        // Each chamber maps to a different Lexicon 224 program + decay multiplier
        float program = 0.0f;
        float decayMul = 1.0f;
        float dampMul  = 1.0f;
        float bassMul  = 0.5f;
        float trebMul  = 0.55f;
        switch (chamberIdx) {
            case ChamberA:  // Hall A — long bright
                program = 0.0f;
                decayMul = 1.20f;
                dampMul  = 0.60f;
                bassMul  = 0.45f;
                trebMul  = 0.75f;
                break;
            case ChamberB:  // Plate — short dark
                program = 0.66f;
                decayMul = 0.55f;
                dampMul  = 0.85f;
                bassMul  = 0.65f;
                trebMul  = 0.30f;
                break;
            case ChamberC:  // Hall B — medium balanced
                program = 0.33f;
                decayMul = 0.85f;
                dampMul  = 0.55f;
                bassMul  = 0.55f;
                trebMul  = 0.55f;
                break;
        }

        m_chamber.setParamValue(FxLexicon224::ParamProgram,    program);
        m_chamber.setParamValue(FxLexicon224::ParamPreDelay,   m_params[ParamPreDelay]);
        m_chamber.setParamValue(FxLexicon224::ParamDecay,      std::min(1.0f, m_params[ParamDecay] * decayMul));
        m_chamber.setParamValue(FxLexicon224::ParamSize,       m_params[ParamSize]);
        m_chamber.setParamValue(FxLexicon224::ParamDiffusion,  0.75f);
        m_chamber.setParamValue(FxLexicon224::ParamHfDamping,  std::min(1.0f, m_params[ParamDamping] * dampMul));
        m_chamber.setParamValue(FxLexicon224::ParamLfCut,      m_params[ParamLowCut]);
        m_chamber.setParamValue(FxLexicon224::ParamBassMult,   bassMul);
        m_chamber.setParamValue(FxLexicon224::ParamTrebleDecay,trebMul);
        m_chamber.setParamValue(FxLexicon224::ParamModDepth,   0.30f);
        m_chamber.setParamValue(FxLexicon224::ParamMix,        m_params[ParamMix]);

        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);
    }

    float m_params[kParamCount] = {};
    FxLexicon224 m_chamber;
    float m_outputLin = 1.0f;
};

} // namespace mc1dsp
