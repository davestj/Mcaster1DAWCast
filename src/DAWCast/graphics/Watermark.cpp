// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Watermark.h"

#include <QPainter>

namespace dawcast {

Watermark::Watermark(QObject *parent)
    : QObject(parent)
{
}

Watermark::~Watermark() = default;

void Watermark::setImage(const QString &path)
{
    m_image = QImage(path);
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
}

float Watermark::opacity() const { return m_opacity; }
float Watermark::scale() const { return m_scale; }

void Watermark::render(QPainter &painter, int frameWidth, int frameHeight)
{
    if (m_image.isNull()) return;

    QImage scaled = m_image.scaled(
        static_cast<int>(m_image.width() * m_scale),
        static_cast<int>(m_image.height() * m_scale),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const int margin = 10;
    int x = margin;
    int y = margin;

    if (m_alignment & Qt::AlignRight) {
        x = frameWidth - scaled.width() - margin;
    } else if (m_alignment & Qt::AlignHCenter) {
        x = (frameWidth - scaled.width()) / 2;
    }

    if (m_alignment & Qt::AlignBottom) {
        y = frameHeight - scaled.height() - margin;
    } else if (m_alignment & Qt::AlignVCenter) {
        y = (frameHeight - scaled.height()) / 2;
    }

    painter.save();
    painter.setOpacity(static_cast<qreal>(m_opacity));
    painter.drawImage(x, y, scaled);
    painter.restore();
}

QImage Watermark::extractWatermark(const QImage &frame)
{
    // TODO: Implement watermark extraction/detection
    // This would involve frequency-domain analysis or template matching
    Q_UNUSED(frame)
    return QImage{};
}

} // namespace dawcast
