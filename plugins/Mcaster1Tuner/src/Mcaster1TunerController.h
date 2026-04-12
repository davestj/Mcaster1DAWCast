/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1TunerController.h — Edit controller (UI thread)
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "Mcaster1Tuner.h"
#include "Mcaster1TunerEditor.h"

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/vstguieditor.h"

namespace Mcaster1 {

// ===================================================================
//  TunerEditorView — VSTGUIEditor that hosts TunerView
// ===================================================================

class TunerEditorView : public Steinberg::Vst::VSTGUIEditor
{
public:
    TunerEditorView(Steinberg::Vst::EditController* controller)
        : VSTGUIEditor(controller, nullptr)
    {
        setRect({0, 0, kEditorWidth, kEditorHeight});
    }

    ~TunerEditorView() override = default;

    // IPlugView
    bool PLUGIN_API open(void* parent,
                         const VSTGUI::PlatformType& type) override
    {
        VSTGUI::CRect frameSize(0, 0, kEditorWidth, kEditorHeight);
        frame = new VSTGUI::CFrame(frameSize, this);
        if (!frame->open(parent, type))
        {
            frame->forget();
            frame = nullptr;
            return false;
        }

        m_tunerView = new TunerView(frameSize, getController());
        frame->addView(m_tunerView);
        m_tunerView->startTimer();

        return true;
    }

    void PLUGIN_API close() override
    {
        if (m_tunerView) {
            m_tunerView->stopTimer();
            m_tunerView = nullptr;   // frame owns the view
        }
        if (frame) {
            frame->forget();
            frame = nullptr;
        }
    }

    /** Receive pitch data messages from processor. */
    void updatePitch(float pitch, float confidence, float cents,
                     const char* noteName, int octave)
    {
        if (m_tunerView)
            m_tunerView->setPitchData(pitch, confidence, cents,
                                       noteName, octave);
    }

    TunerView* tunerView() const { return m_tunerView; }

private:
    void setRect(const Steinberg::ViewRect& r)
    {
        rect.left   = r.left;
        rect.top    = r.top;
        rect.right  = r.right;
        rect.bottom = r.bottom;
    }

    TunerView* m_tunerView = nullptr;
};

// ===================================================================
//  Mcaster1TunerController
// ===================================================================

class Mcaster1TunerController : public Steinberg::Vst::EditController
{
public:
    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IEditController*>(
            new Mcaster1TunerController());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API terminate() override;

    // State
    Steinberg::tresult PLUGIN_API setComponentState(
        Steinberg::IBStream* state) override;

    // Custom VSTGUI editor
    Steinberg::IPlugView* PLUGIN_API createView(
        Steinberg::FIDString name) override;

    // Processor -> Controller message handling
    Steinberg::tresult PLUGIN_API notify(
        Steinberg::Vst::IMessage* message) override;

private:
    TunerEditorView* m_editorView = nullptr;
};

} // namespace Mcaster1
