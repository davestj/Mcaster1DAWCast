/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_mic_modeler.h — Microphone Modeler / Emulator
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Models the acoustic and electronic characteristics of legendary
 * studio microphones. Each model applies:
 *   - Frequency response curve (capsule + body resonance)
 *   - Proximity effect (bass boost at close distance)
 *   - Polar pattern simulation (frequency-dependent)
 *   - Tube/transformer coloring (where applicable)
 *   - Transient response (capsule mass + damping)
 *
 * Three mic categories with 12 total models:
 *
 * TUBE CONDENSERS (large diaphragm):
 *   0: Bock 167 — Dual K67 capsule, EF732 tube, Lundahl transformer
 *   1: U47 — Classic Telefunken, VF14 tube, warm presence
 *   2: C12 — AKG, 6072 tube, brilliant top end
 *   3: U67 — Neumann, EF86 tube, smooth midrange
 *
 * DYNAMIC MICS:
 *   4: DN-7 (SM7B) — Flat, broadcast, smooth highs
 *   5: DN-20 (RE20) — Variable-D, minimal proximity, broadcast
 *   6: DN-88 (RE320) — Smooth, flattering, versatile
 *   7: DN-441 (MD441) — Supercardioid, balanced natural sound
 *
 * RIBBON MICS:
 *   8: RB-77DX (RCA 77) — Figure-8, warm, vintage broadcast
 *   9: RB-160 (Coles 4038) — Warm mid-forward, rock guitar
 *   10: Royer 121 — Modern ribbon, detailed, smooth
 *   11: DN-421 (MD421) — Dynamic/ribbon hybrid character
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>

namespace mc1dsp {

/* ── Mic model profile — defines the acoustic DNA of each microphone ── */

struct MicModelProfile {
    const char* name;
    const char* category;     /* "Tube Condenser", "Dynamic", "Ribbon" */

    /* Frequency response: 6-band parametric EQ
     * Each band: {frequency, gain_dB, Q, type}
     * type: 0=peaking, 1=low_shelf, 2=high_shelf */
    struct Band {
        float freq;
        float gain;
        float Q;
        int type;
    };
    Band bands[6];

