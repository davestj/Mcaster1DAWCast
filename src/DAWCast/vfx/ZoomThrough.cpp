// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ZoomThrough.h"

#include <QPainter>
#include <cmath>

namespace dawcast {

ZoomThrough::ZoomThrough() = default;
ZoomThrough::~ZoomThrough() = default;

QImage ZoomThrough::process(QImage& frameA, QImage& frameB, float progress)
{
    const int w = frameA.width();
    const int h = frameA.height();

    QImage result(QSize(w, h), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::black);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (progress < 0.5f) {
        // Outgoing frame zooms in (scale 1.0 -> 2.0), fading out
        float t = progress * 2.0f; // 0..1 over first half
        float scale = 1.0f + t;    // 1.0 -> 2.0
        float opacity = 1.0f - t;

        painter.setOpacity(static_cast<qreal>(opacity));
        int sw = static_cast<int>(w * scale);
        int sh = static_cast<int>(h * scale);
        QImage scaled = frameA.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter.drawImage((w - sw) / 2, (h - sh) / 2, scaled);
    } else {
        // Incoming frame zooms out (scale 2.0 -> 1.0), fading in
        float t = (progress - 0.5f) * 2.0f; // 0..1 over second half
        float scale = 2.0f - t;              // 2.0 -> 1.0
        float opacity = t;

        painter.setOpacity(static_cast<qreal>(opacity));
        int sw = static_cast<int>(w * scale);
        int sh = static_cast<int>(h * scale);
        QImage scaled = frameB.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter.drawImage((w - sw) / 2, (h - sh) / 2, scaled);
    }

    painter.end();
    return result;
}

QString ZoomThrough::name() const
{
    return QStringLiteral("Zoom Through");
}

int ZoomThrough::parameterCount() const
{
    return 0;
}

} // namespace dawcast
