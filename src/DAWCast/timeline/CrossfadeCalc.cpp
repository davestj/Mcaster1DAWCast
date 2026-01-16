// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CrossfadeCalc.h"
#include <cmath>
#include <algorithm>

namespace dawcast {

// M_PI may not be defined on all platforms
static constexpr float kPi = 3.14159265358979323846f;

float CrossfadeCalc::equalPower(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    // cos curve for outgoing, sin curve for incoming
    // Returns the gain for the incoming signal
    return std::sin(t * kPi * 0.5f);
}

float CrossfadeCalc::linear(float t)
{
    return std::clamp(t, 0.0f, 1.0f);
}

float CrossfadeCalc::sCurve(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    // Hermite smoothstep: 3t^2 - 2t^3
    return t * t * (3.0f - 2.0f * t);
}

float CrossfadeCalc::logarithmic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    // log10(1 + 9*t) maps [0,1] -> [0,1] — slow start, fast finish
    return std::log10(1.0f + 9.0f * t);
}

float CrossfadeCalc::exponential(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    // (e^t - 1) / (e - 1) maps [0,1] -> [0,1] — fast start, slow finish
    static constexpr float kE = 2.71828182845904523536f;
    return (std::exp(t) - 1.0f) / (kE - 1.0f);
}

} // namespace dawcast
