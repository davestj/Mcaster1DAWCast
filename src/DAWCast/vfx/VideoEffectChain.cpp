// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoEffectChain.h"
#include "core/IVideoEffect.h"

namespace dawcast {

VideoEffectChain::VideoEffectChain(QObject* parent)
    : QObject(parent)
{
}

VideoEffectChain::~VideoEffectChain() = default;

void VideoEffectChain::addEffect(IVideoEffect* effect)
{
    if (!effect) return;
    m_effects.append(effect);
    emit chainChanged();
}

void VideoEffectChain::removeEffect(int index)
{
    if (index < 0 || index >= m_effects.size()) return;
    m_effects.removeAt(index);
    emit chainChanged();
}

QImage VideoEffectChain::process(QImage& frameA, QImage& frameB, float progress)
{
    QImage result = frameA;
    for (auto* fx : m_effects) {
        result = fx->process(result, frameB, progress);
    }
    return result;
}

int VideoEffectChain::effectCount() const
{
    return m_effects.size();
}

IVideoEffect* VideoEffectChain::effect(int index) const
{
    if (index < 0 || index >= m_effects.size()) return nullptr;
    return m_effects.at(index);
}

} // namespace dawcast
