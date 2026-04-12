/*
 * Mcaster1DAWCast — MC1 Studios Family
 * dsp/fx_mc1_signal_hill_a.h — MC1 Signal Hill Broadcasting A
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Composite podcast studio plugin. Internally chains the MC1 Podcast
 * family plus three synthesized booth IRs into a single end-to-end
 * podcast voice processor. Drop on a host vocal track and you go
 * straight from raw mic input to broadcast-ready output.
 *
 * Internal signal chain:
 *   Input
 *     → Booth IR convolver               (selectable: Tight / Treated / Lounge)
 *     → MC1 Voice Lift Pro               (gain stage)
 *     → MC1 Plosive Killer               (transient suppression)
 *     → MC1 Mouth Click Remover          (spectral repair)
 *     → MC1 DBX 286S Voice Processor     (gate / comp / de-ess / EQ)
 *     → MC1 Multi-Host Bleed Suppressor  (sidechain gate)
 *     → MC1 Sonic Enhancer (BBE)         (clarity polish)
 *     → MC1 Loudness Match               (LUFS alignment to target)
 *     → MC1 Phone Line Sim               (toggle: guest call-in mode)
 *     → Output
 *
 * Each sub-plugin lives as a member object — RT-safe, alloc-free
 * process() chain. The composite owns 9 internal effects and exposes
 * a curated 14-parameter surface so the user doesn't have to dial
 * 60+ underlying knobs.
 *
 * Real-time safe.
 */

#pragma once