    float proximityFreq;      /* Center of proximity boost (Hz) */
    float proximityQ;         /* Q of proximity resonance */
    float resonanceFreq;      /* Body resonance frequency */
    float resonanceQ;
    bool hasTube;              /* Has tube coloring */
    float defaultProximity;    /* Default proximity amount */
};

static constexpr MicModelProfile kMicModels[] = {
    /* 0: Bock 167 — K67 capsule, EF732 tube, Lundahl transformer.
     *    Rich low-mids from transformer coupling, articulate presence
     *    peak at 3kHz, subtle rolloff above 14kHz from tube stage. */
    {"Bock 167", "Tube Condenser",
     {{80,+2.0f,0.7f,1}, {200,+1.0f,1.0f,0}, {3000,+3.0f,1.2f,0},
      {5000,+1.5f,1.0f,0}, {10000,+1.0f,0.8f,0}, {14000,-1.0f,0.7f,2}},
     100.0f, 0.8f, 5000.0f, 2.0f, true, 0.3f},

    /* 1: U47 — warm, present, legendary vocal mic.
     *    Strong proximity effect (+3dB LF shelf), aggressive 2.8kHz
     *    presence peak for vocal cut-through, gentle HF rolloff above 12kHz
     *    from the VF14 pentode tube and BV8 transformer. */
    {"U47 Tube", "Tube Condenser",
     {{60,+3.0f,0.6f,1}, {240,+1.5f,0.9f,0}, {2800,+4.0f,1.5f,0},
      {5000,+2.0f,1.0f,0}, {8000,-0.5f,0.8f,0}, {12000,-2.0f,0.7f,2}},
     120.0f, 0.7f, 4500.0f, 1.8f, true, 0.35f},

    /* 2: C12 — brilliant, airy, detailed top end.
     *    The 6072 tube and CK12 capsule produce a rising response from
     *    3.5kHz through 16kHz — the "air" that makes this mic legendary
     *    for vocals and acoustic instruments. Minimal LF coloring. */
    {"C12 Tube", "Tube Condenser",
     {{40,+1.0f,0.7f,1}, {300,+0.5f,0.8f,0}, {3500,+3.5f,1.3f,0},
      {8000,+3.0f,1.0f,0}, {12000,+2.5f,0.8f,0}, {16000,+1.0f,0.7f,2}},
     100.0f, 0.9f, 6000.0f, 2.5f, true, 0.25f},

    /* 3: U67 — smooth, warm midrange, less hyped than U47.
     *    EF86 tube with gentler presence peak at 2kHz (not 2.8kHz like U47).
     *    The -10dB pad and switchable patterns make it versatile. Rolls off
     *    smoothly above 8kHz — the "polite" Neumann. */
    {"U67 Tube", "Tube Condenser",
     {{50,+2.5f,0.6f,1}, {250,+1.0f,0.8f,0}, {2000,+2.0f,1.0f,0},
      {4000,+1.0f,0.9f,0}, {8000,-1.0f,0.7f,0}, {12000,-1.5f,0.7f,2}},
     110.0f, 0.7f, 4000.0f, 1.5f, true, 0.3f},

    /* 4: DN-7 (SM7B) — flat, broadcast, smooth highs.
     *    Internal air suspension shock mount kills handling noise.
     *    Mild LF rolloff from transformer, broad 2.5-5kHz presence hump
     *    for speech intelligibility, steep HF rolloff above 12kHz. */
    {"DN-7 (SM7B)", "Dynamic",
     {{80,-1.0f,0.7f,1}, {200,+0.5f,0.8f,0}, {2500,+1.5f,1.0f,0},
      {5000,+2.0f,1.2f,0}, {8000,-1.0f,0.9f,0}, {12000,-3.0f,0.7f,2}},
     150.0f, 0.6f, 6000.0f, 1.5f, false, 0.2f},

    /* 5: DN-20 (RE20) — broadcast, minimal proximity.
     *    Variable-D technology rejects off-axis sound and reduces
     *    proximity effect. Broad, flat response with subtle presence
     *    lift at 4kHz. The industry standard for voice-over and radio. */
    {"DN-20 (RE20)", "Dynamic",
     {{60,+1.0f,0.6f,1}, {150,-0.5f,0.8f,0}, {2000,+1.0f,0.9f,0},
      {4000,+2.5f,1.0f,0}, {8000,+1.0f,0.8f,0}, {12000,-2.0f,0.7f,2}},
     120.0f, 0.5f, 5500.0f, 1.2f, false, 0.1f},

    /* 6: DN-88 (RE320) — smooth, flattering.
     *    Dual-element design with separate paths for lows and highs.
     *    Slightly warmer than the RE20, with a gentler presence peak.
     *    Excellent on kick drum and baritone vocals. */
    {"DN-88 (RE320)", "Dynamic",
     {{60,+0.5f,0.7f,1}, {200,+1.0f,0.8f,0}, {1500,+0.5f,0.8f,0},
      {3500,+2.0f,1.0f,0}, {7000,+1.5f,0.9f,0}, {10000,-1.5f,0.7f,2}},
     130.0f, 0.6f, 5000.0f, 1.3f, false, 0.2f},

    /* 7: DN-441 (MD441) — supercardioid, balanced.
     *    Five-position bass rolloff switch, tight polar pattern.
     *    Smooth response with controlled presence at 5kHz.
     *    Preferred by many European broadcasters. */
    {"DN-441 (MD441)", "Dynamic",
     {{50,+0.5f,0.7f,1}, {300,-0.5f,0.7f,0}, {2000,+1.5f,1.0f,0},
      {5000,+2.5f,1.3f,0}, {8000,+1.0f,0.8f,0}, {12000,-1.0f,0.7f,2}},
     140.0f, 0.7f, 6500.0f, 2.0f, false, 0.15f},

    /* 8: RB-77DX (RCA 77) — warm vintage broadcast ribbon.
     *    Heavy +3dB LF shelf from large ribbon mass, pronounced mid-range
     *    warmth at 200Hz, steep HF rolloff above 6kHz (-5dB at 10kHz).
     *    The golden age broadcast sound. */
    {"RB-77DX (RCA 77)", "Ribbon",
     {{80,+3.0f,0.6f,1}, {200,+2.0f,0.7f,0}, {1500,+1.0f,0.8f,0},
      {3000,-0.5f,0.8f,0}, {6000,-2.0f,0.7f,0}, {10000,-5.0f,0.6f,2}},
     100.0f, 0.5f, 3000.0f, 1.0f, false, 0.4f},

    /* 9: RB-160 (Coles 4038) — warm, mid-forward rock ribbon.
     *    BBC engineering standard. Strong 300Hz body, forward 1.2kHz
     *    midrange, gentle HF rolloff. Extraordinary on guitar amps,
     *    strings, and brass. Ribbon mass gives natural transient rounding. */
    {"RB-160 (Coles 4038)", "Ribbon",
     {{60,+2.0f,0.7f,1}, {300,+2.5f,0.8f,0}, {1200,+1.5f,0.9f,0},
      {3500,+0.5f,0.8f,0}, {6000,-1.5f,0.7f,0}, {10000,-4.0f,0.6f,2}},
     90.0f, 0.6f, 2800.0f, 1.2f, false, 0.35f},

    /* 10: Royer 121 — modern ribbon, detailed, smooth.
     *    Offset ribbon design for higher SPL handling. Flatter than
     *    vintage ribbons with extended HF to 12kHz. Subtle and natural,
     *    with a gentle presence dip at 8kHz that avoids harshness. */
    {"Royer 121", "Ribbon",
     {{40,+1.5f,0.7f,1}, {200,+0.5f,0.7f,0}, {2000,+1.0f,0.9f,0},
      {4000,+0.5f,0.8f,0}, {8000,-1.0f,0.7f,0}, {12000,-3.0f,0.6f,2}},
     100.0f, 0.7f, 4000.0f, 1.5f, false, 0.25f},

    /* 11: DN-421 (MD421) — versatile dynamic.
     *    Five-position bass contour switch. Broad, punchy response
     *    with presence at 4kHz. Less proximity effect than most dynamics
     *    due to internal acoustic labyrinth. Great on toms and guitar cabs. */
    {"DN-421 (MD421)", "Dynamic",
     {{60,+1.5f,0.7f,1}, {250,+0.5f,0.7f,0}, {1800,+1.0f,0.9f,0},
      {4000,+2.0f,1.1f,0}, {6000,+1.5f,0.9f,0}, {10000,-2.0f,0.7f,2}},
     130.0f, 0.6f, 5500.0f, 1.5f, false, 0.2f},
};

static constexpr int kNumMicModels = 12;

/* ── HF contour presets ─────────────────────────────────────────────── */

enum HFContourMode {
    HFC_CUT_5K  = 0,   /* -1.5 dB high shelf at 5kHz */
    HFC_CUT_10K = 1,   /* -3.0 dB high shelf at 10kHz */
    HFC_FLAT    = 2,   /* No contour */
    HFC_BOOST_10K = 3, /* +2.0 dB high shelf at 10kHz */
};

/* ── Mic Modeler effect ─────────────────────────────────────────────── */

class FxMicModeler : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* Parameter indices */
    enum Param {
        P_MIC_MODEL = 0,   // 0–11 (enum)
        P_PROXIMITY,        // 0–100%
        P_AXIS,             // 0–100% (on-axis=0, off-axis=100)
        P_INPUT_GAIN,       // 0 to +40 dB
        P_FAT,              // 0 or 1
        P_HF_CONTOUR,       // 0–3 (enum)
        P_LOW_CUT,          // 20–400 Hz
        P_TUBE_COLOR,       // 0–100%
        P_BODY_RESONANCE,   // 0–100%
        P_OUTPUT,           // -20 to +10 dB
        P_COUNT
    };

