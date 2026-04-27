// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// AuEffectAdapter
// ───────────────
// Wraps a live AuPluginInstance so the host's DspChain can use it as
// any other IEffectUnit.  One adapter owns one plugin instance.
//
// Parameter IDs exchanged through IEffectUnit::setParameter/parameter
// are plain float "plain" values in the AU-native range (minValue..
// maxValue from AudioUnitParameterInfo).  The generic slider editor
// built by AuPluginInstance::openEditor honours that same range.  The
// integer `id` argument here is an *index* into the cached parameter
// list, matching how the VST3 adapter treats its argument.

#pragma once

#include "../core/IEffectUnit.h"
#include "AuHost.h"

namespace dawcast::plugins {

class AuEffectAdapter : public dawcast::IEffectUnit {
public:
    explicit AuEffectAdapter(std::unique_ptr<AuPluginInstance> instance,
                              QString displayName)
        : m_instance(std::move(instance))
        , m_name(std::move(displayName))
    {}

    void process(float* buffer, int frames, int channels) override {
        if (isBypassed() || !m_instance) return;
        m_instance->process(buffer, frames, channels);
    }

    void setParameter(int id, float value) override {
        if (!m_instance) return;
        m_instance->setParameter(id, value);
    }

    float parameter(int id) const override {
        if (!m_instance) return 0.0f;
        return m_instance->getParameter(id);
    }

    int parameterCount() const override {
        return m_instance ? m_instance->parameterCount() : 0;
    }

    QString name() const override { return m_name; }

    /// Access to the underlying instance so MainWindow and
    /// EffectsRackWidget can reopen the plugin's editor dialog or
    /// (de)serialize its state.
    AuPluginInstance* instance() const { return m_instance.get(); }

private:
    std::unique_ptr<AuPluginInstance> m_instance;
    QString                           m_name;
};

} // namespace dawcast::plugins
