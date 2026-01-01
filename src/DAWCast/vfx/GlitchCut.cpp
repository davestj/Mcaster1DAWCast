// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GlitchCut.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace dawcast {

GlitchCut::GlitchCut() = default;
GlitchCut::~GlitchCut() = default;

QImage GlitchCut::process(QImage& frameA, QImage& frameB, float progress)
{
    const int w = frameA.width();
    const int h = frameA.height();

    // Start with a cross-dissolve base
    QImage result = frameA.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&result);
    painter.setOpacity(static_cast<qreal>(progress));
    painter.drawImage(0, 0, frameB);
    painter.setOpacity(1.0);

    // Seed based on progress for deterministic glitch
    m_seed = static_cast<unsigned int>(progress * 10000.0f) + 42;

    int numSlices = std::max(1, static_cast<int>(m_glitchFrames * m_intensity));
    int sliceHeight = std::max(2, h / numSlices);

    for (int i = 0; i < numSlices; ++i) {
        int y = static_cast<int>(nextRand() % static_cast<unsigned>(h));
        int sh = std::min(sliceHeight, h - y);
        int offsetX = static_cast<int>((nextRand() % 100) * m_intensity * w / 100.0f);

        if (nextRand() % 2 == 0) offsetX = -offsetX;

        // Choose source (A or B) for the glitch slice
        const QImage& src = (nextRand() % 2 == 0) ? frameA : frameB;
        QImage slice = src.copy(0, y, w, sh);

        // Draw with offset for scan-line displacement
        painter.drawImage(offsetX, y, slice);
    }

    // End painter before direct pixel manipulation
    painter.end();

    // Channel shift artifact: offset red channel
    if (m_artifactMix > 0.01f) {
        int shift = static_cast<int>(m_artifactMix * m_intensity * 20.0f);
        QImage shifted = result.copy();

        for (int y = 0; y < h; ++y) {
            auto* scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));
            const auto* scanShift = reinterpret_cast<const QRgb*>(shifted.constScanLine(y));

            for (int x = 0; x < w; ++x) {
                int srcX = std::clamp(x + shift, 0, w - 1);
                int r = qRed(scanShift[srcX]);
                int g = qGreen(scanOut[x]);
                int b = qBlue(scanOut[x]);
                scanOut[x] = qRgb(r, g, b);
            }
        }
    }

    // Block corruption: copy random rectangles from wrong positions
    if (m_intensity > 0.3f) {
        int blockCount = static_cast<int>(m_intensity * 5.0f);
        for (int i = 0; i < blockCount; ++i) {
            int bx = static_cast<int>(nextRand() % static_cast<unsigned>(w));
            int by = static_cast<int>(nextRand() % static_cast<unsigned>(h));
            int bw = static_cast<int>((nextRand() % 80) + 20);
            int bh = static_cast<int>((nextRand() % 30) + 5);

            int sx = static_cast<int>(nextRand() % static_cast<unsigned>(w));
            int sy = static_cast<int>(nextRand() % static_cast<unsigned>(h));

            // Choose from A or B
            const QImage& blockSrc = (nextRand() % 2 == 0) ? frameA : frameB;
            QImage blockSrcFmt = blockSrc.convertToFormat(QImage::Format_ARGB32_Premultiplied);

            for (int row = 0; row < bh && (by + row) < h && (sy + row) < blockSrcFmt.height(); ++row) {
                const auto* srcLine = reinterpret_cast<const QRgb*>(blockSrcFmt.constScanLine(sy + row));
                auto* dstLine = reinterpret_cast<QRgb*>(result.scanLine(by + row));
                for (int col = 0; col < bw && (bx + col) < w && (sx + col) < blockSrcFmt.width(); ++col) {
                    dstLine[bx + col] = srcLine[sx + col];
                }
            }
        }
    }

    return result;
}

QString GlitchCut::name() const
{
    return QStringLiteral("Glitch Cut");
}

int GlitchCut::parameterCount() const
{
    return ParamCount;
}

unsigned int GlitchCut::nextRand()
{
    // xorshift32
    m_seed ^= m_seed << 13;
    m_seed ^= m_seed >> 17;
    m_seed ^= m_seed << 5;
    return m_seed;
}

} // namespace dawcast
