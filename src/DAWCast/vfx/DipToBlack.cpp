// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DipToBlack.h"

#include <algorithm>
#include <cmath>

namespace dawcast {

DipToBlack::DipToBlack() = default;
DipToBlack::~DipToBlack() = default;

QImage DipToBlack::process(QImage& frameA, QImage& frameB, float progress)
{
    // Two-phase per-pixel fade through black using scanLine() for performance.
    // First half (t < 0.5): fade A to black, alpha = 1 - 2*t
    // Second half (t >= 0.5): fade B from black, alpha = 2*t - 1

    const QImage& source = (progress < 0.5f) ? frameA : frameB;
    const int w = source.width();
    const int h = source.height();

    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QImage result(w, h, QImage::Format_ARGB32);

    // Compute brightness multiplier: ramps down then back up through black
    float brightness;
    if (progress < 0.5f) {
        brightness = 1.0f - 2.0f * progress;   // 1.0 -> 0.0
    } else {
        brightness = 2.0f * progress - 1.0f;    // 0.0 -> 1.0
    }

    // Fixed-point multiplier (0-256 range)
    const int mul = static_cast<int>(brightness * 256.0f + 0.5f);

    for (int y = 0; y < h; ++y) {
        const auto* scanIn  = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        auto*       scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            const QRgb px = scanIn[x];
            int r = (qRed(px)   * mul) >> 8;
            int g = (qGreen(px) * mul) >> 8;
            int b = (qBlue(px)  * mul) >> 8;
            scanOut[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QString DipToBlack::name() const
{
    return QStringLiteral("Dip to Black");
}

int DipToBlack::parameterCount() const
{
    return 0;
}

} // namespace dawcast
