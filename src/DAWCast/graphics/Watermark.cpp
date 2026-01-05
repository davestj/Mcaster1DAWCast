// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Watermark.h"

#include <QPainter>
#include <QDebug>

namespace dawcast {

Watermark::Watermark(QObject *parent)
    : QObject(parent)
{
}

Watermark::~Watermark() = default;

void Watermark::setImage(const QString &path)
{
    m_imagePath = path;
    m_image = QImage(path);
    // Invalidate cached scaled image when source changes
    m_cachedScaled = QImage();
    m_cachedScaleForWidth = -1;
}

void Watermark::setPosition(Qt::Alignment alignment)
{
    m_alignment = alignment;
}

void Watermark::setOpacity(float opacity)
{
    m_opacity = qBound(0.0f, opacity, 1.0f);
}

void Watermark::setScale(float scale)
{
    m_scale = qBound(0.01f, scale, 10.0f);
    // Invalidate cache when scale changes
    m_cachedScaled = QImage();
    m_cachedScaleForWidth = -1;
}

float Watermark::opacity() const { return m_opacity; }
float Watermark::scale() const { return m_scale; }

void Watermark::render(QPainter &painter, int frameWidth, int frameHeight)
{
    if (m_image.isNull()) return;

    // Scale watermark relative to frame width for consistent sizing
    // across different output resolutions
    const int targetWidth = static_cast<int>(m_scale * frameWidth);

    // Use cached scaled image if dimensions haven't changed
    if (m_cachedScaled.isNull() || m_cachedScaleForWidth != targetWidth) {
        m_cachedScaled = m_image.scaledToWidth(
            targetWidth, Qt::SmoothTransformation);
        m_cachedScaleForWidth = targetWidth;
    }

    const QImage &scaled = m_cachedScaled;
    const int margin = 16;
    int x = margin;
    int y = margin;

    // Horizontal alignment
    if (m_alignment & Qt::AlignRight) {
        x = frameWidth - scaled.width() - margin;
    } else if (m_alignment & Qt::AlignHCenter) {
        x = (frameWidth - scaled.width()) / 2;
    }
    // else: Qt::AlignLeft (default), x stays at margin

    // Vertical alignment
    if (m_alignment & Qt::AlignBottom) {
        y = frameHeight - scaled.height() - margin;
    } else if (m_alignment & Qt::AlignVCenter) {
        y = (frameHeight - scaled.height()) / 2;
    }
    // else: Qt::AlignTop (default), y stays at margin

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(static_cast<qreal>(m_opacity));
    painter.drawImage(x, y, scaled);
    painter.restore();
}

QImage Watermark::extractWatermark(const QImage &frame)
{
    // Watermark extraction requires frequency-domain analysis
    // (DFT/DCT based blind watermark detection) which is beyond the scope
    // of a simple render overlay module. This would need a dedicated
    // signal-processing pipeline with FFT support.
    Q_UNUSED(frame)
    qDebug() << "Watermark::extractWatermark: Not implemented. "
                "Frequency-domain watermark extraction requires a dedicated DSP pipeline.";
    return QImage{};
}

} // namespace dawcast