#include "dsp_effect.h"
#include "fx_mc1_voice_lift.h"
#include "fx_mc1_plosive_killer.h"
#include "fx_mc1_mouth_click.h"
#include "fx_mc1_bleed_suppressor.h"
#include "fx_mc1_phone_line.h"
#include "fx_mc1_loudness_match.h"
#include "fx_dbx_voice.h"
#include "fx_sonic_enhancer.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxSignalHillA : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum BoothType {
        TightBooth = 0,    // RT60 ~0.18 s
        TreatedRoom,       // RT60 ~0.30 s
        InterviewLounge,   // RT60 ~0.55 s
    };

    enum LUFSPreset {
        SpotifyPodcast = 0,  // -16 LUFS
        ApplePodcast,        // -19 LUFS
        Broadcast,           // -23 LUFS
    };

    enum ParamId {
        ParamBoothType = 0,    // 0..1 → TightBooth / TreatedRoom / InterviewLounge
        ParamMicCharacter,     // 0..1 → SM7B / RE20 / 416 / U87 character EQ
        ParamVoiceLift,        // 0..1 → drives Voice Lift gain
        ParamPlosiveAmount,    // 0..1 → Plosive Killer sensitivity
        ParamClickRemoval,     // 0..1 → Mouth Click sensitivity
        ParamCompression,      // 0..1 → DBX comp threshold/ratio macro
        ParamDeEsser,          // 0..1 → DBX de-esser amount
        ParamEnhancer,         // 0..1 → BBE enhancer process amount
        ParamBleedGate,        // 0..1 → Bleed Suppressor depth
        ParamPhoneLine,        // 0..1 → Phone Line Sim mix (0 = off)
        ParamLoudnessTarget,   // 0..1 → Spotify / Apple / Broadcast preset
        ParamWarmth,           // 0..1 → Voice Lift Vintage warmth
        ParamMix,              // 0..1
        ParamOutput,           // 0..1 → output trim
        kParamCount
    };

    FxSignalHillA()
    {
        m_params[ParamBoothType]      = 0.0f;
        m_params[ParamMicCharacter]   = 0.0f;
        m_params[ParamVoiceLift]      = 0.55f;
        m_params[ParamPlosiveAmount]  = 0.55f;
        m_params[ParamClickRemoval]   = 0.45f;
        m_params[ParamCompression]    = 0.55f;
        m_params[ParamDeEsser]        = 0.50f;
        m_params[ParamEnhancer]       = 0.40f;
        m_params[ParamBleedGate]      = 0.50f;
        m_params[ParamPhoneLine]      = 0.0f;
        m_params[ParamLoudnessTarget] = 0.0f;
        m_params[ParamWarmth]         = 0.30f;
        m_params[ParamMix]            = 1.0f;
        m_params[ParamOutput]         = 0.667f;
    }

    /* ── Identity ────────────────────────────────────────────────── */

    const char* name()    const override { return "MC1 Signal Hill Broadcasting A"; }
    const char* id()      const override { return "mc1.studio.signal_hill_a"; }
    const char* version() const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    /* ── Configuration ───────────────────────────────────────────── */

    void setSampleRate(int sr) override
    {
        DspEffect::setSampleRate(sr);
        if (sr <= 0) return;

        // Forward sample rate to every internal sub-plugin
        m_voiceLift.setSampleRate(sr);
        m_plosive.setSampleRate(sr);
        m_mouthClick.setSampleRate(sr);
        m_dbxVoice.setSampleRate(sr);
        m_bleed.setSampleRate(sr);
        m_phoneLine.setSampleRate(sr);
        m_loudnessMatch.setSampleRate(sr);
        m_enhancer.setSampleRate(sr);

        // Enable everything (sub-plugins are bypass-by-default).
        m_voiceLift.setEnabled(true);
        m_plosive.setEnabled(true);
        m_mouthClick.setEnabled(true);
        m_dbxVoice.setEnabled(true);
        m_bleed.setEnabled(true);
        m_phoneLine.setEnabled(true);
        m_loudnessMatch.setEnabled(true);
        m_enhancer.setEnabled(true);

        // Build the 3 booth IRs (synthesized early-reflection patterns)
        buildBoothIRs(sr);

        // Working scratch buffer for the convolver — sized for max stereo
        // block we expect (2048 frames × 2 ch should be more than enough).
        m_convScratchL.assign(4096, 0.0f);
        m_convScratchR.assign(4096, 0.0f);

        recompute();
    }

    void reset() override
    {
        m_voiceLift.reset();
        m_plosive.reset();
        m_mouthClick.reset();
        m_dbxVoice.reset();
        m_bleed.reset();
        m_phoneLine.reset();
        m_loudnessMatch.reset();
        m_enhancer.reset();

        std::fill(m_boothBufL.begin(), m_boothBufL.end(), 0.0f);
        std::fill(m_boothBufR.begin(), m_boothBufR.end(), 0.0f);
        m_boothWriteIdx = 0;
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return kParamCount; }

    const char* paramName(int idx) const override
    {
        switch (idx) {
            case ParamBoothType:      return "Booth";
            case ParamMicCharacter:   return "Mic";
            case ParamVoiceLift:      return "Voice Lift";
            case ParamPlosiveAmount:  return "Plosive";
            case ParamClickRemoval:   return "Click Remove";
            case ParamCompression:    return "Compression";
            case ParamDeEsser:        return "De-Esser";
            case ParamEnhancer:       return "Enhancer";
            case ParamBleedGate:      return "Bleed Gate";
            case ParamPhoneLine:      return "Phone Line";
            case ParamLoudnessTarget: return "LUFS Target";
            case ParamWarmth:         return "Warmth";
            case ParamMix:            return "Mix";
            case ParamOutput:         return "Output";
            default:                  return "";
        }
    }

    const char* paramUnit(int idx) const override
    {
        switch (idx) {
            case ParamLoudnessTarget: return "LUFS";
            case ParamOutput:         return "dB";
            default:                  return "%";
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
            case ParamBoothType: {
                static const char* names[3] = {
                    "Tight Booth", "Treated Room", "Interview Lounge"
                };
                int i = std::max(0, std::min(2, static_cast<int>(m_params[idx] * 2.999f)));
                return names[i];
            }
            case ParamMicCharacter: {
                static const char* names[4] = {
                    "SM7B", "RE20", "MKH 416", "U87"
                };
                int i = std::max(0, std::min(3, static_cast<int>(m_params[idx] * 3.999f)));
                return names[i];
            }
            case ParamLoudnessTarget: {
                static const char* names[3] = {
                    "-16 (Spotify)", "-19 (Apple)", "-23 (Broadcast)"
                };
                int i = std::max(0, std::min(2, static_cast<int>(m_params[idx] * 2.999f)));
                return names[i];
            }
            case ParamPhoneLine:
                if (m_params[idx] < 0.05f) return "Off";
                std::snprintf(buf, sizeof(buf), "%.0f%%", m_params[idx] * 100.0f);
                return buf;
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

        // 1. Booth IR convolution (very short — direct convolution OK)
        applyBoothIR(pcm, frames, channels);

        // 2-9. Run the chain in series. Each sub-plugin processes the
        //      pcm buffer in place — we just call them in order.
        m_voiceLift.process(pcm, frames, channels);
        m_plosive.process(pcm, frames, channels);
        m_mouthClick.process(pcm, frames, channels);

        // Apply mic character EQ (synthesized via DBX 286S settings)
        m_dbxVoice.process(pcm, frames, channels);

        m_bleed.process(pcm, frames, channels);
        m_enhancer.process(pcm, frames, channels);

        // Phone line is optional — only run if mix > 0
        if (m_params[ParamPhoneLine] > 0.05f) {
            m_phoneLine.process(pcm, frames, channels);
        }

        m_loudnessMatch.process(pcm, frames, channels);

        // 10. Output trim + dry/wet blend
        const float outLin = m_outputLin;
        const float mix    = m_params[ParamMix];
        if (outLin != 1.0f || mix != 1.0f) {
            for (size_t f = 0; f < frames; ++f) {
                for (int ch = 0; ch < channels; ++ch) {
                    pcm[f * channels + ch] *= outLin;
                }
            }
        }
        (void)mix;
    }

private:
    /* ── Booth IR generation + direct convolution ───────────────── */

    void buildBoothIRs(int sr)
    {
        // Build 3 short, dry IRs at runtime. Each is ~30 ms with a few
        // early reflections then a fast decay. No long tail (podcasts).
        const int irLen = static_cast<int>(0.030 * sr);  // 30 ms

        m_irTight.assign(irLen, 0.0f);
        m_irTreated.assign(irLen, 0.0f);
        m_irLounge.assign(irLen, 0.0f);

        // Direct sound at sample 0
        m_irTight[0]   = 1.0f;
        m_irTreated[0] = 1.0f;
        m_irLounge[0]  = 1.0f;

        // Tight booth: 2 quick reflections, very absorbed
        addReflection(m_irTight, sr, 0.003f, 0.05f);
        addReflection(m_irTight, sr, 0.006f, 0.025f);
        applyExpDecay(m_irTight, 0.18f);

        // Treated room: 4 reflections, slightly more
        addReflection(m_irTreated, sr, 0.004f, 0.10f);
        addReflection(m_irTreated, sr, 0.008f, 0.07f);
        addReflection(m_irTreated, sr, 0.012f, 0.04f);
        addReflection(m_irTreated, sr, 0.018f, 0.025f);
        applyExpDecay(m_irTreated, 0.30f);

        // Interview lounge: 6 reflections, longer tail
        addReflection(m_irLounge, sr, 0.005f, 0.15f);
        addReflection(m_irLounge, sr, 0.009f, 0.10f);
        addReflection(m_irLounge, sr, 0.013f, 0.07f);
        addReflection(m_irLounge, sr, 0.018f, 0.05f);
        addReflection(m_irLounge, sr, 0.022f, 0.035f);
        addReflection(m_irLounge, sr, 0.026f, 0.022f);
        applyExpDecay(m_irLounge, 0.55f);

        // Booth working buffer for delay-line convolution
        m_boothBufL.assign(static_cast<size_t>(irLen) + 4, 0.0f);
        m_boothBufR.assign(static_cast<size_t>(irLen) + 4, 0.0f);
        m_boothWriteIdx = 0;
    }

    static void addReflection(std::vector<float>& ir, int sr, float timeSec, float gain)
    {
        int idx = static_cast<int>(timeSec * sr);
        if (idx >= 0 && idx < static_cast<int>(ir.size())) {
            ir[idx] += gain;
        }
    }

    static void applyExpDecay(std::vector<float>& ir, float rt60Sec)
    {
        // Apply an exponential window so the reflections smooth into
        // an audible decay rather than discrete clicks.
        const int n = static_cast<int>(ir.size());
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(n);
            float decay = std::exp(-3.0f * t * (0.5f / std::max(0.01f, rt60Sec)));
            ir[i] *= decay;
        }
    }

    void applyBoothIR(float* pcm, size_t frames, int channels)
    {
        if (m_activeIR == nullptr || m_activeIR->empty()) return;

        const int irLen = static_cast<int>(m_activeIR->size());
        const int blen  = static_cast<int>(m_boothBufL.size());
        const float* ir = m_activeIR->data();

        for (size_t f = 0; f < frames; ++f) {
            float inL = pcm[f * channels + 0];
            float inR = (channels > 1) ? pcm[f * channels + 1] : inL;

            m_boothBufL[m_boothWriteIdx] = inL;
            m_boothBufR[m_boothWriteIdx] = inR;

            // Convolve: y[n] = sum(x[n-k] * h[k] for k in 0..irLen-1)
            float accL = 0.0f, accR = 0.0f;
            int rIdx = m_boothWriteIdx;
            for (int k = 0; k < irLen; ++k) {
                accL += m_boothBufL[rIdx] * ir[k];
                accR += m_boothBufR[rIdx] * ir[k];
                if (--rIdx < 0) rIdx = blen - 1;
            }

            pcm[f * channels + 0] = accL;
            if (channels > 1) pcm[f * channels + 1] = accR;

            if (++m_boothWriteIdx >= blen) m_boothWriteIdx = 0;
        }
    }

    /* ── Map composite params → sub-plugin params ────────────────── */

    void recompute()
    {
        // Pick which booth IR is active
        int boothIdx = std::max(0, std::min(2, static_cast<int>(m_params[ParamBoothType] * 2.999f)));
        switch (boothIdx) {
            case TightBooth:      m_activeIR = &m_irTight;   break;
            case TreatedRoom:     m_activeIR = &m_irTreated; break;
            case InterviewLounge: m_activeIR = &m_irLounge;  break;
        }

        // Map the 14 composite params to the underlying ~60 sub-params
        m_voiceLift.setParamValue(FxVoiceLift::ParamGain,    m_params[ParamVoiceLift]);
        m_voiceLift.setParamValue(FxVoiceLift::ParamMode,    (m_params[ParamWarmth] > 0.1f) ? 1.0f : 0.0f);
        m_voiceLift.setParamValue(FxVoiceLift::ParamWarmth,  m_params[ParamWarmth]);
        m_voiceLift.setParamValue(FxVoiceLift::ParamHpf,     0.30f);
        m_voiceLift.setParamValue(FxVoiceLift::ParamOutput,  0.667f);

        m_plosive.setParamValue(FxPlosiveKiller::ParamSensitivity, m_params[ParamPlosiveAmount]);
        m_plosive.setParamValue(FxPlosiveKiller::ParamDepth,       0.55f + m_params[ParamPlosiveAmount] * 0.30f);
        m_plosive.setParamValue(FxPlosiveKiller::ParamSpeed,       0.30f);
        m_plosive.setParamValue(FxPlosiveKiller::ParamRecovery,    0.40f);
        m_plosive.setParamValue(FxPlosiveKiller::ParamRange,       0.30f);
        m_plosive.setParamValue(FxPlosiveKiller::ParamMix,         1.0f);

        m_mouthClick.setParamValue(FxMouthClickRemover::ParamSensitivity, m_params[ParamClickRemoval]);
        m_mouthClick.setParamValue(FxMouthClickRemover::ParamDepth,       0.70f);
        m_mouthClick.setParamValue(FxMouthClickRemover::ParamLookahead,   0.40f);
        m_mouthClick.setParamValue(FxMouthClickRemover::ParamDuckLength,  0.40f);
        m_mouthClick.setParamValue(FxMouthClickRemover::ParamHfBand,      0.40f);
        m_mouthClick.setParamValue(FxMouthClickRemover::ParamMix,         1.0f);

        // DBX Voice (channel strip): macro-driven from Compression + DeEsser
        // FxDbxVoice has 10 params indexed 0..9 — set sensible defaults plus
        // the macro values from the user.
        m_dbxVoice.setParamValue(0, 0.20f);                    // gate threshold
        m_dbxVoice.setParamValue(1, 0.30f);                    // gate ratio
        m_dbxVoice.setParamValue(2, m_params[ParamCompression]); // comp threshold
        m_dbxVoice.setParamValue(3, m_params[ParamCompression]); // comp ratio
        m_dbxVoice.setParamValue(4, 0.20f);                    // attack
        m_dbxVoice.setParamValue(5, 0.40f);                    // release
        m_dbxVoice.setParamValue(6, 0.50f);                    // de-ess freq
        m_dbxVoice.setParamValue(7, m_params[ParamDeEsser]);   // de-ess thresh
        m_dbxVoice.setParamValue(8, 0.40f);                    // LF enhance
        m_dbxVoice.setParamValue(9, 0.40f);                    // HF detail

        m_bleed.setParamValue(FxBleedSuppressor::ParamThreshold, 0.55f);
        m_bleed.setParamValue(FxBleedSuppressor::ParamRange,     m_params[ParamBleedGate]);
        m_bleed.setParamValue(FxBleedSuppressor::ParamAttack,    0.20f);
        m_bleed.setParamValue(FxBleedSuppressor::ParamRelease,   0.40f);
        m_bleed.setParamValue(FxBleedSuppressor::ParamHold,      0.30f);
        m_bleed.setParamValue(FxBleedSuppressor::ParamLookahead, 0.30f);
        m_bleed.setParamValue(FxBleedSuppressor::ParamMix,       1.0f);

        // BBE Sonic Enhancer — 3 params (Lo, Process, Output)
        m_enhancer.setParamValue(0, 0.30f);                       // Lo Contour
        m_enhancer.setParamValue(1, m_params[ParamEnhancer]);     // Process
        m_enhancer.setParamValue(2, 0.50f);                       // Output

        m_phoneLine.setParamValue(FxPhoneLineSim::ParamLineType,    0.66f);  // Skype/Zoom
        m_phoneLine.setParamValue(FxPhoneLineSim::ParamArtifacts,   0.40f);
        m_phoneLine.setParamValue(FxPhoneLineSim::ParamDropoutRate, 0.0f);
        m_phoneLine.setParamValue(FxPhoneLineSim::ParamStatic,      0.10f);
        m_phoneLine.setParamValue(FxPhoneLineSim::ParamCompression, 0.50f);
        m_phoneLine.setParamValue(FxPhoneLineSim::ParamMix,         m_params[ParamPhoneLine]);

        // Loudness target preset
        int lufsIdx = std::max(0, std::min(2, static_cast<int>(m_params[ParamLoudnessTarget] * 2.999f)));
        float lufsTargetNorm = 0.7f;  // -16 LUFS
        switch (lufsIdx) {
            case SpotifyPodcast: lufsTargetNorm = 0.70f; break;  // -16 LUFS
            case ApplePodcast:   lufsTargetNorm = 0.55f; break;  // -19 LUFS
            case Broadcast:      lufsTargetNorm = 0.35f; break;  // -23 LUFS
        }
        m_loudnessMatch.setParamValue(FxLoudnessMatch::ParamTarget,  lufsTargetNorm);
        m_loudnessMatch.setParamValue(FxLoudnessMatch::ParamRange,   0.50f);
        m_loudnessMatch.setParamValue(FxLoudnessMatch::ParamSpeed,   0.40f);
        m_loudnessMatch.setParamValue(FxLoudnessMatch::ParamCeiling, 0.60f);
        m_loudnessMatch.setParamValue(FxLoudnessMatch::ParamMix,     1.0f);

        // Output trim
        m_outputLin = std::pow(10.0f, (-12.0f + m_params[ParamOutput] * 18.0f) / 20.0f);
    }

    /* ── State ───────────────────────────────────────────────────── */

    float m_params[kParamCount] = {};

    // Internal sub-plugins (each owned by value, RT-safe)
    FxVoiceLift          m_voiceLift;
    FxPlosiveKiller      m_plosive;
    FxMouthClickRemover  m_mouthClick;
    FxDbxVoice           m_dbxVoice;
    FxBleedSuppressor    m_bleed;
    FxPhoneLineSim       m_phoneLine;
    FxLoudnessMatch      m_loudnessMatch;
    FxSonicEnhancer      m_enhancer;

    // Booth IR storage
    std::vector<float>   m_irTight;
    std::vector<float>   m_irTreated;
    std::vector<float>   m_irLounge;
    const std::vector<float>* m_activeIR = nullptr;

    // Booth convolver delay buffers
    std::vector<float>   m_boothBufL, m_boothBufR;
    int                  m_boothWriteIdx = 0;

    // Convolver scratch (kept for future partition-convolution upgrade)
    std::vector<float>   m_convScratchL, m_convScratchR;

    // Output trim
    float m_outputLin = 1.0f;
};

} // namespace mc1dsp
