// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DipToWhite.h"

#include <algorithm>
#include <cmath>

namespace dawcast {

DipToWhite::DipToWhite() = default;
DipToWhite::~DipToWhite() = default;

QImage DipToWhite::process(QImage& frameA, QImage& frameB, float progress)
{
    // Two-phase per-pixel fade through white using scanLine() for performance.
    // First half (t < 0.5): fade A to white by lerping each pixel toward 255
    // Second half (t >= 0.5): fade B from white by lerping from 255 toward pixel

    const QImage& source = (progress < 0.5f) ? frameA : frameB;
    const int w = source.width();
    const int h = source.height();

    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QImage result(w, h, QImage::Format_ARGB32);

    // Compute blend factor toward white: ramps up then back down
    float whiteness;
    if (progress < 0.5f) {
        whiteness = 2.0f * progress;            // 0.0 -> 1.0 (full white at midpoint)
    } else {
        whiteness = 2.0f * (1.0f - progress);   // 1.0 -> 0.0
    }

    // Fixed-point multiplier (0-256 range): fraction of white to blend in
    const int wMul = static_cast<int>(whiteness * 256.0f + 0.5f);
    const int sMul = 256 - wMul;

    for (int y = 0; y < h; ++y) {
        const auto* scanIn  = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        auto*       scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            const QRgb px = scanIn[x];
            // Lerp: pixel * (1-whiteness) + 255 * whiteness
            int r = (qRed(px)   * sMul + 255 * wMul) >> 8;
            int g = (qGreen(px) * sMul + 255 * wMul) >> 8;
            int b = (qBlue(px)  * sMul + 255 * wMul) >> 8;
            scanOut[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QString DipToWhite::name() const
{
    return QStringLiteral("Dip to White");
}

int DipToWhite::parameterCount() const
{
    return 0;
}

} // namespace dawcast
