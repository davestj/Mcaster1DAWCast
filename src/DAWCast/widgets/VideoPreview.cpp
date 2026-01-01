// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoPreview.h"

#include <QPainter>

namespace dawcast::widgets {

VideoPreview::VideoPreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

VideoPreview::~VideoPreview() = default;

void VideoPreview::setFrame(const QImage& frame)
{
    m_frame = frame;
    update();
}

void VideoPreview::clear()
{
    m_frame = QImage();
    update();
}

void VideoPreview::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_frame.isNull()) return;

    // Preserve aspect ratio
    QSize scaled = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    int x = (width()  - scaled.width())  / 2;
    int y = (height() - scaled.height()) / 2;

    painter.drawImage(QRect(x, y, scaled.width(), scaled.height()), m_frame);

    // TODO: overlay play/pause button
}

} // namespace dawcast::widgets
