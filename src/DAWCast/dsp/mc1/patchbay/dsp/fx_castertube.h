/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_castertube.h — CasterTube Vocal Tone Shaper + Tube Preamp
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Blue-tube-inspired vocal preamp with advanced tone shaping:
 *   - Dual triode tube stages (configurable tube character)
 *   - Vocal frequency targeting (bass, mid, treble warmth control)
 *   - Smooth sustain (optical compressor before tube stage)
 *   - Vocal range selector (bass, baritone, tenor, alto, soprano)
 *   - Depth control (body/chest resonance enhancement)
 *   - De-ess integration (tame sibilance before tube saturation)
 *   - Silk mode (ultra-smooth top end with harmonic rolloff)
 *
 * Signal chain:
 *   Input Gain → HPF → Optical Compressor (smooth sustain)
 *   → De-Esser → Vocal EQ (range-targeted) → Tube Stage
 *   → Depth (body enhancement) → Silk (HF smoothing)
 *   → Transformer → Output
 *
 * Designed for mic-specific processing via AudioPipe virtual cables
 * as a pre-DAW recording enhancement chain.
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>

namespace mc1dsp {

class FxCasterTube : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* ── Vocal range frequency configurations ───────────────────────── */

    struct VocalRangeConfig {
        float warmthFreq;       /* center freq for warmth boost */
        float presenceFreq;     /* center freq for presence */
        float depthFreq;        /* center freq for body/chest */
        float deessFreq;        /* center freq for sibilance */
        float fundamentalLo;    /* lowest fundamental */
        float fundamentalHi;    /* highest fundamental */
    };

    static constexpr VocalRangeConfig kVocalRanges[] = {
        /* Bass:     E2-E4 (82-330 Hz) */
        { 200.0f, 2500.0f, 120.0f, 5000.0f,  82.0f,  330.0f },
        /* Baritone: A2-A4 (110-440 Hz) */
        { 250.0f, 3000.0f, 150.0f, 5500.0f, 110.0f,  440.0f },
        /* Tenor:    C3-C5 (130-523 Hz) */
        { 300.0f, 3500.0f, 180.0f, 6000.0f, 130.0f,  523.0f },
        /* Alto:     F3-F5 (175-698 Hz) */
        { 400.0f, 4000.0f, 220.0f, 7000.0f, 175.0f,  698.0f },
        /* Soprano:  C4-C6 (262-1047 Hz) */
        { 500.0f, 4500.0f, 280.0f, 8000.0f, 262.0f, 1047.0f },
    };

    /* ── Parameter indices ──────────────────────────────────────────── */

    enum Param {
        P_INPUT_GAIN = 0,   /* 0 to +60 dB */
        P_TUBE_DRIVE,       /* 0 to 100% */
        P_TUBE_CHARACTER,   /* 0 to 100%  (0=clean/modern, 100=vintage/dark) */
        P_SUSTAIN,          /* 0 to 100%  (optical compressor amount) */
        P_VOCAL_RANGE,      /* 0 to 4     (Bass/Baritone/Tenor/Alto/Soprano) */
        P_DEPTH,            /* 0 to 100%  (body/chest resonance) */
        P_WARMTH,           /* 0 to 100%  (low-mid warmth) */
        P_PRESENCE,         /* 0 to 100%  (upper-mid presence) */
        P_SILK,             /* 0 to 100%  (HF smoothing) */
        P_DEESS,            /* 0 to 100%  (sibilance reduction) */
        P_AIR,              /* 0 to +8 dB (ultra-high shelf) */
        P_LOW_CUT,          /* 40 to 300 Hz (HPF) */
        P_TRANSFORMER,      /* 0 to 100%  (iron transformer color) */
        P_OUTPUT,           /* -60 to +10 dB */
        P_COUNT
    };

    FxCasterTube() { updateCoeffs(); }

    /* ── DspEffect interface ─────────────────────────────────────────── */

