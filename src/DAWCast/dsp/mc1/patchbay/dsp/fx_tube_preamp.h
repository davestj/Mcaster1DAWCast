/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_tube_preamp.h — Tube Mic Preamp (Warm Saturation Model)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Models the nonlinear behavior of vacuum tube amplification stages:
 *   - Asymmetric soft clipping (triode transfer curve)
 *   - Even harmonic generation (2nd + 4th harmonics = warmth)
 *   - Grid conduction modeling (soft positive peak compression)
 *   - Plate voltage sag (dynamic compression at high drive)
 *   - Transformer coloring (subtle low-frequency thickening)
 *   - Presence peak (tube amp presence circuit at ~3-5kHz)
 *
 * Signal chain:
 *   Input Gain → HPF (proximity control) → Tube Stage 1 (triode)
 *   → Tube Stage 2 (triode) → Transformer → Presence EQ → Output
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>

namespace mc1dsp {

class FxTubePreamp : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    /* Parameter indices */
    enum Param {
        P_INPUT_GAIN = 0,   // 0 to +60 dB
        P_DRIVE,            // 0 to 100%
        P_WARMTH,           // 0 to 100%
        P_PRESENCE,         // -6 to +6 dB
        P_LOW_CUT,          // 20 to 300 Hz
        P_TRANSFORMER,      // 0 to 100%
        P_BIAS,             // -50 to +50%
        P_SAG,              // 0 to 100%
        P_AIR,              // 0 to +12 dB
        P_OUTPUT,           // -60 to +10 dB
        P_COUNT
    };

    FxTubePreamp() { updateCoeffs(); }

    /* ── DspEffect interface ─────────────────────────────────────── */

    const char* name()     const override { return "Tube Mic Preamp"; }
    const char* id()       const override { return "mc1.analog.tube_preamp"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        int ch = (channels < MAX_CH) ? channels : MAX_CH;

        /* Pre-compute linear gains and derived parameters */
        float inputGainLin = std::pow(10.0f, inputGainDb_ / 20.0f);
        float outputLin    = std::pow(10.0f, outputDb_ / 20.0f);

        /* Drive maps 0-100% to a musically useful saturation range.
         * Stage 1 gets full drive, stage 2 gets ~40% for a cascaded
         * two-triode topology (like a 12AX7 dual triode). */
        float drive1 = 1.0f + (drive_ / 100.0f) * 5.0f;   /* 1.0 – 6.0 */
        float drive2 = 1.0f + (drive_ / 100.0f) * 2.0f;    /* 1.0 – 3.0 */

        float warmthNorm = warmth_ / 100.0f;                /* 0.0 – 1.0 */
        float biasNorm   = bias_ / 100.0f;                  /* -0.5 – 0.5 */
        float xfmrAmount = transformer_ / 100.0f;           /* 0.0 – 1.0 */
        float xfmrDrive  = xfmrAmount * 0.5f;               /* subtle */
        float sagAmount  = sag_ / 100.0f;                   /* 0.0 – 1.0 */

        float inputPeak  = 0.0f;
        float outputPeak = 0.0f;
        float maxSagGr   = 0.0f;  /* track max sag gain reduction for metering */

        for (size_t f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float s = pcm[f * channels + c];

                /* 1. Input Gain */
                s *= inputGainLin;

                /* Input peak metering (pre-tube) */
                float inAbs = std::fabs(s);
                if (inAbs > inputPeak) inputPeak = inAbs;

                /* 2. HPF — proximity control (2nd-order Butterworth) */
                s = bqTick(hpf_[c], s, c);

                /* 3. Sag envelope follower — track input level before saturation.
                 *    The envelope drives dynamic gain reduction, simulating the
                 *    power supply voltage dip in a tube amp under heavy signal. */
                float peak = std::fabs(s);
                float sagCoeff = (peak > sagEnv_[c]) ? sagAttack_ : sagRelease_;
                sagEnv_[c] += sagCoeff * (peak - sagEnv_[c]);

                /* 4. Tube Stage 1 — first triode (higher drive) */
                s = triodeSaturate(s, drive1, biasNorm);

                /* 5. Even harmonic injection — the "warmth".
                 *    x * |x| generates pure 2nd harmonic content.
                 *    Blend with dry signal using warmth parameter.
                 *    This is the secret sauce — even harmonics are what make
                 *    tube gear sound warm versus transistor gear (odd harmonics). */
                float harmonic2nd = s * std::fabs(s);
                float dryMix = 1.0f - warmthNorm * 0.5f;
                float wetMix = warmthNorm * 0.5f;
                s = s * dryMix + harmonic2nd * wetMix;

                /* 6. Tube Stage 2 — second triode (lower drive, half bias).
                 *    Cascaded triode stages in a real preamp create richer
                 *    harmonic content and smoother compression character. */
                s = triodeSaturate(s, drive2, biasNorm * 0.5f);

                /* 7. Transformer coloring.
                 *    Real output transformers add:
                 *    - Subtle low-frequency thickening (iron core saturation)
                 *    - Soft saturation at signal peaks (core hysteresis)
                 *    The shelf boost + tanh saturation models both behaviors. */
                s = bqTick(xfmrShelf_[c], s, c);
                float xfmrSat = std::tanh(s * (1.0f + xfmrDrive));
                s = s * (1.0f - xfmrAmount) + xfmrSat * xfmrAmount;

                /* 8. Plate voltage sag — dynamic compression.
                 *    When input signal is loud, the envelope follower rises,
                 *    reducing gain — exactly like a tube amp's B+ voltage
                 *    sagging under heavy current draw. Creates musical,
                 *    program-dependent compression. */
                float sagGain = 1.0f / (1.0f + sagEnv_[c] * sagAmount * 3.0f);
                s *= sagGain;

                /* Track sag GR for metering */
                float sagGr = 1.0f - sagGain;
                if (sagGr > maxSagGr) maxSagGr = sagGr;

                /* 9. Presence EQ — peaking at 3.5kHz.
                 *    Models the presence control found on classic tube amps.
                 *    Adds cut-through and articulation to vocals. */
                s = bqTick(presence_[c], s, c);

                /* 10. Air EQ — high shelf at 12kHz.
                 *     Adds sheen and openness ("air") to the top end.
                 *     Classic tube preamps often have a subtle HF lift
                 *     from inter-stage coupling capacitor resonance. */
                s = bqTick(airShelf_[c], s, c);

                /* 11. Output level */
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
        /* Sag GR: convert the gain reduction ratio to dB.
         * sagGain = 1/(1+env*amount*3), so GR in dB = 20*log10(sagGain).
         * We stored maxSagGr = 1-sagGain, so sagGain = 1-maxSagGr. */
        float sagGainMin = 1.0f - maxSagGr;
        if (sagGainMin < 0.001f) sagGainMin = 0.001f;
        float grDb = -20.0f * std::log10(sagGainMin);
        meterGainReduction_.store(grDb, std::memory_order_relaxed);
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(hpf_[c]);
            bqClear(xfmrShelf_[c]);
            bqClear(presence_[c]);
            bqClear(airShelf_[c]);
            sagEnv_[c] = 0.0f;
        }
        meterGainReduction_.store(0.0f);
        meterInputPeak_.store(-96.0f);
        meterOutputPeak_.store(-96.0f);
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        updateCoeffs();
    }

