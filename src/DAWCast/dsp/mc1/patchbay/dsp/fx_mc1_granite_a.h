/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_mc1_granite_a.h — MC1 Granite Hall Studios A
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Granite Hall Studios A — the converted-machine-works rock studio
 * with the massive live tracking room. Tribute to the legendary
 * Van Nuys-style rock studio sound. Features:
 *
 *   - 5 huge live-room source positions (drum riser, gtr amp left,
 *     gtr amp right, vocal booth, bass DI corner)
 *   - British custom console: 3-band Neve-style EQ + inline
 *     compression (modeled after the famous '70s rock desks)
 *   - Outboard chain: FET limiter (1176-style), Pultec EQ, plate
 *     reverb, and the "secret weapon" Dolby A-style tape expander
 *     for that hi-fi opened-up character on cymbals and vocals
 *   - Reverb chamber that's noticeably bigger / louder than Tidemark
 */

#pragma once

#include "dsp_effect.h"
#include "fx_tube_preamp.h"
#include "fx_xenyx_preamp.h"
#include "fx_compressor.h"
#include "fx_dbx166xs.h"
#include "fx_parametric_eq.h"
#include "fx_sonic_enhancer.h"
#include "fx_mic_modeler.h"
#include "fx_lexicon_480l.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxGraniteA : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum SourcePosition {
        DrumRiser = 0,    // huge live drum sound
        GuitarAmpLeft,    // stack 1 in the corner
        GuitarAmpRight,   // stack 2 in the other corner
        VocalBooth,       // close vocal
        BassDIRoom,       // DI + cab in the room
    };

    enum MicSelection {
        MicU67 = 0,
        MicU47Fet,
        MicSM57,
        MicAKGD12,
        MicColes4038,
        MicRCA44,
    };

    enum ParamId {
        ParamSourcePosition = 0,
        ParamMicSelect,
        ParamMicProximity,
        ParamConsoleDrive,
        ParamConsoleEQ,
        ParamFETLimiter,        // 1176-style
        ParamDolbyA,            // secret weapon expander amount
        ParamRoomTone,
        ParamChamberSend,
        ParamChamberDecay,
        ParamPolish,
        ParamMix,
        ParamOutput,
        kParamCount
    };

    FxGraniteA()
    {
        m_params[ParamSourcePosition] = 0.0f;
        m_params[ParamMicSelect]      = 0.0f;
        m_params[ParamMicProximity]   = 0.45f;
        m_params[ParamConsoleDrive]   = 0.50f;
        m_params[ParamConsoleEQ]      = 0.50f;
        m_params[ParamFETLimiter]     = 0.55f;
        m_params[ParamDolbyA]         = 0.30f;
        m_params[ParamRoomTone]       = 0.65f;
        m_params[ParamChamberSend]    = 0.35f;
        m_params[ParamChamberDecay]   = 0.65f;
        m_params[ParamPolish]         = 0.40f;
        m_params[ParamMix]            = 1.0f;
        m_params[ParamOutput]         = 0.667f;
    }

    const char* name()    const override { return "MC1 Granite Hall Studios A"; }
    const char* id()      const override { return "mc1.studio.granite_a"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        m_micModeler.setSampleRate(sr);
        m_tubePreamp.setSampleRate(sr);
        m_xenyx.setSampleRate(sr);
        m_fetLimiter.setSampleRate(sr);
        m_dbx166.setSampleRate(sr);
        m_eq.setSampleRate(sr);
        m_chamber.setSampleRate(sr);
        m_enhancer.setSampleRate(sr);

        m_micModeler.setEnabled(true);
        m_tubePreamp.setEnabled(true);
        m_xenyx.setEnabled(true);
        m_fetLimiter.setEnabled(true);
        m_dbx166.setEnabled(true);
        m_eq.setEnabled(true);
        m_chamber.setEnabled(true);
        m_enhancer.setEnabled(true);

        buildRoomIRs(sr);
        recompute();
    }

    void reset() override
    {
        m_micModeler.reset(); m_tubePreamp.reset(); m_xenyx.reset();
        m_fetLimiter.reset(); m_dbx166.reset();
        m_eq.reset(); m_chamber.reset(); m_enhancer.reset();
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
            case ParamFETLimiter:     return "1176";
            case ParamDolbyA:         return "Dolby A";
            case ParamRoomTone:       return "Room Tone";
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
                static const char* names[5] = {
                    "Drum Riser", "Gtr Amp L", "Gtr Amp R", "Vocal Booth", "Bass DI Room"
                };
                int i = std::max(0, std::min(4, static_cast<int>(m_params[idx] * 4.999f)));
                return names[i];
            }
            case ParamMicSelect: {
                static const char* names[6] = {
                    "U67", "U47 fet", "SM57", "AKG D12", "Coles 4038", "RCA 44"
                };
                int i = std::max(0, std::min(5, static_cast<int>(m_params[idx] * 5.999f)));
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
        m_fetLimiter.process(pcm, frames, channels);

        // Dolby-A "secret weapon": gentle dbx166 expansion to open up
        // the top end. Only run when the user dials it in.
        if (m_params[ParamDolbyA] > 0.05f) {
            m_dbx166.process(pcm, frames, channels);
        }

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
        // Granite Hall is a BIG room — longer reflections, more density
        const int irLen = static_cast<int>(0.120 * sr);
        for (int p = 0; p < 5; ++p) {
            m_roomIRs[p].assign(irLen, 0.0f);
            m_roomIRs[p][0] = 1.0f;
        }

        // Drum Riser (HUGE room sound)
        addReflection(m_roomIRs[DrumRiser], sr, 0.018f, 0.25f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.032f, 0.18f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.052f, 0.13f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.075f, 0.09f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.098f, 0.06f);
        applyExpDecay(m_roomIRs[DrumRiser], 1.10f);

        // Guitar Amp Left
        addReflection(m_roomIRs[GuitarAmpLeft], sr, 0.011f, 0.22f);
        addReflection(m_roomIRs[GuitarAmpLeft], sr, 0.022f, 0.15f);
        addReflection(m_roomIRs[GuitarAmpLeft], sr, 0.038f, 0.10f);
        addReflection(m_roomIRs[GuitarAmpLeft], sr, 0.060f, 0.07f);
        applyExpDecay(m_roomIRs[GuitarAmpLeft], 0.85f);

        // Guitar Amp Right (mirrored timing)
        addReflection(m_roomIRs[GuitarAmpRight], sr, 0.013f, 0.22f);
        addReflection(m_roomIRs[GuitarAmpRight], sr, 0.024f, 0.15f);
        addReflection(m_roomIRs[GuitarAmpRight], sr, 0.040f, 0.10f);
        addReflection(m_roomIRs[GuitarAmpRight], sr, 0.062f, 0.07f);
        applyExpDecay(m_roomIRs[GuitarAmpRight], 0.85f);

        // Vocal Booth (smaller, drier zone in the same building)
        addReflection(m_roomIRs[VocalBooth], sr, 0.005f, 0.12f);
        addReflection(m_roomIRs[VocalBooth], sr, 0.010f, 0.07f);
        applyExpDecay(m_roomIRs[VocalBooth], 0.30f);

        // Bass DI Room
        addReflection(m_roomIRs[BassDIRoom], sr, 0.014f, 0.20f);
        addReflection(m_roomIRs[BassDIRoom], sr, 0.025f, 0.14f);
        addReflection(m_roomIRs[BassDIRoom], sr, 0.045f, 0.09f);
        applyExpDecay(m_roomIRs[BassDIRoom], 0.70f);

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
        int posIdx = std::max(0, std::min(4, static_cast<int>(m_params[ParamSourcePosition] * 4.999f)));
        m_activeIR = &m_roomIRs[posIdx];

        int micIdx = std::max(0, std::min(5, static_cast<int>(m_params[ParamMicSelect] * 5.999f)));
        m_micModeler.setParamValue(0, static_cast<float>(micIdx) / 11.0f);
        m_micModeler.setParamValue(1, m_params[ParamMicProximity]);
        for (int i = 2; i < 10; ++i) m_micModeler.setParamValue(i, 0.5f);

        m_tubePreamp.setParamValue(1, m_params[ParamConsoleDrive]);
        m_tubePreamp.setParamValue(5, 0.6f);  // transformer (more colored than Tidemark)

        m_xenyx.setParamValue(4, m_params[ParamConsoleEQ]);
        m_xenyx.setParamValue(5, m_params[ParamConsoleEQ]);
        m_xenyx.setParamValue(7, m_params[ParamConsoleEQ]);

        // 1176 FET limiter — fast, aggressive
        m_fetLimiter.setParamValue(1, m_params[ParamFETLimiter]);
        m_fetLimiter.setParamValue(2, std::min(1.0f, m_params[ParamFETLimiter] * 1.2f));
        m_fetLimiter.setParamValue(3, 0.10f);
        m_fetLimiter.setParamValue(4, 0.30f);

        // Dolby A-style expander (FxDbx166xs as the secret weapon)
        m_dbx166.setParamValue(0, 0.30f);
        m_dbx166.setParamValue(1, m_params[ParamDolbyA]);

        for (int b = 0; b < 10; ++b) m_eq.setParamValue(b, m_params[ParamConsoleEQ]);

        // Lexicon 480L Random Hall — Granite gets the bigger reverb
        m_chamber.setParamValue(FxLexicon480L::ParamAlgo,        0.0f);
        m_chamber.setParamValue(FxLexicon480L::ParamRtMid,       m_params[ParamChamberDecay]);
        m_chamber.setParamValue(FxLexicon480L::ParamSize,        0.75f);
        m_chamber.setParamValue(FxLexicon480L::ParamShape,       0.5f);
        m_chamber.setParamValue(FxLexicon480L::ParamSpread,      0.75f);
        m_chamber.setParamValue(FxLexicon480L::ParamPreDelay,    0.10f);
        m_chamber.setParamValue(FxLexicon480L::ParamErTime,      0.40f);
        m_chamber.setParamValue(FxLexicon480L::ParamDiffusion,   0.75f);
        m_chamber.setParamValue(FxLexicon480L::ParamHfCut,       0.40f);
        m_chamber.setParamValue(FxLexicon480L::ParamBassBoost,   0.55f);
        m_chamber.setParamValue(FxLexicon480L::ParamModRate,     0.40f);
        m_chamber.setParamValue(FxLexicon480L::ParamModDepth,    0.40f);
        m_chamber.setParamValue(FxLexicon480L::ParamTailDensity, 0.30f);
        m_chamber.setParamValue(FxLexicon480L::ParamMix,         m_params[ParamChamberSend]);

        m_enhancer.setParamValue(1, m_params[ParamPolish]);

        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);
    }

    float m_params[kParamCount] = {};

    FxMicModeler     m_micModeler;
    FxTubePreamp     m_tubePreamp;
    FxXenyxPreamp    m_xenyx;
    FxCompressor     m_fetLimiter;
    FxDbx166xs       m_dbx166;
    FxParametricEq   m_eq;
    FxLexicon480L    m_chamber;
    FxSonicEnhancer  m_enhancer;

    std::vector<float> m_roomIRs[5];
    const std::vector<float>* m_activeIR = nullptr;

    std::vector<float> m_roomBufL, m_roomBufR;
    int m_roomWriteIdx = 0;
    float m_outputLin = 1.0f;
};

} // namespace mc1dsp
