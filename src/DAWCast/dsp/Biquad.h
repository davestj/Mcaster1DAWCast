// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawcast {

// -----------------------------------------------------------------------
// Biquad filter — Transposed Direct Form II
// Factory methods use Robert Bristow-Johnson Audio EQ Cookbook formulas.
// -----------------------------------------------------------------------

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
};

class Biquad
{
public:
    void setCoeffs(const BiquadCoeffs& c) { m_c = c; }

    float process(float sample)
    {
        // Transposed Direct Form II
        double out = m_c.b0 * sample + z1;
        z1 = m_c.b1 * sample - m_c.a1 * out + z2;
        z2 = m_c.b2 * sample - m_c.a2 * out;
        return static_cast<float>(out);
    }

    void reset()
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    // ------------------------------------------------------------------
    // RBJ Audio EQ Cookbook factory methods
    // ------------------------------------------------------------------

    static BiquadCoeffs lowpass(float freq, float q, float sampleRate)
    {
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = ((1.0 - cosW0) / 2.0) / a0;
        c.b1 = (1.0 - cosW0) / a0;
        c.b2 = ((1.0 - cosW0) / 2.0) / a0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    static BiquadCoeffs highpass(float freq, float q, float sampleRate)
    {
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = ((1.0 + cosW0) / 2.0) / a0;
        c.b1 = (-(1.0 + cosW0)) / a0;
        c.b2 = ((1.0 + cosW0) / 2.0) / a0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    static BiquadCoeffs peaking(float freq, float q, float gainDb, float sampleRate)
    {
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);

        const double a0 = 1.0 + alpha / A;
        BiquadCoeffs c;
        c.b0 = (1.0 + alpha * A) / a0;
        c.b1 = (-2.0 * cosW0) / a0;
        c.b2 = (1.0 - alpha * A) / a0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha / A) / a0;
        return c;
    }

    static BiquadCoeffs lowshelf(float freq, float q, float gainDb, float sampleRate)
    {
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        const double a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
        BiquadCoeffs c;
        c.b0 = (A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha)) / a0;
        c.b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0)) / a0;
        c.b2 = (A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha)) / a0;
        c.a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosW0)) / a0;
        c.a2 = ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha) / a0;
        return c;
    }

    static BiquadCoeffs highshelf(float freq, float q, float gainDb, float sampleRate)
    {
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        const double a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
        BiquadCoeffs c;
        c.b0 = (A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha)) / a0;
        c.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0)) / a0;
        c.b2 = (A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha)) / a0;
        c.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosW0)) / a0;
        c.a2 = ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha) / a0;
        return c;
    }

    static BiquadCoeffs bandpass(float freq, float q, float sampleRate)
    {
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = alpha / a0;
        c.b1 = 0.0;
        c.b2 = -alpha / a0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    static BiquadCoeffs notch(float freq, float q, float sampleRate)
    {
        const double w0 = 2.0 * M_PI * freq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * q);

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = 1.0 / a0;
        c.b1 = (-2.0 * cosW0) / a0;
        c.b2 = 1.0 / a0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

private:
    BiquadCoeffs m_c;
    double z1 = 0.0;
    double z2 = 0.0;
};

} // namespace dawcast
