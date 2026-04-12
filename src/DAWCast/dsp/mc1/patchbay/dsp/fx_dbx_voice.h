/*
 * Mcaster1PatchBay — DSP Effects Plugin
 * dsp/fx_dbx_voice.h — DBX 286S Voice Processor Clone (5-section channel strip)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Plugin Version: 1.0.0
 *
 * Processing chain: Gate → Compressor → De-Esser → LF Enhancer → HF Detail
 * Each section individually bypassable.
 */

#pragma once

#include "dsp_effect.h"
#include <cmath>
#include <algorithm>

namespace mc1dsp {

class FxDbxVoice : public DspEffect {
public:
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    enum Param {
        P_GATE_THRESH = 0, P_GATE_RATIO,
        P_COMP_THRESH, P_COMP_RATIO, P_COMP_ATTACK, P_COMP_RELEASE,
        P_DEESS_FREQ, P_DEESS_THRESH,
        P_LF_GAIN, P_HF_GAIN,
        P_COUNT
    };

    FxDbxVoice() { updateCoeffs(); }

    const char* name()     const override { return "DBX 286S Voice Processor"; }
    const char* id()       const override { return "mc1.channel.dbx286s"; }
    const char* version()  const override { return PLUGIN_VERSION; }
    EffectCategory category() const override { return EffectCategory::ChannelStrip; }

    void process(float* pcm, size_t frames, int channels) override {
        if (!isEnabled()) return;

        for (size_t f = 0; f < frames; ++f) {
            for (int ch = 0; ch < channels && ch < 2; ++ch) {
                float s = pcm[f * channels + ch];

                /* 1. Gate/Expander */
                float peakDb = (std::fabs(s) > 1e-10f) ? 20.0f * std::log10(std::fabs(s)) : -96.0f;
                if (peakDb < gateThreshDb_) {
                    float reduction = (gateThreshDb_ - peakDb) * (1.0f - 1.0f / gateRatio_);
                    s *= std::pow(10.0f, -reduction / 20.0f);
                }

                /* 2. Compressor */
                if (peakDb > compThreshDb_) {
                    float over = peakDb - compThreshDb_;
                    float gr = over * (1.0f - 1.0f / compRatio_);
                    float target = std::pow(10.0f, -gr / 20.0f);
                    float coeff = (target < compEnv_[ch]) ? compAttCoeff_ : compRelCoeff_;
                    compEnv_[ch] += coeff * (target - compEnv_[ch]);
                    s *= compEnv_[ch];
                    meterGainReduction_.store(gr, std::memory_order_relaxed);
                }

                /* 3. De-Esser (bandpass sidechain at deessFreq) */
                float bp = bqTick(deessBP_[ch], s, ch);
                float bpLevel = std::fabs(bp);
                float bpDb = (bpLevel > 1e-10f) ? 20.0f * std::log10(bpLevel) : -96.0f;
                if (bpDb > deessThreshDb_) {
                    float att = std::min(6.0f, bpDb - deessThreshDb_);
                    s *= std::pow(10.0f, -att / 20.0f);
                }

                /* 4. LF Enhancer (low shelf at 120Hz) */
                s = bqTick(lfShelf_[ch], s, ch);

                /* 5. HF Detail (high shelf at 4kHz) */
                s = bqTick(hfShelf_[ch], s, ch);

                pcm[f * channels + ch] = s;
            }
        }
    }

    void reset() override {
        for (int c = 0; c < 2; ++c) {
            compEnv_[c] = 1.0f;
            bqClear(deessBP_[c]); bqClear(lfShelf_[c]); bqClear(hfShelf_[c]);
        }
    }

    void setSampleRate(int sr) override {
        DspEffect::setSampleRate(sr);
        updateCoeffs();
    }

    int paramCount() const override { return P_COUNT; }

    const char* paramName(int i) const override {
        static const char* n[] = {
            "Gate Threshold", "Gate Ratio",
            "Comp Threshold", "Comp Ratio", "Comp Attack", "Comp Release",
            "De-Ess Frequency", "De-Ess Threshold",
            "LF Enhance", "HF Detail"
        };
        return (i >= 0 && i < P_COUNT) ? n[i] : "";
    }

