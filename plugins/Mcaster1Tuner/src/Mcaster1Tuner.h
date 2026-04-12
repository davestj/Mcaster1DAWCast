/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1Tuner.h — Audio processor (real-time thread)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "PitchDetector.h"
#include "TuningTables.h"

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <atomic>

// Unique IDs (generated UUIDs)
static const Steinberg::FUID kMcaster1TunerProcessorUID(
    0x4D433154, 0x554E4552, 0x50524F43, 0x45535331);  // MC1TUNERPROCES1

static const Steinberg::FUID kMcaster1TunerControllerUID(
    0x4D433154, 0x554E4552, 0x434F4E54, 0x524F4C31);  // MC1TUNERCONTROL1

namespace Mcaster1 {

// Parameter IDs
enum ParamIDs : Steinberg::Vst::ParamID {
    kParamPresetIndex = 100,     // which TuningPreset is selected (0..N-1)
    kParamConcertPitch = 101,    // concert pitch A (430..450 Hz, normalized 0..1)
    kParamBypass = 200,
};

// Message IDs for processor → controller communication
static const char* kMsgPitch      = "Pitch";
static const char* kMsgConfidence = "Confidence";
static const char* kMsgNoteName   = "NoteName";
static const char* kMsgCents      = "Cents";

class Mcaster1TunerProcessor : public Steinberg::Vst::AudioEffect
{
public:
    Mcaster1TunerProcessor();
    ~Mcaster1TunerProcessor() override = default;

    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(
            new Mcaster1TunerProcessor());
    }

    // AudioEffect overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup& newSetup) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override;
    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData& data) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize) override;

    // State
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;

private:
    PitchDetector m_pitchDetector{2048, 0.15f};
    int   m_presetIndex = 0;
    float m_concertPitch = 440.0f;
    bool  m_bypass = false;

    // Send pitch data to controller via message
    void sendPitchMessage(float pitch, float confidence);
};

} // namespace Mcaster1
