/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_mc1_tidemark_b.h — MC1 Tidemark Studios B
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * The smaller mid-room of the Tidemark Studios complex. Drier, more
 * intimate, optimized for vocal R&B / soul / acoustic singer-songwriter
 * recordings. Same vintage tube console + outboard chain as Studio A
 * but with a tighter room signature and a closer chamber send.
 *
 * Composite chain: same as Tidemark A but with smaller room IRs and
 * more intimate mic placement defaults.
 */

#pragma once

#include "dsp_effect.h"
#include "fx_tube_preamp.h"
#include "fx_xenyx_preamp.h"
#include "fx_compressor.h"
#include "fx_parametric_eq.h"
#include "fx_sonic_enhancer.h"
#include "fx_mic_modeler.h"
#include "fx_lexicon_224.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxTidemarkB : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum SourcePosition {
        VocalClose = 0,    // tightest, most intimate
        VocalBack,         // slightly back, more body
        InstrumentSpot,    // for guitar/piano
    };

    enum ParamId {
        ParamSourcePosition = 0,
        ParamMicSelect,
        ParamMicProximity,
        ParamConsoleDrive,
        ParamConsoleEQ,
        ParamCompression,
        ParamRoomTone,
        ParamChamberSend,
        ParamChamberDecay,
        ParamPolish,
        ParamMix,
        ParamOutput,
        kParamCount
    };

    FxTidemarkB()
    {
        m_params[ParamSourcePosition] = 0.0f;
        m_params[ParamMicSelect]      = 0.0f;
        m_params[ParamMicProximity]   = 0.65f;
        m_params[ParamConsoleDrive]   = 0.40f;
        m_params[ParamConsoleEQ]      = 0.50f;
        m_params[ParamCompression]    = 0.55f;
        m_params[ParamRoomTone]       = 0.40f;
        m_params[ParamChamberSend]    = 0.25f;
        m_params[ParamChamberDecay]   = 0.40f;
        m_params[ParamPolish]         = 0.40f;
        m_params[ParamMix]            = 1.0f;
        m_params[ParamOutput]         = 0.667f;
    }

    const char* name()    const override { return "MC1 Tidemark Studios B"; }
    const char* id()      const override { return "mc1.studio.tidemark_b"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;
        m_micModeler.setSampleRate(sr);
        m_tubePreamp.setSampleRate(sr);
        m_xenyx.setSampleRate(sr);
        m_compressor.setSampleRate(sr);
        m_eq.setSampleRate(sr);
        m_chamber.setSampleRate(sr);
        m_enhancer.setSampleRate(sr);
        m_micModeler.setEnabled(true);
        m_tubePreamp.setEnabled(true);
        m_xenyx.setEnabled(true);
        m_compressor.setEnabled(true);
        m_eq.setEnabled(true);
        m_chamber.setEnabled(true);
        m_enhancer.setEnabled(true);
        buildRoomIRs(sr);
        recompute();
    }

    void reset() override
    {
        m_micModeler.reset(); m_tubePreamp.reset(); m_xenyx.reset();
        m_compressor.reset(); m_eq.reset(); m_chamber.reset(); m_enhancer.reset();
        std::fill(m_roomBufL.begin(), m_roomBufL.end(), 0.0f);
        std::fill(m_roomBufR.begin(), m_roomBufR.end(), 0.0f);
        m_roomWriteIdx = 0;
    }

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamSourcePosition: return "Position";
            case ParamMicSelect:      return "Mic";
            case ParamMicProximity:   return "Proximity";
            case ParamConsoleDrive:   return "Console";
            case ParamConsoleEQ:      return "EQ";
            case ParamCompression:    return "Comp";
            case ParamRoomTone:       return "Room";
            case ParamChamberSend:    return "Chamber";
            case ParamChamberDecay:   return "Decay";
            case ParamPolish:         return "Polish";
            case ParamMix:            return "Mix";
            case ParamOutput:         return "Output";
            default:                  return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        return (idx == ParamOutput) ? "dB" : "%";
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
            case ParamSourcePosition: {
                static const char* names[3] = {
                    "Vocal Close", "Vocal Back", "Instrument"
                };
                int i = std::max(0, std::min(2, static_cast<int>(m_params[idx] * 2.999f)));
                return names[i];
            }
            case ParamMicSelect: {
                static const char* names[4] = { "U47", "C12", "SM57", "RIBBON 121" };
                int i = std::max(0, std::min(3, static_cast<int>(m_params[idx] * 3.999f)));
                return names[i];
            }
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
        applyRoomIR(pcm, frames, channels);
        m_micModeler.process(pcm, frames, channels);
        m_tubePreamp.process(pcm, frames, channels);
        m_xenyx.process(pcm, frames, channels);
        m_compressor.process(pcm, frames, channels);
        m_eq.process(pcm, frames, channels);
        m_chamber.process(pcm, frames, channels);
        m_enhancer.process(pcm, frames, channels);
        const float outLin = m_outputLin;
        if (outLin != 1.0f) {
            for (size_t f = 0; f < frames; ++f)
                for (int ch = 0; ch < channels; ++ch)
                    pcm[f * channels + ch] *= outLin;
        }
    }

