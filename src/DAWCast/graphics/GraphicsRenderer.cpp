// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GraphicsRenderer.h"

#include <QPainter>

namespace dawcast {

GraphicsRenderer::GraphicsRenderer(QObject *parent)
    : QObject(parent)
{
}

GraphicsRenderer::~GraphicsRenderer() = default;

void GraphicsRenderer::renderFrame(QImage &frame, const QList<OverlayRenderFunc> &overlays, double timeSeconds)
{
    if (frame.isNull() || overlays.isEmpty()) return;

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    for (const auto &renderFunc : overlays) {
        if (renderFunc) {
            painter.save();
            renderFunc(painter, frame.width(), frame.height(), timeSeconds);
            painter.restore();
        }
    }

    painter.end();
}

} // namespace dawcast