    const char* name()     const override { return "CasterTube Vocal"; }
    const char* id()       const override { return "mc1.analog.castertube"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        int ch = (channels < MAX_CH) ? channels : MAX_CH;

        /* Pre-compute linear gains */
        float inputGainLin = std::pow(10.0f, inputGainDb_ / 20.0f);
        float outputLin    = std::pow(10.0f, outputDb_ / 20.0f);

        /* Normalized controls */
        float driveNorm     = tubeDrive_ / 100.0f;
        float charNorm      = tubeCharacter_ / 100.0f;
        float sustainNorm   = sustain_ / 100.0f;
        float depthNorm     = depth_ / 100.0f;
        float warmthNorm    = warmth_ / 100.0f;
        float presenceNorm  = presence_ / 100.0f;
        float silkNorm      = silk_ / 100.0f;
        float deessNorm     = deess_ / 100.0f;
        float xfmrNorm      = transformer_ / 100.0f;

        /* Tube character: controls drive multiplier, HF rolloff, bias asymmetry.
         * At 0% (clean/modern): minimal harmonic distortion, tight transients.
         * At 100% (vintage/dark): heavy even harmonics, rolled-off highs, spongy. */
        float driveMultiplier = 1.0f + charNorm * 1.5f;

        /* Stage 1 drive: full drive scaled by character */
        float drive1 = 1.0f + driveNorm * 4.0f * driveMultiplier;
        /* Stage 2 drive: ~40% of stage 1 for cascaded dual triode topology */
        float drive2 = 1.0f + driveNorm * 1.6f * driveMultiplier;

        /* Bias asymmetry increases with vintage character */
        float biasAmount = charNorm * 0.4f;

        /* Optical compressor: threshold and ratio from sustain amount.
         * LA-2A-style: slow attack, very slow release, program-dependent. */
        float compThreshDb = -20.0f + (1.0f - sustainNorm) * 20.0f; /* -20 to 0 dBFS */
        float compRatio    =  2.0f + sustainNorm * 2.0f;            /* 2:1 to 4:1 */
        float compThreshLin = std::pow(10.0f, compThreshDb / 20.0f);

        /* De-esser threshold: lower deess% = less reduction (higher threshold) */
        float deessThreshLin = 0.05f + (1.0f - deessNorm) * 0.15f; /* 0.05 to 0.20 */

        /* Transformer color: subtle saturation + LF thickening */
        float xfmrDrive = xfmrNorm * 0.5f;

        float inputPeak  = 0.0f;
        float outputPeak = 0.0f;
        float maxGr      = 0.0f;

        for (size_t f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float s = pcm[f * channels + c];

                /* ── 1. Input Gain ── */
                s *= inputGainLin;

                /* Input peak metering (pre-processing) */
                float inAbs = std::fabs(s);
                if (inAbs > inputPeak) inputPeak = inAbs;

                /* ── 2. HPF — proximity/rumble control (2nd-order Butterworth) ── */
                s = bqTick(hpf_[c], s, c);

                /* ── 3. Optical Compressor (smooth sustain) ──
                 * Models an LA-2A-style opto cell: slow attack captures the
                 * broad envelope, very slow release creates the smooth leveling
                 * that makes vocals sit perfectly in a mix without pumping.
                 * The optical cell reacts to average level, not peak — this
                 * is what gives it the transparent, musical character. */
                if (sustainNorm > 0.001f) {
                    float absLevel = std::fabs(s);

                    /* Smooth envelope follower — the optical cell.
                     * Attack is slow (20ms) to avoid clamping transients.
                     * Release is very slow (500ms) to prevent audible pumping.
                     * This creates the distinctive LA-2A "leveling" behavior. */
                    float coeff = (absLevel > compEnv_[c]) ? optoAttack_ : optoRelease_;
                    compEnv_[c] += coeff * (absLevel - compEnv_[c]);

                    /* Gain computation: above threshold, apply ratio.
                     * The smoothed envelope means gain changes are gradual —
                     * you hear leveling, not compression artifacts. */
                    if (compEnv_[c] > compThreshLin && compThreshLin > 1e-10f) {
                        float overLin = compEnv_[c] / compThreshLin;
                        /* Compute gain reduction: output = thresh * (over)^(1/ratio-1)
                         * Simplified: gainReduction = over^(1/ratio - 1) */
                        float grLin = std::pow(overLin, 1.0f / compRatio - 1.0f);
                        s *= grLin;

                        float grDb = -20.0f * std::log10(grLin > 1e-10f ? grLin : 1e-10f);
                        if (grDb > maxGr) maxGr = grDb;
                    }

                    /* Optical auto-makeup: compensate for average gain reduction.
                     * Subtle — scales with sustain amount. Keeps perceived level
                     * stable as the compressor works harder. */
                    float makeup = 1.0f + sustainNorm * 0.3f;
                    s *= makeup;
                }

                /* ── 4. De-Esser ──
                 * Sidechain bandpass at the vocal range's sibilance frequency.
                 * When sibilance energy exceeds threshold, proportionally
                 * attenuate. Placed before tube stage so sibilants don't
                 * get amplified and made harsher by saturation. */
                if (deessNorm > 0.001f) {
                    float sidechain = bqTick(deessBP_[c], s, c);
                    float sibilance = std::fabs(sidechain);
                    if (sibilance > deessThreshLin) {
                        float reduction = (sibilance - deessThreshLin) * deessNorm * 4.0f;
                        s *= 1.0f / (1.0f + reduction);
                    }
                }

                /* ── 5. Vocal EQ — range-targeted warmth + presence ──
                 * The warmth and presence filters are tuned to the selected
                 * vocal range so they always hit the right frequencies.
                 * This is what makes CasterTube "vocal-aware" rather than
                 * a generic saturation box. */
                if (warmthNorm > 0.001f) {
                    s = bqTick(warmthEQ_[c], s, c);
                }
                if (presenceNorm > 0.001f) {
                    s = bqTick(presenceEQ_[c], s, c);
                }

                /* ── 6. Dual Tube Stages ──
                 * Stage 1: higher drive, full bias asymmetry.
                 * Stage 2: lower drive, half bias — cascaded dual-triode
                 * topology like a 12AX7. Creates richer, more complex
                 * harmonic content than a single stage. */

                /* Stage 1 — primary triode */
                s = triodeSaturate(s, drive1, biasAmount);

                /* Even harmonic injection — the warmth.
                 * x * |x| generates pure 2nd harmonic content.
                 * This is the fundamental difference between tube (even harmonics)
                 * and transistor (odd harmonics) distortion character. */
                float harmonic2nd = s * std::fabs(s);
                float dryMix = 1.0f - driveNorm * 0.4f;
                float wetMix = driveNorm * 0.4f;
                s = s * dryMix + harmonic2nd * wetMix;

                /* Stage 2 — second triode (lower drive, half bias) */
                s = triodeSaturate(s, drive2, biasAmount * 0.5f);

                /* Character-dependent HF rolloff.
                 * Vintage tubes have significant high-frequency losses from
                 * Miller capacitance and inter-electrode capacitance.
                 * At full vintage, this provides a natural 6dB cut above 8kHz. */
                if (charNorm > 0.01f) {
                    s = bqTick(charHFCut_[c], s, c);
                }

                /* ── 7. Depth — body/chest resonance enhancement ──
                 * Low shelf boost at the fundamental range of the selected
                 * vocal type. Adds body and chest resonance without muddiness
                 * because the frequency is tuned to the actual vocal range. */
                if (depthNorm > 0.001f) {
                    s = bqTick(depthShelf_[c], s, c);

                    /* Subtle harmonic enhancement in the fundamental range.
                     * Gentle 2nd harmonic in the low-mids makes the voice
                     * sound fuller and more "present in the room". */
                    float depthHarm = bqTick(depthBP_[c], s, c);
                    float harmContent = depthHarm * std::fabs(depthHarm);
                    s += harmContent * depthNorm * 0.15f;
                }

                /* ── 8. Silk — HF smoothing ──
                 * Splits at 4kHz crossover. Above crossover, applies soft
                 * saturation that rounds off harsh peaks while maintaining
                 * clarity. The result is a "silky" top end — even harmonics
                 * replace odd harmonics, and harsh transients are tamed.
                 * This is the opposite of a de-esser: instead of removing
                 * HF energy, it reshapes it to be smoother. */
                if (silkNorm > 0.001f) {
                    float hfSignal = bqTick(silkHP_[c], s, c);
                    float lfSignal = s - hfSignal;

                    /* Soft saturation on HF only — rounds off harsh peaks.
                     * The tanh waveshaper adds subtle even harmonics while
                     * naturally limiting sharp transients above 4kHz. */
                    float satDrive = 1.0f + silkNorm * 0.3f;
                    hfSignal = std::tanh(hfSignal * satDrive) / satDrive;

                    /* Slight HF level reduction — the "silk" attenuation.
                     * Combined with the smoother waveshape, this creates
                     * the perception of polish and refinement on the top end. */
                    float hfAtten = 1.0f - silkNorm * 0.2f;
                    s = lfSignal + hfSignal * hfAtten;
                }

                /* ── 9. Air — ultra-high shelf at 14kHz ──
                 * Adds breath and openness. In a vocal chain this is the
                 * "air" that makes a voice sound alive and present rather
                 * than muffled or closed-off. Placed after silk so the
                 * smoothed HF gets the shelf boost, not the raw signal. */
                s = bqTick(airShelf_[c], s, c);

                /* ── 10. Transformer coloring ──
                 * Iron output transformer model:
                 * - Low-frequency thickening from core saturation
                 * - Soft peak saturation from core hysteresis
                 * - Subtle even harmonics at all frequencies
                 * The transformer is what gives hardware channel strips
                 * their "glue" and weight — it rounds and thickens. */
                if (xfmrNorm > 0.001f) {
                    s = bqTick(xfmrShelf_[c], s, c);
                    float xfmrSat = std::tanh(s * (1.0f + xfmrDrive));
                    s = s * (1.0f - xfmrNorm) + xfmrSat * xfmrNorm;
                }

                /* ── 11. Output level ── */
                s *= outputLin;

                /* Output peak metering */
                float outAbs = std::fabs(s);
                if (outAbs > outputPeak) outputPeak = outAbs;

                pcm[f * channels + c] = s;
            }
        }

