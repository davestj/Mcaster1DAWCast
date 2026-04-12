// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Mc1EffectAdapter
// ────────────────
// Bridges mc1dsp::DspEffect (the in-house Mcaster1 DSP plugin base class
// shared with Mcaster1AudioPipe) to dawcast::IEffectUnit, which is the
// interface DspChain / EffectsRackWidget already speak.
//
// The adapter takes ownership of the wrapped mc1 effect via unique_ptr,
// enables it on construction (mc1 effects default to enabled=false), and
// forwards process()/parameter calls. Bypass is honored on the DAWCast
// side so a single bypass toggle drives both layers.

#pragma once

#include "../../core/IEffectUnit.h"
#include "patchbay/dsp/dsp_effect.h"

#include <QString>
#include <memory>
#include <utility>

namespace dawcast::dsp {

class Mc1EffectAdapter : public dawcast::IEffectUnit
{
public:
    explicit Mc1EffectAdapter(std::unique_ptr<mc1dsp::DspEffect> effect,
                              int sampleRate = 48000)
        : m_effect(std::move(effect))
    {
        if (m_effect) {
            m_effect->setSampleRate(sampleRate);
            m_effect->setEnabled(true);
        }
    }

    ~Mc1EffectAdapter() override = default;

    Mc1EffectAdapter(const Mc1EffectAdapter&) = delete;
    Mc1EffectAdapter& operator=(const Mc1EffectAdapter&) = delete;

    // ── IEffectUnit ──────────────────────────────────────────────────
    void process(float* buffer, int frames, int channels) override
    {
        if (isBypassed() || !m_effect || frames <= 0) return;
        m_effect->process(buffer,
                          static_cast<size_t>(frames),
                          channels);
    }

    void setParameter(int id, float value) override
    {
        if (m_effect && id >= 0 && id < m_effect->paramCount()) {
            m_effect->setParamValue(id, value);
        }
    }

    float parameter(int id) const override
    {
        if (m_effect && id >= 0 && id < m_effect->paramCount()) {
            return m_effect->paramValue(id);
        }
        return 0.0f;
    }

    QString name() const override
    {
        return m_effect ? QString::fromLatin1(m_effect->name())
                        : QStringLiteral("MC1 Effect");
    }

    int parameterCount() const override
    {
        return m_effect ? m_effect->paramCount() : 0;
    }

    // ── mc1-specific accessors (used by editor dialogs) ──────────────
    mc1dsp::DspEffect* mc1() const { return m_effect.get(); }

    QString id() const
    {
        return m_effect ? QString::fromLatin1(m_effect->id())
                        : QString();
    }

    QString version() const
    {
        return m_effect ? QString::fromLatin1(m_effect->version())
                        : QString();
    }

    QString paramDisplay(int index) const
    {
        if (!m_effect) return {};
        return QString::fromStdString(m_effect->paramDisplayValue(index));
    }

    QString paramName(int index) const
    {
        return m_effect ? QString::fromLatin1(m_effect->paramName(index))
                        : QString();
    }

    QString paramUnit(int index) const
    {
        return m_effect ? QString::fromLatin1(m_effect->paramUnit(index))
                        : QString();
    }

private:
    std::unique_ptr<mc1dsp::DspEffect> m_effect;
};

} // namespace dawcast::dsp