    FxMicModeler() { recomputeFilters(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "Mic Modeler"; }
    const char* id()       const override { return "mc1.modeling.mic"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        float inputPeak  = 0.0f;
        float outputPeak = 0.0f;

        for (size_t f = 0; f < frames; ++f) {
            for (int ch = 0; ch < channels && ch < MAX_CH; ++ch) {
                float s = pcm[f * channels + ch];

                /* 1. Input gain */
                s *= inputGainLin_;

                /* Input peak metering (pre-processing) */
                float inAbs = std::fabs(s);
                if (inAbs > inputPeak) inputPeak = inAbs;

                /* 2. HPF (low cut) — removes rumble before modeling */
                s = bqTick(hpf_[ch], s, ch);

                /* 3. Frequency response — 6-band EQ from mic profile.
                 *    This is the core of the modeler: each band shapes the
                 *    incoming signal to match the target mic's capsule,
                 *    body, and electronics frequency response curve. */
                for (int b = 0; b < 6; ++b)
                    s = bqTick(modelEQ_[b][ch], s, ch);

                /* 4. Proximity effect — bass boost based on distance.
                 *    Real mics exhibit a rising LF response as the source
                 *    moves closer (pressure gradient transducers). The boost
                 *    is centered at the mic's characteristic proximity
                 *    frequency with its natural Q. */
                s = bqTick(proximityBoost_[ch], s, ch);

                /* 5. Off-axis coloring — HF rolloff when off-axis.
                 *    Moving off-axis causes progressive high-frequency loss
                 *    because the capsule becomes directionally selective at
                 *    shorter wavelengths. Controlled by the axis parameter. */
                s = bqTick(axisFilter_[ch], s, ch);

                /* 6. Body resonance — capsule housing resonance peak.
                 *    Every mic body has a resonant frequency determined by
                 *    the headbasket geometry, grille mesh, and internal
                 *    acoustic chamber. This adds that characteristic "ring". */
                s = bqTick(bodyResonance_bq_[ch], s, ch);

                /* 7. Fat switch — LF boost for condensers.
                 *    Engages a broad low shelf from 10–400Hz, adding
                 *    weight and chest resonance to vocals. Common on large
                 *    diaphragm condensers (Neumann -10dB pad circuits
                 *    often add this as a side effect). */
                if (fatEnabled_)
                    s = bqTick(fatShelf_[ch], s, ch);

                /* 8. HF contour — user-selectable high-frequency shaping.
                 *    Four presets: two cuts, flat, and a presence boost.
                 *    Bypassed when set to flat (coefficients are unity). */
                if (hfContourMode_ != HFC_FLAT)
                    s = bqTick(hfContour_[ch], s, ch);

                /* 9. Tube/ribbon color — soft saturation.
                 *    For tube condensers: models the triode's asymmetric
                 *    soft clipping and even harmonic generation.
                 *    For ribbons: models the transformer's iron core
                 *    hysteresis and subtle LF thickening.
                 *    For dynamics: minimal, just transformer warmth. */
                if (tubeAmount_ > 0.01f) {
                    float saturated = std::tanh(s * (1.0f + tubeAmount_ * 2.0f));
                    s = s * (1.0f - tubeAmount_) + saturated * tubeAmount_;
                }

                /* 10. Output level */
                s *= outputLin_;

                /* Output peak metering */
                float outAbs = std::fabs(s);
                if (outAbs > outputPeak) outputPeak = outAbs;

                pcm[f * channels + ch] = s;
            }
        }

        /* Metering (atomic for UI thread) */
        meterInputPeak_.store(
            (inputPeak > 1e-10f) ? 20.0f * std::log10(inputPeak) : -96.0f,
            std::memory_order_relaxed);
        meterOutputPeak_.store(
            (outputPeak > 1e-10f) ? 20.0f * std::log10(outputPeak) : -96.0f,
            std::memory_order_relaxed);
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(hpf_[c]);
            for (int b = 0; b < 6; ++b)
                bqClear(modelEQ_[b][c]);
            bqClear(proximityBoost_[c]);
            bqClear(axisFilter_[c]);
            bqClear(bodyResonance_bq_[c]);
            bqClear(fatShelf_[c]);
            bqClear(hfContour_[c]);
        }
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        recomputeFilters();
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Mic Model", "Proximity", "Axis", "Input Gain",
            "Fat Switch", "HF Contour", "Low Cut", "Tube/Ribbon Color",
            "Body Resonance", "Output"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "", "%", "%", "dB", "", "", "Hz", "%", "%", "dB"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_MIC_MODEL:      return static_cast<float>(modelIndex_) / 11.0f;
            case P_PROXIMITY:      return proximity_ / 100.0f;
            case P_AXIS:           return axis_ / 100.0f;
            case P_INPUT_GAIN:     return inputGainDb_ / 40.0f;
            case P_FAT:            return fatEnabled_ ? 1.0f : 0.0f;
            case P_HF_CONTOUR:     return static_cast<float>(hfContourMode_) / 3.0f;
            case P_LOW_CUT:        return (lowCutHz_ - 20.0f) / 380.0f;
            case P_TUBE_COLOR:     return tubeAmount_;
            case P_BODY_RESONANCE: return bodyResonance_ / 100.0f;
            case P_OUTPUT:         return (outputDb_ + 20.0f) / 30.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        switch (index) {
            case P_MIC_MODEL: {
                int m = static_cast<int>(v * 11.0f + 0.5f);
                if (m < 0) m = 0; else if (m > 11) m = 11;
                modelIndex_ = m;
                recomputeFilters();
            } break;
            case P_PROXIMITY:
                proximity_ = v * 100.0f;
                recomputeFilters();
                break;
            case P_AXIS:
                axis_ = v * 100.0f;
                recomputeFilters();
                break;
            case P_INPUT_GAIN:
                inputGainDb_ = v * 40.0f;
                inputGainLin_ = std::pow(10.0f, inputGainDb_ / 20.0f);
                break;
            case P_FAT:
                fatEnabled_ = (v >= 0.5f);
                recomputeFilters();
                break;
            case P_HF_CONTOUR: {
                int m = static_cast<int>(v * 3.0f + 0.5f);
                if (m < 0) m = 0; else if (m > 3) m = 3;
                hfContourMode_ = m;
                recomputeFilters();
            } break;
            case P_LOW_CUT:
                lowCutHz_ = v * 380.0f + 20.0f;
                recomputeFilters();
                break;
            case P_TUBE_COLOR:
                tubeAmount_ = v;
                break;
            case P_BODY_RESONANCE:
                bodyResonance_ = v * 100.0f;
                recomputeFilters();
                break;
            case P_OUTPUT:
                outputDb_ = v * 30.0f - 20.0f;
                outputLin_ = std::pow(10.0f, outputDb_ / 20.0f);
                break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[48];
        switch (index) {
            case P_MIC_MODEL:
                snprintf(buf, 48, "%s", kMicModels[modelIndex_].name);
                break;
            case P_PROXIMITY:
                snprintf(buf, 48, "%.0f%%", proximity_);
                break;
            case P_AXIS:
                snprintf(buf, 48, "%.0f%%", axis_);
                break;
            case P_INPUT_GAIN:
                snprintf(buf, 48, "+%.1f dB", inputGainDb_);
                break;
            case P_FAT:
                snprintf(buf, 48, "%s", fatEnabled_ ? "On" : "Off");
                break;
            case P_HF_CONTOUR:
                switch (hfContourMode_) {
                    case HFC_CUT_5K:    snprintf(buf, 48, "Cut -1.5dB@5k"); break;
                    case HFC_CUT_10K:   snprintf(buf, 48, "Cut -3dB@10k"); break;
                    case HFC_FLAT:      snprintf(buf, 48, "Flat"); break;
                    case HFC_BOOST_10K: snprintf(buf, 48, "Boost +2dB@10k"); break;
                    default:            snprintf(buf, 48, "Flat"); break;
                }
                break;
            case P_LOW_CUT:
                snprintf(buf, 48, "%.0f Hz", lowCutHz_);
                break;
            case P_TUBE_COLOR:
                snprintf(buf, 48, "%.0f%%", tubeAmount_ * 100.0f);
                break;
            case P_BODY_RESONANCE:
                snprintf(buf, 48, "%.0f%%", bodyResonance_);
                break;
            case P_OUTPUT:
                snprintf(buf, 48, "%+.1f dB", outputDb_);
                break;
            default:
                buf[0] = 0;
        }
        return buf;
    }

    /* ── Convenience: get current model info for UI ──────────────── */

    int currentModelIndex() const { return modelIndex_; }
    const char* currentModelName() const { return kMicModels[modelIndex_].name; }
    const char* currentModelCategory() const { return kMicModels[modelIndex_].category; }

private:
    /* ── Parameters ──────────────────────────────────────────────── */

    int   modelIndex_     =   4;       /* 0–11, default: DN-7 (SM7B) */
    float proximity_      =  30.0f;    /* 0–100% */
    float axis_           =  50.0f;    /* 0–100% (on-axis=0, off-axis=100) */
    float inputGainDb_    =  10.0f;    /* 0 to +40 dB */
    bool  fatEnabled_     = false;     /* LF boost (condensers) */
    int   hfContourMode_  =   2;       /* 0–3, default: Flat */
    float lowCutHz_       =  80.0f;    /* 20–400 Hz */
    float tubeAmount_     =   0.0f;    /* 0.0–1.0 (0–100%) */
    float bodyResonance_  =  50.0f;    /* 0–100% */
    float outputDb_       =   0.0f;    /* -20 to +10 dB */

    /* ── Pre-computed linear gains ───────────────────────────────── */

    float inputGainLin_   = std::pow(10.0f, 10.0f / 20.0f);  /* +10 dB */
    float outputLin_      = 1.0f;                              /*  0 dB */

    /* ── Biquad filter ───────────────────────────────────────────── */

    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };

