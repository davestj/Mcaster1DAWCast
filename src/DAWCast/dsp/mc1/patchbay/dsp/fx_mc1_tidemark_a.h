/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_mc1_tidemark_a.h — MC1 Tidemark Studios A
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Tribute to the legendary West Coast tracking rooms — large live
 * floor with vintage tube console, multi-position source placement,
 * Putnam-style chamber sends, and the classic outboard chain that
 * defined the sound of '70s/'80s pop, soul, and singer-songwriter
 * records.
 *
 * Composite DSP chain:
 *   Input
 *     → Live Room IR convolver       (5 source positions in the room)
 *     → Mic Stage                    (multi-mic locker via FxMicModeler)
 *     → Tube Console Preamp          (FxTubePreamp + FxXenyxPreamp tone stack)
 *     → Outboard Chain
 *         · MC1 Compressor            (1176-style fast comp)
 *         · MC1 Parametric EQ         (Pultec-style 4-band)
 *     → Chamber Send                 (Lexicon 224 Plate algorithm)
 *     → MC1 Sonic Enhancer           (BBE polish)
 *     → Output
 *
 * Real-time safe.
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

class FxTidemarkA : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum SourcePosition {
        DrumRiser = 0,    // back of room, lots of natural ambience
        VocalBooth,       // close, dry, vocal-focused
        LiveFloorFront,   // mid-room, balanced
        LiveFloorRear,    // back of room, big space
        IsoBooth,         // tight isolation
    };

    enum MicSelection {
        MicU47 = 0,       // tube condenser, warm vocal
        MicC12,           // tube condenser, bright airy
        MicSM57,          // dynamic, drum/guitar workhorse
        MicRibbon121,     // ribbon, smooth
    };

    enum ParamId {
        ParamSourcePosition = 0,  // 0..1 → enum
        ParamMicSelect,           // 0..1 → enum
        ParamMicProximity,        // 0..1 → distance from source
        ParamConsoleDrive,        // 0..1 → tube console saturation amount
        ParamConsoleEQ,           // 0..1 → 3-band Pultec macro
        ParamCompression,         // 0..1 → 1176-style comp amount
        ParamRoomTone,            // 0..1 → balance of close vs room mics
        ParamChamberSend,         // 0..1 → Putnam chamber wet send
        ParamChamberDecay,        // 0..1 → chamber RT60
        ParamPolish,              // 0..1 → BBE enhancer
        ParamMix,                 // 0..1
        ParamOutput,              // 0..1 → -12..+6 dB
        kParamCount
    };

    FxTidemarkA()
    {
        m_params[ParamSourcePosition] = 0.4f;   // LiveFloorFront
        m_params[ParamMicSelect]      = 0.0f;   // U47
        m_params[ParamMicProximity]   = 0.55f;
        m_params[ParamConsoleDrive]   = 0.45f;
        m_params[ParamConsoleEQ]      = 0.50f;
        m_params[ParamCompression]    = 0.50f;
        m_params[ParamRoomTone]       = 0.50f;
        m_params[ParamChamberSend]    = 0.30f;
        m_params[ParamChamberDecay]   = 0.55f;
        m_params[ParamPolish]         = 0.40f;
        m_params[ParamMix]            = 1.0f;
        m_params[ParamOutput]         = 0.667f;
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "MC1 Tidemark Studios A"; }
    const char* id()      const override { return "mc1.studio.tidemark_a"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    /* ── Configuration ───────────────────────────────────────────── */

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
        m_micModeler.reset();
        m_tubePreamp.reset();
        m_xenyx.reset();
        m_compressor.reset();
        m_eq.reset();
        m_chamber.reset();
        m_enhancer.reset();
        std::fill(m_roomBufL.begin(), m_roomBufL.end(), 0.0f);
        std::fill(m_roomBufR.begin(), m_roomBufR.end(), 0.0f);
        m_roomWriteIdx = 0;
    }

    /* ── Parameters ──────────────────────────────────────────────── */

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
        if (idx == ParamOutput) return "dB";
        return "%";
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
                    "Drum Riser", "Vocal Booth", "Live Floor F",
                    "Live Floor R", "Iso Booth"
                };
                int i = std::max(0, std::min(4, static_cast<int>(m_params[idx] * 4.999f)));
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

    /* ── Audio processing ────────────────────────────────────────── */

    void process(float* pcm, size_t frames, int channels) override
    {
        if (!isEnabled() || isBypassed()) return;
        if (channels < 1 || frames == 0) return;

        // 1. Live room convolution (source-position-dependent IR)
        applyRoomIR(pcm, frames, channels);

        // 2. Mic stage — apply mic character via FxMicModeler
        m_micModeler.process(pcm, frames, channels);

        // 3. Console preamp (tube saturation + tone stack)
        m_tubePreamp.process(pcm, frames, channels);
        m_xenyx.process(pcm, frames, channels);

        // 4. Outboard chain
        m_compressor.process(pcm, frames, channels);
        m_eq.process(pcm, frames, channels);

        // 5. Chamber reverb (Lexicon 224 in Plate mode)
        m_chamber.process(pcm, frames, channels);

        // 6. Final polish
        m_enhancer.process(pcm, frames, channels);

        // 7. Output trim
        const float outLin = m_outputLin;
        if (outLin != 1.0f) {
            for (size_t f = 0; f < frames; ++f) {
                for (int ch = 0; ch < channels; ++ch) {
                    pcm[f * channels + ch] *= outLin;
                }
            }
        }
    }

