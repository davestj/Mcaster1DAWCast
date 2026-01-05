// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoMixer.h"
#include <QPainter>

namespace dawcast {

VideoMixer::VideoMixer(QObject* parent)
    : QObject(parent)
{
}

VideoMixer::~VideoMixer() = default;

QImage VideoMixer::composite(const QList<VideoFrame>& layers)
{
    QImage output(m_outputWidth, m_outputHeight, QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::black);

    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    for (const auto& layer : layers) {
        if (!layer.isValid()) continue;

        // Apply layer opacity (default 1.0 if not set)
        qreal opacity = qBound(0.0, layer.opacity, 1.0);
        painter.setOpacity(opacity);

        // Determine target rectangle based on layer position and size
        QImage scaled;
        QPoint pos(layer.posX, layer.posY);

        if (layer.pipWidth > 0 && layer.pipHeight > 0) {
            // PIP (Picture-in-Picture): scale to the specified PIP dimensions
            scaled = layer.image.scaled(layer.pipWidth, layer.pipHeight,
                                        Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
        } else if (layer.image.width() != m_outputWidth ||
                   layer.image.height() != m_outputHeight) {
            // Full-frame layer: scale to fit the output while keeping aspect ratio
            scaled = layer.image.scaled(m_outputWidth, m_outputHeight,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
            // Centre the scaled image if smaller than output
            if (pos.isNull()) {
                pos.setX((m_outputWidth  - scaled.width())  / 2);
                pos.setY((m_outputHeight - scaled.height()) / 2);
            }
        } else {
            scaled = layer.image;
        }

        painter.drawImage(pos, scaled);
    }

    painter.setOpacity(1.0);
    painter.end();
    return output;
}

void VideoMixer::setOutputSize(int width, int height)
{
    m_outputWidth  = width;
    m_outputHeight = height;
}

} // namespace dawcast
