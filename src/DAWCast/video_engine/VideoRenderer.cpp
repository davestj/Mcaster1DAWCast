// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoRenderer.h"
#include <QPainter>
#include <QPaintEvent>

namespace dawcast {

VideoRenderer::VideoRenderer(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 180);
}

VideoRenderer::~VideoRenderer() = default;

void VideoRenderer::setFrame(const QImage& frame)
{
    m_currentFrame = frame;
    update();
}

double VideoRenderer::aspectRatio() const
{
    if (m_currentFrame.isNull() || m_currentFrame.height() == 0) {
        return 16.0 / 9.0;  // Default to 16:9
    }
    return static_cast<double>(m_currentFrame.width()) / m_currentFrame.height();
}

void VideoRenderer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_currentFrame.isNull()) return;

    // Maintain aspect ratio, center in widget
    double widgetAspect = static_cast<double>(width()) / height();
    double frameAspect  = aspectRatio();

    QRect targetRect;
    if (widgetAspect > frameAspect) {
        int w = static_cast<int>(height() * frameAspect);
        targetRect = QRect((width() - w) / 2, 0, w, height());
    } else {
        int h = static_cast<int>(width() / frameAspect);
        targetRect = QRect(0, (height() - h) / 2, width(), h);
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(targetRect, m_currentFrame);
}

} // namespace dawcast