        /* Metering (atomic for UI thread) */
        meterInputPeak_.store(
            (inputPeak > 1e-10f) ? 20.0f * std::log10(inputPeak) : -96.0f,
            std::memory_order_relaxed);
        meterOutputPeak_.store(
            (outputPeak > 1e-10f) ? 20.0f * std::log10(outputPeak) : -96.0f,
            std::memory_order_relaxed);
        meterGainReduction_.store(maxGr, std::memory_order_relaxed);
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(hpf_[c]);
            bqClear(deessBP_[c]);
            bqClear(warmthEQ_[c]);
            bqClear(presenceEQ_[c]);
            bqClear(charHFCut_[c]);
            bqClear(depthShelf_[c]);
            bqClear(depthBP_[c]);
            bqClear(silkHP_[c]);
            bqClear(airShelf_[c]);
            bqClear(xfmrShelf_[c]);
            compEnv_[c] = 0.0f;
        }
        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        updateCoeffs();
    }

    /* ── Parameters ──────────────────────────────────────────────────── */

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Input Gain", "Tube Drive", "Tube Character", "Sustain",
            "Vocal Range", "Depth", "Warmth", "Presence",
            "Silk", "De-Ess", "Air", "Low Cut",
            "Transformer", "Output"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dB", "%", "%", "%",
            "", "%", "%", "%",
            "%", "%", "dB", "Hz",
            "%", "dB"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_INPUT_GAIN:      return inputGainDb_ / 60.0f;
            case P_TUBE_DRIVE:      return tubeDrive_ / 100.0f;
            case P_TUBE_CHARACTER:  return tubeCharacter_ / 100.0f;
            case P_SUSTAIN:         return sustain_ / 100.0f;
            case P_VOCAL_RANGE:     return static_cast<float>(vocalRange_) / 4.0f;
            case P_DEPTH:           return depth_ / 100.0f;
            case P_WARMTH:          return warmth_ / 100.0f;
            case P_PRESENCE:        return presence_ / 100.0f;
            case P_SILK:            return silk_ / 100.0f;
            case P_DEESS:           return deess_ / 100.0f;
            case P_AIR:             return airDb_ / 8.0f;
            case P_LOW_CUT:         return (lowCutHz_ - 40.0f) / 260.0f;
            case P_TRANSFORMER:     return transformer_ / 100.0f;
            case P_OUTPUT:          return (outputDb_ + 60.0f) / 70.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        switch (index) {
            case P_INPUT_GAIN:      inputGainDb_ = v * 60.0f; break;
            case P_TUBE_DRIVE:      tubeDrive_ = v * 100.0f; break;
            case P_TUBE_CHARACTER:  tubeCharacter_ = v * 100.0f; updateCoeffs(); break;
            case P_SUSTAIN:         sustain_ = v * 100.0f; break;
            case P_VOCAL_RANGE: {
                int r = static_cast<int>(v * 4.0f + 0.5f);
                if (r < 0) r = 0; else if (r > 4) r = 4;
                vocalRange_ = r;
                updateCoeffs();
                break;
            }
            case P_DEPTH:           depth_ = v * 100.0f; updateCoeffs(); break;
            case P_WARMTH:          warmth_ = v * 100.0f; updateCoeffs(); break;
            case P_PRESENCE:        presence_ = v * 100.0f; updateCoeffs(); break;
            case P_SILK:            silk_ = v * 100.0f; break;
            case P_DEESS:           deess_ = v * 100.0f; updateCoeffs(); break;
            case P_AIR:             airDb_ = v * 8.0f; updateCoeffs(); break;
            case P_LOW_CUT:         lowCutHz_ = v * 260.0f + 40.0f; updateCoeffs(); break;
            case P_TRANSFORMER:     transformer_ = v * 100.0f; updateCoeffs(); break;
            case P_OUTPUT:          outputDb_ = v * 70.0f - 60.0f; break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case P_INPUT_GAIN:      snprintf(buf, 32, "+%.1f dB", inputGainDb_); break;
            case P_TUBE_DRIVE:      snprintf(buf, 32, "%.0f%%", tubeDrive_); break;
            case P_TUBE_CHARACTER:  snprintf(buf, 32, "%.0f%%", tubeCharacter_); break;
            case P_SUSTAIN:         snprintf(buf, 32, "%.0f%%", sustain_); break;
            case P_VOCAL_RANGE: {
                static const char* rangeNames[] = {
                    "Bass", "Baritone", "Tenor", "Alto", "Soprano"
                };
                int r = vocalRange_;
                if (r < 0) r = 0; else if (r > 4) r = 4;
                snprintf(buf, 32, "%s", rangeNames[r]);
                break;
            }
            case P_DEPTH:           snprintf(buf, 32, "%.0f%%", depth_); break;
            case P_WARMTH:          snprintf(buf, 32, "%.0f%%", warmth_); break;
            case P_PRESENCE:        snprintf(buf, 32, "%.0f%%", presence_); break;
            case P_SILK:            snprintf(buf, 32, "%.0f%%", silk_); break;
            case P_DEESS:           snprintf(buf, 32, "%.0f%%", deess_); break;
            case P_AIR:             snprintf(buf, 32, "+%.1f dB", airDb_); break;
            case P_LOW_CUT:         snprintf(buf, 32, "%.0f Hz", lowCutHz_); break;
            case P_TRANSFORMER:     snprintf(buf, 32, "%.0f%%", transformer_); break;
            case P_OUTPUT:          snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* ── Parameters ──────────────────────────────────────────────────── */

    float inputGainDb_      =  25.0f;   /* 0 to +60 dB */
    float tubeDrive_        =  35.0f;   /* 0 to 100% */
    float tubeCharacter_    =  50.0f;   /* 0 to 100% (clean→vintage) */
    float sustain_          =  40.0f;   /* 0 to 100% (optical comp amount) */
    int   vocalRange_       =   2;      /* 0=Bass 1=Bari 2=Tenor 3=Alto 4=Sop */
    float depth_            =  30.0f;   /* 0 to 100% (body/chest) */
    float warmth_           =  50.0f;   /* 0 to 100% (low-mid warmth) */
    float presence_         =  40.0f;   /* 0 to 100% (upper-mid presence) */
    float silk_             =  30.0f;   /* 0 to 100% (HF smoothing) */
    float deess_            =  25.0f;   /* 0 to 100% (sibilance reduction) */
    float airDb_            =   2.0f;   /* 0 to +8 dB */
    float lowCutHz_         =  80.0f;   /* 40 to 300 Hz */
    float transformer_      =  35.0f;   /* 0 to 100% (iron color) */
    float outputDb_         =   0.0f;   /* -60 to +10 dB */

    /* ── Optical compressor envelope state ───────────────────────────── */

    float compEnv_[MAX_CH]  = {};       /* per-channel opto envelope followers */
    float optoAttack_       = 0.0f;     /* slow attack (~20ms) */
    float optoRelease_      = 0.0f;     /* very slow release (~500ms) */

    /* ── Biquad filter ───────────────────────────────────────────────── */

    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };

    BQ hpf_[MAX_CH];               /* high-pass (proximity/rumble control) */
    BQ deessBP_[MAX_CH];           /* de-esser sidechain bandpass */
    BQ warmthEQ_[MAX_CH];          /* vocal warmth peaking EQ */
    BQ presenceEQ_[MAX_CH];        /* vocal presence peaking EQ */
    BQ charHFCut_[MAX_CH];         /* character HF rolloff shelf at 8kHz */
    BQ depthShelf_[MAX_CH];        /* body/chest low-shelf boost */
    BQ depthBP_[MAX_CH];           /* depth harmonic enhancement bandpass */
    BQ silkHP_[MAX_CH];            /* silk crossover high-pass at 4kHz */
    BQ airShelf_[MAX_CH];          /* air high shelf at 14kHz */
    BQ xfmrShelf_[MAX_CH];         /* transformer low shelf at 80Hz */

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch] - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y;
        return y;
    }
    static void bqClear(BQ& f) {
        for(int c=0;c<2;c++) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0;
    }

    /* ── Triode saturation — same model as fx_tube_preamp.h ─────────── */

    static inline float triodeSaturate(float x, float drive, float bias) {
        /* Apply bias offset — shifts the operating point on the tube's
         * transfer curve. Real tubes have a DC bias voltage on the grid
         * that determines the quiescent operating point. Moving it
         * creates asymmetry = even harmonics = warmth. */
        x += bias * 0.3f;

        /* Asymmetric soft clipping — models the triode plate current curve.
         *
         * POSITIVE peaks (grid conduction region):
         *   atan gives smooth, musical saturation with graceful limiting.
         *
         * NEGATIVE peaks (plate current cutoff):
         *   tanh gives a harder characteristic — this asymmetry between
         *   positive and negative generates even harmonics (2nd, 4th). */
        float y;
        if (x >= 0.0f) {
            y = (2.0f / 3.14159265f) * std::atan(x * drive);
        } else {
            y = std::tanh(x * drive * 1.3f);
        }

        return y;
    }

    /* ── RBJ Cookbook filter computations ─────────────────────────────── */

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

    static void computeBP(BQ& f, float freq, float Q, float sr) {
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 = alpha / a0;
        f.b1 = 0.0f;
        f.b2 = -alpha / a0;
        f.a1 = -2.0f * cosw0 / a0;
        f.a2 = (1.0f - alpha) / a0;
    }

    /* ── Coefficient update ──────────────────────────────────────────── */

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        /* Optical compressor time constants.
         * Attack: 20ms — slow enough to let transients through, giving
         * the vocal its natural attack while controlling sustained level.
         * Release: 500ms — very slow to prevent pumping artifacts.
         * This pair creates the classic LA-2A "invisible leveling" behavior. */
        optoAttack_  = 1.0f - std::exp(-1.0f / (20.0f * 0.001f * sr));
        optoRelease_ = 1.0f - std::exp(-1.0f / (500.0f * 0.001f * sr));

        /* Get vocal range configuration for frequency targeting */
        int r = vocalRange_;
        if (r < 0) r = 0; else if (r > 4) r = 4;
        const VocalRangeConfig& vr = kVocalRanges[r];

        /* Warmth: peaking EQ at the vocal range's warmth frequency.
         * Q = 0.8 (wide, musical) — covers the low-mid body of the voice.
         * Up to +8 dB boost, scaled by warmth parameter. */
        float warmthDb = (warmth_ / 100.0f) * 8.0f;

        /* Presence: peaking EQ at the vocal range's presence frequency.
         * Q = 1.2 (moderate bandwidth) — adds intelligibility and cut.
         * Up to +6 dB boost, scaled by presence parameter. */
        float presenceDb = (presence_ / 100.0f) * 6.0f;

        /* Character HF cut: gentle high shelf cut above 8kHz.
         * At full vintage character: -6 dB cut — models Miller capacitance
         * and inter-electrode losses in vintage tubes. */
        float charHFDb = -(tubeCharacter_ / 100.0f) * 6.0f;

        /* Depth: low shelf boost at the vocal range's body frequency.
         * Up to +6 dB, scaled by depth parameter. */
        float depthDb = (depth_ / 100.0f) * 6.0f;

        /* Transformer: low shelf boost at 80Hz for iron thickening.
         * Up to +6 dB at full transformer setting. */
        float xfmrDb = (transformer_ / 100.0f) * 6.0f;

        for (int c = 0; c < MAX_CH; ++c) {
            /* HPF: 2nd-order Butterworth high-pass (proximity/rumble) */
            computeHP(hpf_[c], lowCutHz_, sr);

            /* De-esser: bandpass sidechain at vocal range sibilance freq.
             * Q = 4.0 (narrow) — targets sibilance band precisely. */
            computeBP(deessBP_[c], vr.deessFreq, 4.0f, sr);

            /* Warmth: peaking at vocal range warmth freq, Q=0.8 (wide) */
            computePeaking(warmthEQ_[c], vr.warmthFreq, warmthDb, 0.8f, sr);

            /* Presence: peaking at vocal range presence freq, Q=1.2 */
            computePeaking(presenceEQ_[c], vr.presenceFreq, presenceDb, 1.2f, sr);

            /* Character HF cut: high shelf at 8kHz */
            computeShelf(charHFCut_[c], 8000.0f, charHFDb, sr, false);

            /* Depth: low shelf at vocal range body freq */
            computeShelf(depthShelf_[c], vr.depthFreq, depthDb, sr, true);

            /* Depth harmonic BP: bandpass centered on fundamental range.
             * Uses the midpoint between fundamentalLo and fundamentalHi. */
            float depthBPFreq = (vr.fundamentalLo + vr.fundamentalHi) * 0.5f;
            computeBP(depthBP_[c], depthBPFreq, 1.0f, sr);

            /* Silk: high-pass crossover at 4kHz (Butterworth) */
            computeHP(silkHP_[c], 4000.0f, sr);

            /* Air: high shelf at 14kHz */
            computeShelf(airShelf_[c], 14000.0f, airDb_, sr, false);

            /* Transformer: low shelf at 80Hz */
            computeShelf(xfmrShelf_[c], 80.0f, xfmrDb, sr, true);
        }
    }
};

} // namespace mc1dsp