    BQ hpf_[MAX_CH];                   /* high-pass (low cut) */
    BQ modelEQ_[6][MAX_CH];            /* 6-band mic profile EQ */
    BQ proximityBoost_[MAX_CH];        /* proximity effect LF boost */
    BQ axisFilter_[MAX_CH];            /* off-axis HF rolloff */
    BQ bodyResonance_bq_[MAX_CH];      /* capsule body resonance */
    BQ fatShelf_[MAX_CH];              /* fat switch LF shelf */
    BQ hfContour_[MAX_CH];             /* HF contour shelf */

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch] - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y;
        return y;
    }
    static void bqClear(BQ& f) {
        for(int c=0;c<2;c++) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0;
    }

    /* ── RBJ Cookbook filter computations ─────────────────────────── */

    static void computeHP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q = sqrt(2)/2 Butterworth */
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 =  (1.0f + cosw0) / 2.0f / a0;
        f.b1 = -(1.0f + cosw0) / a0;
        f.b2 =  (1.0f + cosw0) / 2.0f / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    static void computeShelf(BQ& f, float freq, float gainDb, float sr, bool low) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.707f);
        float cosw0 = std::cos(w0);
        float sqA = std::sqrt(A);
        float a0;
        if (low) {
            f.b0 = A*((A+1)-(A-1)*cosw0+2*sqA*alpha);
            f.b1 = 2*A*((A-1)-(A+1)*cosw0);
            f.b2 = A*((A+1)-(A-1)*cosw0-2*sqA*alpha);
            a0   = (A+1)+(A-1)*cosw0+2*sqA*alpha;
            f.a1 = -2*((A-1)+(A+1)*cosw0);
            f.a2 = (A+1)+(A-1)*cosw0-2*sqA*alpha;
        } else {
            f.b0 = A*((A+1)+(A-1)*cosw0+2*sqA*alpha);
            f.b1 = -2*A*((A-1)+(A+1)*cosw0);
            f.b2 = A*((A+1)+(A-1)*cosw0-2*sqA*alpha);
            a0   = (A+1)-(A-1)*cosw0+2*sqA*alpha;
            f.a1 = 2*((A-1)-(A+1)*cosw0);
            f.a2 = (A+1)-(A-1)*cosw0-2*sqA*alpha;
        }
        f.b0/=a0; f.b1/=a0; f.b2/=a0; f.a1/=a0; f.a2/=a0;
    }

    static void computePeaking(BQ& f, float freq, float gainDb, float Q, float sr) {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha / A;
        f.b0 = (1.0f + alpha * A) / a0;
        f.b1 = (-2.0f * cosw0) / a0;
        f.b2 = (1.0f - alpha * A) / a0;
        f.a1 = (-2.0f * cosw0) / a0;
        f.a2 = (1.0f - alpha / A) / a0;
    }

    /* Set BQ to unity (pass-through) */
    static void computeFlat(BQ& f) {
        f.b0 = 1.0f; f.b1 = 0.0f; f.b2 = 0.0f;
        f.a1 = 0.0f; f.a2 = 0.0f;
    }

    /* ── Filter recomputation ────────────────────────────────────── */

    void recomputeFilters() {
        float sr = static_cast<float>(sampleRate_);
        const auto& model = kMicModels[modelIndex_];

        /* Proximity normalization: 0–1 range */
        float proxNorm = proximity_ / 100.0f;

        /* Axis normalization: 0=on-axis, 1=fully off-axis */
        float axisNorm = axis_ / 100.0f;

        /* Body resonance normalization: 0–1 range */
        float bodyNorm = bodyResonance_ / 100.0f;

        for (int c = 0; c < MAX_CH; ++c) {

            /* 1. HPF — 2nd-order Butterworth high-pass */
            computeHP(hpf_[c], lowCutHz_, sr);

            /* 2. Mic profile 6-band EQ — the frequency response fingerprint.
             *    Each band is computed according to its type (peaking, low shelf,
             *    or high shelf). Gains are taken directly from the mic profile. */
            for (int b = 0; b < 6; ++b) {
                const auto& band = model.bands[b];
                if (std::fabs(band.gain) < 0.01f) {
                    computeFlat(modelEQ_[b][c]);
                } else {
                    switch (band.type) {
                        case 1: /* low shelf */
                            computeShelf(modelEQ_[b][c], band.freq, band.gain, sr, true);
                            break;
                        case 2: /* high shelf */
                            computeShelf(modelEQ_[b][c], band.freq, band.gain, sr, false);
                            break;
                        default: /* 0 = peaking */
                            computePeaking(modelEQ_[b][c], band.freq, band.gain, band.Q, sr);
                            break;
                    }
                }
            }

            /* 3. Proximity effect — peaking boost at the mic's characteristic
             *    proximity frequency. The gain scales with the proximity
             *    parameter: 0% = no proximity boost, 100% = full +8dB boost
             *    scaled by the model's default proximity amount. */
            float proxGainDb = proxNorm * 8.0f * model.defaultProximity;
            if (proxGainDb > 0.05f) {
                computePeaking(proximityBoost_[c], model.proximityFreq,
                               proxGainDb, model.proximityQ, sr);
            } else {
                computeFlat(proximityBoost_[c]);
            }

            /* 4. Off-axis coloring — high shelf cut that increases with axis angle.
             *    On-axis (0%): no cut. Fully off-axis (100%): up to -8dB at 8kHz.
             *    Real mics lose HF progressively as the source moves off-axis
             *    because the capsule diameter becomes significant relative to
             *    the wavelength at high frequencies. */
            float axisHFCutDb = -axisNorm * 8.0f;
            if (axisHFCutDb < -0.05f) {
                computeShelf(axisFilter_[c], 8000.0f, axisHFCutDb, sr, false);
            } else {
                computeFlat(axisFilter_[c]);
            }

            /* 5. Body resonance — peaking EQ at the mic's resonance frequency.
             *    The gain scales with the body resonance parameter:
             *    0% = no resonance emphasis, 100% = full +4dB peak.
             *    This models the acoustic resonance of the mic body/headbasket. */
            float bodyGainDb = bodyNorm * 4.0f;
            if (bodyGainDb > 0.05f) {
                computePeaking(bodyResonance_bq_[c], model.resonanceFreq,
                               bodyGainDb, model.resonanceQ, sr);
            } else {
                computeFlat(bodyResonance_bq_[c]);
            }

            /* 6. Fat switch — broad low shelf boost from ~200Hz.
             *    Adds +3dB of warmth and body to the low end.
             *    Primarily useful on tube condensers but available on all models. */
            if (fatEnabled_) {
                computeShelf(fatShelf_[c], 200.0f, 3.0f, sr, true);
            } else {
                computeFlat(fatShelf_[c]);
            }

            /* 7. HF contour — four presets for high-frequency shaping. */
            switch (hfContourMode_) {
                case HFC_CUT_5K:
                    computeShelf(hfContour_[c], 5000.0f, -1.5f, sr, false);
                    break;
                case HFC_CUT_10K:
                    computeShelf(hfContour_[c], 10000.0f, -3.0f, sr, false);
                    break;
                case HFC_BOOST_10K:
                    computeShelf(hfContour_[c], 10000.0f, 2.0f, sr, false);
                    break;
                default: /* HFC_FLAT */
                    computeFlat(hfContour_[c]);
                    break;
            }
        }
    }

};

} // namespace mc1dsp