    const char* paramUnit(int i) const override {
        switch (i) {
            case P_GATE_THRESH: case P_COMP_THRESH: case P_DEESS_THRESH: return "dBFS";
            case P_GATE_RATIO: case P_COMP_RATIO: return ":1";
            case P_COMP_ATTACK: case P_COMP_RELEASE: return "ms";
            case P_DEESS_FREQ: return "Hz";
            case P_LF_GAIN: case P_HF_GAIN: return "dB";
            default: return "";
        }
    }

    float paramValue(int i) const override {
        switch (i) {
            case P_GATE_THRESH:  return (gateThreshDb_ + 80.0f) / 80.0f;
            case P_GATE_RATIO:   return (gateRatio_ - 1.0f) / 9.0f;
            case P_COMP_THRESH:  return (compThreshDb_ + 60.0f) / 60.0f;
            case P_COMP_RATIO:   return (compRatio_ - 1.0f) / 19.0f;
            case P_COMP_ATTACK:  return (compAttackMs_ - 0.1f) / 99.9f;
            case P_COMP_RELEASE: return (compReleaseMs_ - 10.0f) / 990.0f;
            case P_DEESS_FREQ:   return (deessFreq_ - 2000.0f) / 10000.0f;
            case P_DEESS_THRESH: return (deessThreshDb_ + 60.0f) / 60.0f;
            case P_LF_GAIN:      return lfGainDb_ / 12.0f;
            case P_HF_GAIN:      return hfGainDb_ / 12.0f;
            default: return 0.0f;
        }
    }

    void setParamValue(int i, float v) override {
        switch (i) {
            case P_GATE_THRESH:  gateThreshDb_ = v * 80.0f - 80.0f; break;
            case P_GATE_RATIO:   gateRatio_ = v * 9.0f + 1.0f; break;
            case P_COMP_THRESH:  compThreshDb_ = v * 60.0f - 60.0f; break;
            case P_COMP_RATIO:   compRatio_ = v * 19.0f + 1.0f; break;
            case P_COMP_ATTACK:  compAttackMs_ = v * 99.9f + 0.1f; updateCoeffs(); break;
            case P_COMP_RELEASE: compReleaseMs_ = v * 990.0f + 10.0f; updateCoeffs(); break;
            case P_DEESS_FREQ:   deessFreq_ = v * 10000.0f + 2000.0f; updateCoeffs(); break;
            case P_DEESS_THRESH: deessThreshDb_ = v * 60.0f - 60.0f; break;
            case P_LF_GAIN:      lfGainDb_ = v * 12.0f; updateCoeffs(); break;
            case P_HF_GAIN:      hfGainDb_ = v * 12.0f; updateCoeffs(); break;
        }
    }

    std::string paramDisplayValue(int i) const override {
        char buf[32];
        switch (i) {
            case P_GATE_THRESH:  snprintf(buf, 32, "%.1f dBFS", gateThreshDb_); break;
            case P_GATE_RATIO:   snprintf(buf, 32, "%.1f:1", gateRatio_); break;
            case P_COMP_THRESH:  snprintf(buf, 32, "%.1f dBFS", compThreshDb_); break;
            case P_COMP_RATIO:   snprintf(buf, 32, "%.1f:1", compRatio_); break;
            case P_COMP_ATTACK:  snprintf(buf, 32, "%.1f ms", compAttackMs_); break;
            case P_COMP_RELEASE: snprintf(buf, 32, "%.0f ms", compReleaseMs_); break;
            case P_DEESS_FREQ:   snprintf(buf, 32, "%.0f Hz", deessFreq_); break;
            case P_DEESS_THRESH: snprintf(buf, 32, "%.1f dBFS", deessThreshDb_); break;
            case P_LF_GAIN:      snprintf(buf, 32, "%+.1f dB", lfGainDb_); break;
            case P_HF_GAIN:      snprintf(buf, 32, "%+.1f dB", hfGainDb_); break;
            default: buf[0] = 0;
        }
        return buf;
    }

private:
    float gateThreshDb_ = -40.0f, gateRatio_ = 4.0f;
    float compThreshDb_ = -20.0f, compRatio_ = 4.0f;
    float compAttackMs_ = 5.0f, compReleaseMs_ = 150.0f;
    float deessFreq_ = 6000.0f, deessThreshDb_ = -20.0f;
    float lfGainDb_ = 3.0f, hfGainDb_ = 3.0f;
    float compAttCoeff_ = 0.0f, compRelCoeff_ = 0.0f;
    float compEnv_[2] = {1.0f, 1.0f};