private:
    void buildRoomIRs(int sr)
    {
        // Smaller, drier IRs (~50 ms) than Tidemark A
        const int irLen = static_cast<int>(0.050 * sr);
        for (int p = 0; p < 3; ++p) {
            m_roomIRs[p].assign(irLen, 0.0f);
            m_roomIRs[p][0] = 1.0f;
        }

        // Vocal Close
        addReflection(m_roomIRs[VocalClose], sr, 0.003f, 0.10f);
        addReflection(m_roomIRs[VocalClose], sr, 0.007f, 0.05f);
        applyExpDecay(m_roomIRs[VocalClose], 0.22f);

        // Vocal Back
        addReflection(m_roomIRs[VocalBack], sr, 0.005f, 0.13f);
        addReflection(m_roomIRs[VocalBack], sr, 0.010f, 0.08f);
        addReflection(m_roomIRs[VocalBack], sr, 0.018f, 0.04f);
        applyExpDecay(m_roomIRs[VocalBack], 0.32f);

        // Instrument Spot
        addReflection(m_roomIRs[InstrumentSpot], sr, 0.006f, 0.14f);
        addReflection(m_roomIRs[InstrumentSpot], sr, 0.012f, 0.09f);
        addReflection(m_roomIRs[InstrumentSpot], sr, 0.022f, 0.05f);
        applyExpDecay(m_roomIRs[InstrumentSpot], 0.40f);

        m_roomBufL.assign(static_cast<size_t>(irLen) + 4, 0.0f);
        m_roomBufR.assign(static_cast<size_t>(irLen) + 4, 0.0f);
        m_roomWriteIdx = 0;
    }

    static void addReflection(std::vector<float>& ir, int sr, float t, float g)
    {
        int idx = static_cast<int>(t * sr);
        if (idx >= 0 && idx < static_cast<int>(ir.size())) ir[idx] += g;
    }

    static void applyExpDecay(std::vector<float>& ir, float rt60)
    {
        const int n = static_cast<int>(ir.size());
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(n);
            ir[i] *= std::exp(-3.0f * t * (0.5f / std::max(0.01f, rt60)));
        }
    }

    void applyRoomIR(float* pcm, size_t frames, int channels)
    {
        if (!m_activeIR || m_activeIR->empty()) return;
        const int irLen = static_cast<int>(m_activeIR->size());
        const int blen  = static_cast<int>(m_roomBufL.size());
        const float* ir = m_activeIR->data();
        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;
            m_roomBufL[m_roomWriteIdx] = inL;
            m_roomBufR[m_roomWriteIdx] = inR;
            float accL = 0.0f, accR = 0.0f;
            int rIdx = m_roomWriteIdx;
            for (int k = 0; k < irLen; ++k) {
                accL += m_roomBufL[rIdx] * ir[k];
                accR += m_roomBufR[rIdx] * ir[k];
                if (--rIdx < 0) rIdx = blen - 1;
            }
            pcm[f * channels + 0] = accL;
            if (channels > 1) pcm[f * channels + 1] = accR;
            if (++m_roomWriteIdx >= blen) m_roomWriteIdx = 0;
        }
    }

    void recompute()
    {
        int posIdx = std::max(0, std::min(2, static_cast<int>(m_params[ParamSourcePosition] * 2.999f)));
        m_activeIR = &m_roomIRs[posIdx];

        int micIdx = std::max(0, std::min(3, static_cast<int>(m_params[ParamMicSelect] * 3.999f)));
        m_micModeler.setParamValue(0, static_cast<float>(micIdx) / 11.0f);
        m_micModeler.setParamValue(1, m_params[ParamMicProximity]);
        for (int i = 2; i < 10; ++i) m_micModeler.setParamValue(i, 0.5f);

        m_tubePreamp.setParamValue(1, m_params[ParamConsoleDrive]);
        m_tubePreamp.setParamValue(2, 0.5f);
        m_tubePreamp.setParamValue(3, 0.5f);
        m_tubePreamp.setParamValue(8, 0.4f);

        m_xenyx.setParamValue(4, m_params[ParamConsoleEQ]);
        m_xenyx.setParamValue(5, m_params[ParamConsoleEQ]);
        m_xenyx.setParamValue(7, m_params[ParamConsoleEQ]);

        m_compressor.setParamValue(1, m_params[ParamCompression]);
        m_compressor.setParamValue(2, m_params[ParamCompression]);
        m_compressor.setParamValue(3, 0.30f);
        m_compressor.setParamValue(4, 0.50f);
        m_compressor.setParamValue(6, 0.55f);

        for (int b = 0; b < 10; ++b) m_eq.setParamValue(b, m_params[ParamConsoleEQ]);

        m_chamber.setParamValue(FxLexicon224::ParamProgram,   0.66f);
        m_chamber.setParamValue(FxLexicon224::ParamDecay,     m_params[ParamChamberDecay]);
        m_chamber.setParamValue(FxLexicon224::ParamMix,       m_params[ParamChamberSend]);

        m_enhancer.setParamValue(1, m_params[ParamPolish]);

        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);
    }

    float m_params[kParamCount] = {};

    FxMicModeler     m_micModeler;
    FxTubePreamp     m_tubePreamp;
    FxXenyxPreamp    m_xenyx;
    FxCompressor     m_compressor;
    FxParametricEq   m_eq;
    FxLexicon224     m_chamber;
    FxSonicEnhancer  m_enhancer;

    std::vector<float> m_roomIRs[3];
    const std::vector<float>* m_activeIR = nullptr;

    std::vector<float> m_roomBufL, m_roomBufR;
    int m_roomWriteIdx = 0;
    float m_outputLin = 1.0f;
};

} // namespace mc1dsp