    /* ── Parameters ──────────────────────────────────────────────── */

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int index) const override {
        static const char* names[] = {
            "Input Gain", "Drive", "Warmth", "Presence",
            "Low Cut", "Transformer", "Bias", "Sag",
            "Air", "Output"
        };
        return (index >= 0 && index < P_COUNT) ? names[index] : "";
    }

    const char* paramUnit(int index) const override {
        static const char* units[] = {
            "dB", "%", "%", "dB", "Hz", "%", "%", "%", "dB", "dB"
        };
        return (index >= 0 && index < P_COUNT) ? units[index] : "";
    }

    float paramValue(int index) const override {
        switch (index) {
            case P_INPUT_GAIN:   return inputGainDb_ / 60.0f;
            case P_DRIVE:        return drive_ / 100.0f;
            case P_WARMTH:       return warmth_ / 100.0f;
            case P_PRESENCE:     return (presenceDb_ + 6.0f) / 12.0f;
            case P_LOW_CUT:      return (lowCutHz_ - 20.0f) / 280.0f;
            case P_TRANSFORMER:  return transformer_ / 100.0f;
            case P_BIAS:         return (bias_ + 50.0f) / 100.0f;
            case P_SAG:          return sag_ / 100.0f;
            case P_AIR:          return airDb_ / 12.0f;
            case P_OUTPUT:       return (outputDb_ + 60.0f) / 70.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int index, float v) override {
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        switch (index) {
            case P_INPUT_GAIN:   inputGainDb_ = v * 60.0f; break;
            case P_DRIVE:        drive_ = v * 100.0f; break;
            case P_WARMTH:       warmth_ = v * 100.0f; break;
            case P_PRESENCE:     presenceDb_ = v * 12.0f - 6.0f; updateCoeffs(); break;
            case P_LOW_CUT:      lowCutHz_ = v * 280.0f + 20.0f; updateCoeffs(); break;
            case P_TRANSFORMER:  transformer_ = v * 100.0f; updateCoeffs(); break;
            case P_BIAS:         bias_ = v * 100.0f - 50.0f; break;
            case P_SAG:          sag_ = v * 100.0f; break;
            case P_AIR:          airDb_ = v * 12.0f; updateCoeffs(); break;
            case P_OUTPUT:       outputDb_ = v * 70.0f - 60.0f; break;
        }
    }

    std::string paramDisplayValue(int index) const override {
        char buf[32];
        switch (index) {
            case P_INPUT_GAIN:   snprintf(buf, 32, "+%.1f dB", inputGainDb_); break;
            case P_DRIVE:        snprintf(buf, 32, "%.0f%%", drive_); break;
            case P_WARMTH:       snprintf(buf, 32, "%.0f%%", warmth_); break;
            case P_PRESENCE:     snprintf(buf, 32, "%+.1f dB", presenceDb_); break;
            case P_LOW_CUT:      snprintf(buf, 32, "%.0f Hz", lowCutHz_); break;
            case P_TRANSFORMER:  snprintf(buf, 32, "%.0f%%", transformer_); break;
            case P_BIAS:         snprintf(buf, 32, "%+.0f%%", bias_); break;
            case P_SAG:          snprintf(buf, 32, "%.0f%%", sag_); break;
            case P_AIR:          snprintf(buf, 32, "+%.1f dB", airDb_); break;
            case P_OUTPUT:       snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    /* ── Parameters ──────────────────────────────────────────────── */

    float inputGainDb_  =  20.0f;   /* 0 to +60 dB */
    float drive_        =  30.0f;   /* 0 to 100% */
    float warmth_       =  50.0f;   /* 0 to 100% */
    float presenceDb_   =   2.0f;   /* -6 to +6 dB */
    float lowCutHz_     =  80.0f;   /* 20 to 300 Hz */
    float transformer_  =  40.0f;   /* 0 to 100% */
    float bias_         =   0.0f;   /* -50 to +50% */
    float sag_          =  20.0f;   /* 0 to 100% */
    float airDb_        =   1.0f;   /* 0 to +12 dB */
    float outputDb_     =   0.0f;   /* -60 to +10 dB */

    /* ── Sag envelope state ──────────────────────────────────────── */

    float sagEnv_[MAX_CH]   = {};   /* per-channel envelope followers */
    float sagAttack_        = 0.0f; /* fast attack (~5ms) */
    float sagRelease_       = 0.0f; /* slow release (~200ms) */

    /* ── Biquad filter ───────────────────────────────────────────── */

    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };

    BQ hpf_[MAX_CH];           /* high-pass (proximity control) */
    BQ xfmrShelf_[MAX_CH];    /* transformer low shelf @ 80Hz */
    BQ presence_[MAX_CH];      /* presence peaking @ 3.5kHz */
    BQ airShelf_[MAX_CH];     /* air high shelf @ 12kHz */

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch] - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y;
        return y;
    }
    static void bqClear(BQ& f) {
        for(int c=0;c<2;c++) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0;
    }

    /* ── Triode saturation — the heart of the tube sound ─────────── */

    static inline float triodeSaturate(float x, float drive, float bias) {
        /* Apply bias offset — shifts the operating point on the tube's
         * transfer curve. Real tubes have a DC bias voltage on the grid
         * that determines the quiescent operating point. Moving it
         * creates asymmetry = even harmonics = warmth. */
        x += bias * 0.3f;

        /* Asymmetric soft clipping — models the triode plate current curve.
         *
         * POSITIVE peaks (grid conduction region):
         *   When signal drives the grid positive, grid current flows and
         *   the tube compresses softly. atan gives smooth, musical saturation
         *   with graceful limiting — the "warm" quality of tube clipping.
         *
         * NEGATIVE peaks (plate current cutoff):
         *   When signal drives the grid far negative, plate current cuts off
         *   sharply. tanh gives a harder characteristic than atan, with a
         *   steeper approach to the limit — this asymmetry between positive
         *   and negative is what generates even harmonics (2nd, 4th). */
        float y;
        if (x >= 0.0f) {
            y = (2.0f / 3.14159265f) * std::atan(x * drive);
        } else {
            y = std::tanh(x * drive * 1.3f);
        }

        return y;
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

    /* ── Coefficient update ──────────────────────────────────────── */

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);

        /* Sag envelope time constants.
         * Fast attack (5ms) lets the sag respond quickly to transients.
         * Slow release (200ms) creates the lingering compression feel
         * that makes tube amps sound "alive" and breathing. */
        sagAttack_  = 1.0f - std::exp(-1.0f / (5.0f * 0.001f * sr));
        sagRelease_ = 1.0f - std::exp(-1.0f / (200.0f * 0.001f * sr));

        /* Transformer low shelf — iron core thickening.
         * The amount of boost scales with the transformer knob:
         * 0% = flat, 100% = +6dB at 80Hz (subtle but audible body). */
        float xfmrBoostDb = (transformer_ / 100.0f) * 6.0f;

        for (int c = 0; c < MAX_CH; ++c) {
            /* HPF: 2nd-order Butterworth high-pass (proximity control) */
            computeHP(hpf_[c], lowCutHz_, sr);

            /* Transformer: low shelf at 80Hz */
            computeShelf(xfmrShelf_[c], 80.0f, xfmrBoostDb, sr, true);

            /* Presence: peaking at 3.5kHz, Q=1.2 (musical bandwidth).
             * Q=1.2 gives a ~1 octave wide peak — broad enough to add
             * presence without sounding ringy or nasal. */
            computePeaking(presence_[c], 3500.0f, presenceDb_, 1.2f, sr);

            /* Air: high shelf at 12kHz */
            computeShelf(airShelf_[c], 12000.0f, airDb_, sr, false);
        }
    }
};

} // namespace mc1dsp
