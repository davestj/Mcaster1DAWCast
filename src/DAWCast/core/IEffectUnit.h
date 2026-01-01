// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace dawcast {

class IEffectUnit
{
public:
    virtual ~IEffectUnit() = default;

    virtual void process(float* buffer, int frames, int channels) = 0;
    virtual void setParameter(int id, float value) = 0;
    virtual float parameter(int id) const = 0;
    virtual QString name() const = 0;
    virtual int parameterCount() const = 0;

    // Per-effect bypass — default implementation so existing subclasses
    // don't need to override.
    void setBypassed(bool b) { m_bypassed = b; }
    bool isBypassed() const  { return m_bypassed; }

private:
    bool m_bypassed = false;
};

} // namespace dawcast
