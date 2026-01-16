// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace dawcast {

/// Static crossfade calculation utilities.
/// Input t is normalized [0.0, 1.0] crossfade progress.
class CrossfadeCalc
{
public:
    CrossfadeCalc() = delete;

    /// Equal-power crossfade using sine/cosine curves.
    static float equalPower(float t);

    /// Simple linear crossfade.
    static float linear(float t);

    /// S-curve (smoothstep) crossfade.
    static float sCurve(float t);

    /// Logarithmic crossfade (slow start, fast finish).
    static float logarithmic(float t);

    /// Exponential crossfade (fast start, slow finish).
    static float exponential(float t);
};

} // namespace dawcast