    struct BQ { float b0=1,b1=0,b2=0,a1=0,a2=0; float x1[2]={},x2[2]={},y1[2]={},y2[2]={}; };
    BQ deessBP_[2], lfShelf_[2], hfShelf_[2];

    static float bqTick(BQ& f, float x, int ch) {
        float y = f.b0*x+f.b1*f.x1[ch]+f.b2*f.x2[ch]-f.a1*f.y1[ch]-f.a2*f.y2[ch];
        f.x2[ch]=f.x1[ch]; f.x1[ch]=x; f.y2[ch]=f.y1[ch]; f.y1[ch]=y; return y;
    }
    static void bqClear(BQ& f) { for(int c=0;c<2;c++) f.x1[c]=f.x2[c]=f.y1[c]=f.y2[c]=0; }

    void updateCoeffs() {
        float sr = static_cast<float>(sampleRate_);
        compAttCoeff_ = 1.0f - std::exp(-1.0f / (compAttackMs_ * 0.001f * sr));
        compRelCoeff_ = 1.0f - std::exp(-1.0f / (compReleaseMs_ * 0.001f * sr));

        for (int c = 0; c < 2; ++c) {
            computeBP(deessBP_[c], deessFreq_, 4.0f, sr);
            computeShelf(lfShelf_[c], 120.0f, lfGainDb_, sr, true);
            computeShelf(hfShelf_[c], 4000.0f, hfGainDb_, sr, false);
        }
    }

    static void computeBP(BQ& f, float freq, float Q, float sr) {
        float w0=2*3.14159265f*freq/sr, alpha=std::sin(w0)/(2*Q), cosw0=std::cos(w0);
        float a0=1+alpha;
        f.b0=alpha/a0; f.b1=0; f.b2=-alpha/a0; f.a1=-2*cosw0/a0; f.a2=(1-alpha)/a0;
    }

    static void computeShelf(BQ& f, float freq, float gainDb, float sr, bool low) {
        float A=std::pow(10.0f,gainDb/40.0f),w0=2*3.14159265f*freq/sr;
        float alpha=std::sin(w0)/(2*0.707f),cosw0=std::cos(w0),sqA=std::sqrt(A),a0;
        if(low){
            f.b0=A*((A+1)-(A-1)*cosw0+2*sqA*alpha);f.b1=2*A*((A-1)-(A+1)*cosw0);
            f.b2=A*((A+1)-(A-1)*cosw0-2*sqA*alpha);a0=(A+1)+(A-1)*cosw0+2*sqA*alpha;
            f.a1=-2*((A-1)+(A+1)*cosw0);f.a2=(A+1)+(A-1)*cosw0-2*sqA*alpha;
        }else{
            f.b0=A*((A+1)+(A-1)*cosw0+2*sqA*alpha);f.b1=-2*A*((A-1)+(A+1)*cosw0);
            f.b2=A*((A+1)+(A-1)*cosw0-2*sqA*alpha);a0=(A+1)-(A-1)*cosw0+2*sqA*alpha;
            f.a1=2*((A-1)-(A+1)*cosw0);f.a2=(A+1)-(A-1)*cosw0-2*sqA*alpha;
        }
        f.b0/=a0;f.b1/=a0;f.b2/=a0;f.a1/=a0;f.a2/=a0;
    }
};

} // namespace mc1dsp
