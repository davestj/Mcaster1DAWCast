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
        // TODO: Apply layer-specific transforms (position, scale, opacity)
        // TODO: Handle PIP regions, lower-third overlays
        painter.drawImage(0, 0, layer.image.scaled(m_outputWidth, m_outputHeight,
                                                    Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));
    }

    painter.end();
    return output;
}

void VideoMixer::setOutputSize(int width, int height)
{
    m_outputWidth  = width;
    m_outputHeight = height;
}

} // namespace dawcast
