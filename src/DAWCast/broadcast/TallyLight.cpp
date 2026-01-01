// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TallyLight.h"

namespace dawcast {

TallyLight::TallyLight(QObject *parent)
    : QObject(parent)
{
}

TallyLight::~TallyLight() = default;

void TallyLight::setOnAir(bool onAir)
{
    if (m_onAir != onAir) {
        m_onAir = onAir;
        emit stateChanged();
    }
}

void TallyLight::setRecording(bool recording)
{
    if (m_recording != recording) {
        m_recording = recording;
        emit stateChanged();
    }
}

bool TallyLight::isOnAir() const
{
    return m_onAir;
}

bool TallyLight::isRecording() const
{
    return m_recording;
}

} // namespace dawcast
