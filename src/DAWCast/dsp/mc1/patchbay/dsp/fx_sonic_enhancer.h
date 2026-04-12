/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_sonic_enhancer.h — BBE 882I Sonic Maximizer Clone
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * 3-band phase-corrective processor:
 *   Input → LR4 crossover (150Hz, 1.2kHz)
 *         → Phase delay alignment (low ~2.5ms, mid ~0.5ms)
 *         → Lo Contour (low-shelf boost at 80Hz)
 *         → Process (high-shelf presence + soft saturation at 3kHz)
 *         → Recombine → Output gain
 */

#pragma once

#include "dsp_effect.h"
#include <array>
#include <vector>
#include <cmath>

namespace mc1dsp {

class FxSonicEnhancer : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr int MAX_CH = 2;

    enum Param { P_LO_CONTOUR = 0, P_PROCESS, P_OUTPUT_GAIN, P_COUNT };

    FxSonicEnhancer() { initDelays(48000); computeFilters(); }

    const char* name()     const override { return "Sonic Enhancer (BBE 882I)"; }
    const char* id()       const override { return "mc1.enhancer.sonic"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::Enhancer; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;
        int ch = std::min(channels, MAX_CH);

        for (size_t f = 0; f < frames; ++f) {
            for (int c = 0; c < ch; ++c) {
                float in = pcm[f * channels + c];

                /* 3-band split via LR4 crossover */
                float lp1 = bqTick(xLP1a_[c], bqTick(xLP1b_[c], in, c), c);
                float hp1 = in - lp1;  /* complementary HP */
                float lp2 = bqTick(xLP2a_[c], bqTick(xLP2b_[c], hp1, c), c);
                float hp2 = hp1 - lp2;

                float low  = lp1;
                float mid  = lp2;
                float high = hp2;

                /* Phase alignment via delay */
                low = delayTick(delayLo_[c], low);
                mid = delayTick(delayMid_[c], mid);

                /* Lo Contour: low-shelf boost */
                low = bqTick(loShelf_[c], low, c);
                low *= loContourGain_;

                /* Process: presence shelf + soft saturation */
                high = bqTick(hiShelf_[c], high, c);
                high *= processGain_;
                high = std::tanh(high);  /* soft clip / harmonic exciter */

                /* Recombine */
                float out = (low + mid + high) * outputLin_;
                pcm[f * channels + c] = out;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < MAX_CH; ++c) {
            bqClear(xLP1a_[c]); bqClear(xLP1b_[c]);
            bqClear(xLP2a_[c]); bqClear(xLP2b_[c]);
            bqClear(loShelf_[c]); bqClear(hiShelf_[c]);
            delayClear(delayLo_[c]); delayClear(delayMid_[c]);
        }
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        initDelays(sr);
        computeFilters();
    }

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int i) const override {
        static const char* n[] = { "Lo Contour", "Process", "Output Gain" };
        return (i >= 0 && i < P_COUNT) ? n[i] : "";
    }
    const char* paramUnit(int i) const override {
        return (i == P_OUTPUT_GAIN) ? "dB" : "";
    }

    float paramValue(int i) const override {
        switch (i) {
            case P_LO_CONTOUR:  return loContour_ / 10.0f;
            case P_PROCESS:     return process_ / 10.0f;
            case P_OUTPUT_GAIN: return (outputDb_ + 12.0f) / 18.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int i, float v) override {
        switch (i) {
            case P_LO_CONTOUR:
                loContour_ = v * 10.0f;
                loContourGain_ = 1.0f + loContour_ * 0.12f;  /* 0-10 → 1.0-2.2 */
                computeFilters();
                break;
            case P_PROCESS:
                process_ = v * 10.0f;
                processGain_ = 1.0f + process_ * 0.12f;
                computeFilters();
                break;
            case P_OUTPUT_GAIN:
                outputDb_ = v * 18.0f - 12.0f;
                outputLin_ = std::pow(10.0f, outputDb_ / 20.0f);
                break;
        }
    }

    std::string paramDisplayValue(int i) const override {
        char buf[32];
        switch (i) {
            case P_LO_CONTOUR:  snprintf(buf, 32, "%.1f", loContour_); break;
            case P_PROCESS:     snprintf(buf, 32, "%.1f", process_); break;
            case P_OUTPUT_GAIN: snprintf(buf, 32, "%+.1f dB", outputDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    float loContour_ = 5.0f, process_ = 5.0f, outputDb_ = 0.0f;
    float loContourGain_ = 1.6f, processGain_ = 1.6f, outputLin_ = 1.0f;

    /* ── Simple biquad ── */
    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };
    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x + f.b1*f.x1[ch] + f.b2*f.x2[ch] - f.a1*f.y1[ch] - f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y;
        return y;
    }
    static void bqClear(BQ& f) {
        for(int c=0;c<2;c++) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0;
    }

    /* ── Delay line ── */
    struct DL { std::vector<float> buf; size_t wp=0, delay=0; };
    void initDelays(int sr) {
        size_t maxD = static_cast<size_t>(sr * 0.003) + 1;  /* 3ms max */
        for (int c = 0; c < MAX_CH; ++c) {
            delayLo_[c].buf.assign(maxD, 0.0f); delayLo_[c].wp = 0;
            delayLo_[c].delay = static_cast<size_t>(sr * 0.0025);  /* 2.5ms */
            delayMid_[c].buf.assign(maxD, 0.0f); delayMid_[c].wp = 0;
            delayMid_[c].delay = static_cast<size_t>(sr * 0.0005); /* 0.5ms */
        }
    }
    static float delayTick(DL& d, float in) {
        d.buf[d.wp] = in;
        size_t rd = (d.wp + d.buf.size() - d.delay) % d.buf.size();
        float out = d.buf[rd];
        d.wp = (d.wp + 1) % d.buf.size();
        return out;
    }
    static void delayClear(DL& d) {
        std::fill(d.buf.begin(), d.buf.end(), 0.0f); d.wp = 0;
    }

    /* Crossover + shelf filters per channel */
    std::array<BQ, MAX_CH> xLP1a_{}, xLP1b_{}, xLP2a_{}, xLP2b_{};
    std::array<BQ, MAX_CH> loShelf_{}, hiShelf_{};
    std::array<DL, MAX_CH> delayLo_{}, delayMid_{};

    void computeFilters() {
        float sr = static_cast<float>(sampleRate_);
        for (int c = 0; c < MAX_CH; ++c) {
            computeLP(xLP1a_[c], 150.0f, sr);
            computeLP(xLP1b_[c], 150.0f, sr);
            computeLP(xLP2a_[c], 1200.0f, sr);
            computeLP(xLP2b_[c], 1200.0f, sr);
            computeShelf(loShelf_[c], 80.0f, loContour_ * 1.2f, sr, true);
            computeShelf(hiShelf_[c], 3000.0f, process_ * 1.2f, sr, false);
        }
    }

    static void computeLP(BQ& f, float freq, float sr) {
        float w0 = 2.0f * 3.14159265f * freq / sr;
        float alpha = std::sin(w0) / (2.0f * 0.7071f);  /* Q=sqrt(2)/2 Butterworth */
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        f.b0 = (1.0f - cosw0) / 2.0f / a0;
        f.b1 = (1.0f - cosw0) / a0;
        f.b2 = f.b0;
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
};

} // namespace mc1dsp
