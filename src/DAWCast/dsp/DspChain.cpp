// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DspChain.h"
#include "core/IEffectUnit.h"

namespace dawcast {

DspChain::DspChain(QObject* parent)
    : QObject(parent)
{
}

DspChain::~DspChain() = default;

void DspChain::addEffect(IEffectUnit* effect)
{
    if (!effect) return;
    m_effects.append(effect);
    emit chainChanged();
}

void DspChain::removeEffect(int index)
{
    if (index < 0 || index >= m_effects.size()) return;
    m_effects.removeAt(index);
    emit chainChanged();
}

void DspChain::process(float* buffer, int frames, int channels)
{
    if (m_bypassed || !buffer || frames <= 0 || channels <= 0) return;

    for (auto* fx : m_effects) {
        if (fx && !fx->isBypassed()) {
            fx->process(buffer, frames, channels);
        }
    }
}

int DspChain::effectCount() const
{
    return m_effects.size();
}

IEffectUnit* DspChain::effect(int index) const
{
    if (index < 0 || index >= m_effects.size()) return nullptr;
    return m_effects.at(index);
}

void DspChain::bypass(bool bypassed)
{
    m_bypassed = bypassed;
}

} // namespace dawcast
