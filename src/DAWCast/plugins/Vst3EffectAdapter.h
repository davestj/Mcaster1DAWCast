// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Vst3EffectAdapter
// ─────────────────
// Wraps a live VST3 Vst3PluginInstance so the host's DspChain can use
// it as any other IEffectUnit. One adapter owns one plugin instance.
//
// setParameter/parameter go through the plugin's IEditController-driven
// parameter-change queue (see Vst3PluginInstance::setParameterNormalized).
// Values exchanged here are the normalized [0,1] domain — the generic
// slider editor uses the same scale, and any UI that wants plain units
// can translate via parameterInfo() + controller->normalizedParamToPlain.

#pragma once

#include "../core/IEffectUnit.h"
#include "Vst3Host.h"

namespace dawcast::plugins {

class Vst3EffectAdapter : public dawcast::IEffectUnit {
public:
    explicit Vst3EffectAdapter(std::unique_ptr<Vst3PluginInstance> instance,
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
        m_instance->setParameterNormalized(id, static_cast<double>(value));
    }

    float parameter(int id) const override {
        if (!m_instance) return 0.0f;
        return static_cast<float>(m_instance->getParameterNormalized(id));
    }

    int parameterCount() const override {
        return m_instance ? m_instance->parameterCount() : 0;
    }

    QString name() const override { return m_name; }

    /// Access to the underlying instance so MainWindow can open the
    /// plugin's native editor dialog or (de)serialize its state.
    Vst3PluginInstance* instance() const { return m_instance.get(); }

private:
    std::unique_ptr<Vst3PluginInstance> m_instance;
    QString m_name;
};

} // namespace dawcast::plugins