private:
    void buildRoomIRs(int sr)
    {
        // Build 5 short live-room IRs (~80 ms each) with different
        // early reflection patterns per source position. Bigger room
        // = longer reflections, more density.
        const int irLen = static_cast<int>(0.080 * sr);

        for (int pos = 0; pos < 5; ++pos) {
            m_roomIRs[pos].assign(irLen, 0.0f);
            m_roomIRs[pos][0] = 1.0f;
        }

        // Drum Riser (back of room — long reflections)
        addReflection(m_roomIRs[DrumRiser], sr, 0.012f, 0.18f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.022f, 0.13f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.038f, 0.09f);
        addReflection(m_roomIRs[DrumRiser], sr, 0.055f, 0.06f);
        applyExpDecay(m_roomIRs[DrumRiser], 0.70f);

        // Vocal Booth (close, dry)
        addReflection(m_roomIRs[VocalBooth], sr, 0.004f, 0.12f);
        addReflection(m_roomIRs[VocalBooth], sr, 0.008f, 0.06f);
        applyExpDecay(m_roomIRs[VocalBooth], 0.20f);

        // Live Floor Front
        addReflection(m_roomIRs[LiveFloorFront], sr, 0.008f, 0.16f);
        addReflection(m_roomIRs[LiveFloorFront], sr, 0.014f, 0.11f);
        addReflection(m_roomIRs[LiveFloorFront], sr, 0.025f, 0.07f);
        addReflection(m_roomIRs[LiveFloorFront], sr, 0.040f, 0.045f);
        applyExpDecay(m_roomIRs[LiveFloorFront], 0.50f);

        // Live Floor Rear (more ambient)
        addReflection(m_roomIRs[LiveFloorRear], sr, 0.014f, 0.20f);
        addReflection(m_roomIRs[LiveFloorRear], sr, 0.024f, 0.14f);
        addReflection(m_roomIRs[LiveFloorRear], sr, 0.040f, 0.10f);
        addReflection(m_roomIRs[LiveFloorRear], sr, 0.058f, 0.07f);
        applyExpDecay(m_roomIRs[LiveFloorRear], 0.85f);

        // Iso Booth (very dry, tight)
        addReflection(m_roomIRs[IsoBooth], sr, 0.003f, 0.08f);
        applyExpDecay(m_roomIRs[IsoBooth], 0.15f);

        // Working buffer for time-domain convolution
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

        // Mic — set via FxMicModeler model index
        int micIdx = std::max(0, std::min(3, static_cast<int>(m_params[ParamMicSelect] * 3.999f)));
        m_micModeler.setParamValue(0, static_cast<float>(micIdx) / 11.0f);  // U47-ish
        m_micModeler.setParamValue(1, m_params[ParamMicProximity]);
        m_micModeler.setParamValue(2, 0.5f);  // axis
        m_micModeler.setParamValue(3, 0.5f);  // input gain
        m_micModeler.setParamValue(4, 0.0f);  // fat
        m_micModeler.setParamValue(5, 0.5f);  // hf contour
        m_micModeler.setParamValue(6, 0.2f);  // low cut
        m_micModeler.setParamValue(7, 0.3f);  // tube color
        m_micModeler.setParamValue(8, 0.4f);  // body
        m_micModeler.setParamValue(9, 0.5f);  // output

        // Tube preamp — drive scales with console drive
        m_tubePreamp.setParamValue(0, 0.5f);                       // input
        m_tubePreamp.setParamValue(1, m_params[ParamConsoleDrive]); // drive
        m_tubePreamp.setParamValue(2, 0.5f);                       // warmth
        m_tubePreamp.setParamValue(3, 0.5f);                       // presence
        m_tubePreamp.setParamValue(4, 0.3f);                       // low cut
        m_tubePreamp.setParamValue(5, 0.5f);                       // transformer
        m_tubePreamp.setParamValue(6, 0.5f);                       // bias
        m_tubePreamp.setParamValue(7, 0.5f);                       // sag
        m_tubePreamp.setParamValue(8, 0.4f);                       // air
        m_tubePreamp.setParamValue(9, 0.5f);                       // output

        // Xenyx (acts as 3-band Pultec macro here)
        m_xenyx.setParamValue(0, 0.5f);                            // gain
        m_xenyx.setParamValue(1, 0.0f);                            // hpf enable
        m_xenyx.setParamValue(2, 0.3f);                            // hpf freq
        m_xenyx.setParamValue(3, 0.0f);                            // comp amount (handled by next)
        m_xenyx.setParamValue(4, m_params[ParamConsoleEQ]);        // low
        m_xenyx.setParamValue(5, m_params[ParamConsoleEQ]);        // mid
        m_xenyx.setParamValue(6, 0.5f);                            // mid freq
        m_xenyx.setParamValue(7, m_params[ParamConsoleEQ]);        // high
        m_xenyx.setParamValue(8, 0.5f);                            // output

        // 1176-style compressor
        m_compressor.setParamValue(0, 0.5f);                       // input gain
        m_compressor.setParamValue(1, m_params[ParamCompression]); // threshold
        m_compressor.setParamValue(2, m_params[ParamCompression]); // ratio
        m_compressor.setParamValue(3, 0.20f);                      // attack (fast)
        m_compressor.setParamValue(4, 0.40f);                      // release
        m_compressor.setParamValue(5, 0.30f);                      // knee
        m_compressor.setParamValue(6, 0.55f);                      // makeup
        m_compressor.setParamValue(7, 0.10f);                      // gate threshold
        m_compressor.setParamValue(8, 0.85f);                      // limiter ceiling

        // Pultec EQ (10 bands of FxParametricEq)
        for (int b = 0; b < 10; ++b) {
            m_eq.setParamValue(b, m_params[ParamConsoleEQ]);
        }

        // Lexicon 224 chamber — Plate algo
        m_chamber.setParamValue(FxLexicon224::ParamProgram,    0.66f);  // Plate
        m_chamber.setParamValue(FxLexicon224::ParamPreDelay,   0.10f);
        m_chamber.setParamValue(FxLexicon224::ParamDecay,      m_params[ParamChamberDecay]);
        m_chamber.setParamValue(FxLexicon224::ParamSize,       0.5f);
        m_chamber.setParamValue(FxLexicon224::ParamDiffusion,  0.7f);
        m_chamber.setParamValue(FxLexicon224::ParamHfDamping,  0.4f);
        m_chamber.setParamValue(FxLexicon224::ParamLfCut,      0.15f);
        m_chamber.setParamValue(FxLexicon224::ParamBassMult,   0.5f);
        m_chamber.setParamValue(FxLexicon224::ParamTrebleDecay,0.55f);
        m_chamber.setParamValue(FxLexicon224::ParamModDepth,   0.35f);
        m_chamber.setParamValue(FxLexicon224::ParamMix,        m_params[ParamChamberSend]);

        // BBE Polish
        m_enhancer.setParamValue(0, 0.40f);                        // Lo
        m_enhancer.setParamValue(1, m_params[ParamPolish]);        // Process
        m_enhancer.setParamValue(2, 0.50f);                        // Output

        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);
    }

    /* ── State ───────────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    FxMicModeler     m_micModeler;
    FxTubePreamp     m_tubePreamp;
    FxXenyxPreamp    m_xenyx;
    FxCompressor     m_compressor;
    FxParametricEq   m_eq;
    FxLexicon224     m_chamber;
    FxSonicEnhancer  m_enhancer;

    std::vector<float> m_roomIRs[5];
    const std::vector<float>* m_activeIR = nullptr;

    std::vector<float> m_roomBufL, m_roomBufR;
    int m_roomWriteIdx = 0;

    float m_outputLin = 1.0f;
};

} // namespace mc1dsp
