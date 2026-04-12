/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1Tuner.cpp — Audio processor implementation
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Mcaster1Tuner.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

namespace Mcaster1 {

Mcaster1TunerProcessor::Mcaster1TunerProcessor()
{
    setControllerClass(kMcaster1TunerControllerUID);
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::initialize(
    Steinberg::FUnknown* context)
{
    auto result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultOk)
        return result;

    // Stereo input (we only analyze, but accept stereo for compatibility)
    addAudioInput(STR16("Audio In"),
                  Steinberg::Vst::SpeakerArr::kStereo);
    // Stereo output (pass-through — tuner doesn't modify audio)
    addAudioOutput(STR16("Audio Out"),
                   Steinberg::Vst::SpeakerArr::kStereo);

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::terminate()
{
    return AudioEffect::terminate();
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::setActive(
    Steinberg::TBool state)
{
    if (state) {
        m_pitchDetector.reset();
    }
    return AudioEffect::setActive(state);
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::setupProcessing(
    Steinberg::Vst::ProcessSetup& newSetup)
{
    m_pitchDetector.setSampleRate(static_cast<int>(newSetup.sampleRate));
    return AudioEffect::setupProcessing(newSetup);
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::setBusArrangements(
    Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
    Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts)
{
    // Accept stereo or mono
    if (numIns == 1 && numOuts == 1) {
        if (inputs[0] == Steinberg::Vst::SpeakerArr::kStereo &&
            outputs[0] == Steinberg::Vst::SpeakerArr::kStereo)
            return Steinberg::kResultOk;
        if (inputs[0] == Steinberg::Vst::SpeakerArr::kMono &&
            outputs[0] == Steinberg::Vst::SpeakerArr::kMono)
            return Steinberg::kResultOk;
    }
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::canProcessSampleSize(
    Steinberg::int32 symbolicSampleSize)
{
    if (symbolicSampleSize == Steinberg::Vst::kSample32)
        return Steinberg::kResultTrue;
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::process(
    Steinberg::Vst::ProcessData& data)
{
    // Handle parameter changes
    if (data.inputParameterChanges) {
        Steinberg::int32 numParams = data.inputParameterChanges->getParameterCount();
        for (Steinberg::int32 i = 0; i < numParams; ++i) {
            auto* paramQueue = data.inputParameterChanges->getParameterData(i);
            if (!paramQueue) continue;

            Steinberg::Vst::ParamValue value;
            Steinberg::int32 sampleOffset;
            Steinberg::int32 numPoints = paramQueue->getPointCount();
            if (numPoints <= 0) continue;

            // Use last point
            if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) ==
                Steinberg::kResultOk) {
                switch (paramQueue->getParameterId()) {
                case kParamPresetIndex:
                    m_presetIndex = static_cast<int>(
                        value * (TuningTables::presetCount() - 1) + 0.5);
                    break;
                case kParamConcertPitch:
                    m_concertPitch = 430.0f + static_cast<float>(value) * 20.0f;
                    break;
                case kParamBypass:
                    m_bypass = value > 0.5;
                    break;
                }
            }
        }
    }

    // Process audio: pass-through + pitch detection on channel 0
    if (data.numInputs == 0 || data.numOutputs == 0)
        return Steinberg::kResultOk;

    auto& inBus  = data.inputs[0];
    auto& outBus = data.outputs[0];
    Steinberg::int32 numChannels = inBus.numChannels;
    Steinberg::int32 numSamples  = data.numSamples;

    if (numSamples <= 0)
        return Steinberg::kResultOk;

    // Pass-through: copy input to output (tuner doesn't modify audio)
    for (Steinberg::int32 ch = 0; ch < numChannels; ++ch) {
        if (inBus.channelBuffers32[ch] && outBus.channelBuffers32[ch]) {
            if (inBus.channelBuffers32[ch] != outBus.channelBuffers32[ch]) {
                std::memcpy(outBus.channelBuffers32[ch],
                           inBus.channelBuffers32[ch],
                           static_cast<size_t>(numSamples) * sizeof(float));
            }
        }
    }

    // Feed channel 0 to pitch detector (even when bypassed — tuner always reads)
    if (inBus.channelBuffers32[0]) {
        m_pitchDetector.process(inBus.channelBuffers32[0], numSamples);
    }

    // Send pitch info to controller
    float pitch = m_pitchDetector.detectedPitch();
    float confidence = m_pitchDetector.confidence();
    if (pitch > 0.0f && confidence > 0.3f) {
        sendPitchMessage(pitch, confidence);
    }

    return Steinberg::kResultOk;
}

void Mcaster1TunerProcessor::sendPitchMessage(float pitch, float confidence)
{
    // Send pitch data to the controller via VST3 IMessage.
    // The controller computes note name / cents from the raw Hz value.
    auto* msg = allocateMessage();
    if (!msg)
        return;

    msg->setMessageID(kMsgPitch);
    auto* attrs = msg->getAttributes();
    if (attrs) {
        attrs->setFloat("pitch", static_cast<double>(pitch));
        attrs->setFloat("confidence", static_cast<double>(confidence));
    }
    sendMessage(msg);
    msg->release();
}

// ── State persistence ───────────────────────────────────────────────────────

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::setState(
    Steinberg::IBStream* state)
{
    if (!state) return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(state, kLittleEndian);

    Steinberg::int32 presetIdx = 0;
    if (!streamer.readInt32(presetIdx))
        return Steinberg::kResultFalse;
    m_presetIndex = presetIdx;

    float concertPitch = 440.0f;
    if (!streamer.readFloat(concertPitch))
        return Steinberg::kResultFalse;
    m_concertPitch = concertPitch;

    Steinberg::int32 bypass = 0;
    if (!streamer.readInt32(bypass))
        return Steinberg::kResultFalse;
    m_bypass = bypass != 0;

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Mcaster1TunerProcessor::getState(
    Steinberg::IBStream* state)
{
    if (!state) return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(state, kLittleEndian);
    streamer.writeInt32(static_cast<Steinberg::int32>(m_presetIndex));
    streamer.writeFloat(m_concertPitch);
    streamer.writeInt32(m_bypass ? 1 : 0);

    return Steinberg::kResultOk;
}

} // namespace Mcaster1
