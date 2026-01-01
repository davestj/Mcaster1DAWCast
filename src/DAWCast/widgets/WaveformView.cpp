// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "WaveformView.h"

#include <QPainter>
#include <cmath>

namespace dawcast::widgets {

WaveformView::WaveformView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(60);
}

WaveformView::~WaveformView() = default;

void WaveformView::setWaveformData(const float* data, int frames)
{
    m_data   = data;
    m_frames = frames;
    update();
}

void WaveformView::setZoom(float zoom)
{
    m_zoom = zoom;
    update();
}

void WaveformView::setColor(const QColor& color)
{
    m_color = color;
    update();
}

void WaveformView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());

    if (!m_data || m_frames <= 0) return;

    const int w = width();
    const int h = height();
    const int midY = h / 2;

    painter.setPen(m_color);

    const float samplesPerPixel = static_cast<float>(m_frames) / (static_cast<float>(w) * m_zoom);

    for (int x = 0; x < w; ++x) {
        int startSample = static_cast<int>(x * samplesPerPixel);
        int endSample   = static_cast<int>((x + 1) * samplesPerPixel);
        if (endSample > m_frames) endSample = m_frames;

        float peak = 0.0f;
        float rms  = 0.0f;
        int count  = 0;

        for (int s = startSample; s < endSample; ++s) {
            float val = std::fabs(m_data[s]);
            if (val > peak) peak = val;
            rms += m_data[s] * m_data[s];
            ++count;
        }

        if (count > 0) {
            rms = std::sqrt(rms / static_cast<float>(count));
        }

        int peakH = static_cast<int>(peak * midY);
        int rmsH  = static_cast<int>(rms * midY);

        // Draw peak (lighter)
        QColor peakColor = m_color.lighter(130);
        painter.setPen(peakColor);
        painter.drawLine(x, midY - peakH, x, midY + peakH);

        // Draw RMS (solid)
        painter.setPen(m_color);
        painter.drawLine(x, midY - rmsH, x, midY + rmsH);
    }
}

} // namespace dawcast::widgets
