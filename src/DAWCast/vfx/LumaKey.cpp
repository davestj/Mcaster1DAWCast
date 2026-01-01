// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LumaKey.h"

#include <algorithm>
#include <cmath>

namespace dawcast {

LumaKey::LumaKey() = default;
LumaKey::~LumaKey() = default;

QImage LumaKey::process(QImage& frameA, QImage& frameB, float progress)
{
    Q_UNUSED(progress);

    const int w = frameA.width();
    const int h = frameA.height();

    QImage srcA = frameA.convertToFormat(QImage::Format_ARGB32);
    QImage srcB = frameB.convertToFormat(QImage::Format_ARGB32);
    QImage result(w, h, QImage::Format_ARGB32);

    const float halfSoft = m_softness * 0.5f;

    for (int y = 0; y < h; ++y) {
        const auto* scanA = reinterpret_cast<const QRgb*>(srcA.constScanLine(y));
        const auto* scanB = reinterpret_cast<const QRgb*>(srcB.constScanLine(y));
        auto* scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            // BT.709 luminance
            float luma = (0.2126f * qRed(scanA[x])
                        + 0.7152f * qGreen(scanA[x])
                        + 0.0722f * qBlue(scanA[x])) / 255.0f;

            // Compute key alpha (0 = transparent/show B, 1 = opaque/show A)
            float alpha = 1.0f;
            if (halfSoft > 0.001f) {
                alpha = std::clamp((luma - m_threshold + halfSoft) / m_softness, 0.0f, 1.0f);
            } else {
                alpha = (luma >= m_threshold) ? 1.0f : 0.0f;
            }

            if (m_invert) alpha = 1.0f - alpha;

            // Composite: A over B using key alpha
            int r = static_cast<int>(qRed(scanA[x])   * alpha + qRed(scanB[x])   * (1.0f - alpha));
            int g = static_cast<int>(qGreen(scanA[x]) * alpha + qGreen(scanB[x]) * (1.0f - alpha));
            int b = static_cast<int>(qBlue(scanA[x])  * alpha + qBlue(scanB[x])  * (1.0f - alpha));
            scanOut[x] = qRgba(r, g, b, 255);
        }
    }

    return result;
}

QString LumaKey::name() const
{
    return QStringLiteral("Luma Key");
}

int LumaKey::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
