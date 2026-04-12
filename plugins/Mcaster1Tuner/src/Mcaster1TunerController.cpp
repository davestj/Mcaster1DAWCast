/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1TunerController.cpp — Edit controller implementation
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Mcaster1TunerController.h"
#include "PitchDetector.h"
#include "TuningTables.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstmessage.h"

#include <cstring>

namespace Mcaster1 {

// ===================================================================
//  initialize / terminate
// ===================================================================

Steinberg::tresult PLUGIN_API Mcaster1TunerController::initialize(
    Steinberg::FUnknown* context)
{
    auto result = EditController::initialize(context);
    if (result != Steinberg::kResultOk)
        return result;

    // Preset selector: 0..1 mapped to preset indices
    parameters.addParameter(
        STR16("Tuning Preset"), STR16(""),
        TuningTables::presetCount() - 1, 0,
        Steinberg::Vst::ParameterInfo::kCanAutomate,
        kParamPresetIndex);

    // Concert pitch: 0..1 -> 430..450 Hz
    parameters.addParameter(
        STR16("Concert Pitch"), STR16("Hz"),
        0, 0.5,   // default = 440 Hz -> (440-430)/20 = 0.5
        Steinberg::Vst::ParameterInfo::kCanAutomate,
        kParamConcertPitch);

    // Bypass
    parameters.addParameter(
        STR16("Bypass"), STR16(""),
        1, 0,
        Steinberg::Vst::ParameterInfo::kCanAutomate |
        Steinberg::Vst::ParameterInfo::kIsBypass,
        kParamBypass);

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Mcaster1TunerController::terminate()
{
    m_editorView = nullptr;
    return EditController::terminate();
}

// ===================================================================
//  setComponentState
// ===================================================================

Steinberg::tresult PLUGIN_API Mcaster1TunerController::setComponentState(
    Steinberg::IBStream* state)
{
    if (!state) return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(state, kLittleEndian);

    Steinberg::int32 presetIdx = 0;
    if (!streamer.readInt32(presetIdx))
        return Steinberg::kResultFalse;
    setParamNormalized(kParamPresetIndex,
        static_cast<double>(presetIdx) / (TuningTables::presetCount() - 1));

    float concertPitch = 440.0f;
    if (!streamer.readFloat(concertPitch))
        return Steinberg::kResultFalse;
    setParamNormalized(kParamConcertPitch,
        static_cast<double>(concertPitch - 430.0f) / 20.0);

    Steinberg::int32 bypass = 0;
    if (!streamer.readInt32(bypass))
        return Steinberg::kResultFalse;
    setParamNormalized(kParamBypass, bypass ? 1.0 : 0.0);

    return Steinberg::kResultOk;
}

// ===================================================================
//  createView — return our custom VSTGUI editor
// ===================================================================

Steinberg::IPlugView* PLUGIN_API Mcaster1TunerController::createView(
    Steinberg::FIDString name)
{
    if (std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0)
    {
        auto* view = new TunerEditorView(this);
        m_editorView = view;
        return view;
    }
    return nullptr;
}

// ===================================================================
//  notify — receive pitch messages from processor
// ===================================================================

Steinberg::tresult PLUGIN_API Mcaster1TunerController::notify(
    Steinberg::Vst::IMessage* message)
{
    if (!message)
        return Steinberg::kInvalidArgument;

    if (std::strcmp(message->getMessageID(), kMsgPitch) == 0)
    {
        auto* attrs = message->getAttributes();
        if (!attrs)
            return Steinberg::kResultFalse;

        double pitch = 0.0;
        double confidence = 0.0;
        attrs->getFloat("pitch", pitch);
        attrs->getFloat("confidence", confidence);

        // Compute note info using the current concert pitch setting
        double pitchNorm = getParamNormalized(kParamConcertPitch);
        float concertHz = 430.0f + static_cast<float>(pitchNorm) * 20.0f;

        auto noteInfo = PitchDetector::nearestNote(
            static_cast<float>(pitch), concertHz);

        // Forward to the editor view
        if (m_editorView) {
            m_editorView->updatePitch(
                static_cast<float>(pitch),
                static_cast<float>(confidence),
                noteInfo.centDeviation,
                noteInfo.noteName,
                noteInfo.octave);
        }

        return Steinberg::kResultOk;
    }

    return EditController::notify(message);
}

} // namespace Mcaster1
