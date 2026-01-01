// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CrossDissolve.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace dawcast {

CrossDissolve::CrossDissolve() = default;
CrossDissolve::~CrossDissolve() = default;

QImage CrossDissolve::process(QImage& frameA, QImage& frameB, float progress)
{
    // Alpha blend using scanLine() for fast per-pixel access: out = A*(1-t) + B*t

    const int w = frameA.width();
    const int h = frameA.height();

    QImage srcA = frameA.convertToFormat(QImage::Format_ARGB32);

    // Handle size mismatch: scale B to match A
    QImage srcB;
    if (frameB.size() != frameA.size()) {
        srcB = frameB.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                      .convertToFormat(QImage::Format_ARGB32);
    } else {
        srcB = frameB.convertToFormat(QImage::Format_ARGB32);
    }

    QImage result(w, h, QImage::Format_ARGB32);

    // Pre-compute integer blend factors (0-256 range for fixed-point blending)
    const int tB = static_cast<int>(progress * 256.0f + 0.5f);
    const int tA = 256 - tB;

    for (int y = 0; y < h; ++y) {
        const auto* scanA   = reinterpret_cast<const QRgb*>(srcA.constScanLine(y));
        const auto* scanB   = reinterpret_cast<const QRgb*>(srcB.constScanLine(y));
        auto*       scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            const QRgb a = scanA[x];
            const QRgb b = scanB[x];

            int r = (qRed(a)   * tA + qRed(b)   * tB) >> 8;
            int g = (qGreen(a) * tA + qGreen(b) * tB) >> 8;
            int bl= (qBlue(a)  * tA + qBlue(b)  * tB) >> 8;
            int al= (qAlpha(a) * tA + qAlpha(b) * tB) >> 8;

            scanOut[x] = qRgba(r, g, bl, al);
        }
    }

    return result;
}

QString CrossDissolve::name() const
{
    return QStringLiteral("Cross Dissolve");
}

int CrossDissolve::parameterCount() const
{
    return 0;
}

} // namespace dawcast
