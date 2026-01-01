// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SpectrumWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace dawcast::widgets {

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 100);
    m_magnitudes.resize(static_cast<size_t>(m_fftSize / 2), 0.0f);
}

SpectrumWidget::~SpectrumWidget() = default;

void SpectrumWidget::processBuffer(const float* /*data*/, int /*frames*/)
{
    // TODO: perform FFT (using FFTW or similar) and fill m_magnitudes
    update();
}

void SpectrumWidget::setFFTSize(int size)
{
    m_fftSize = size;
    m_magnitudes.resize(static_cast<size_t>(size / 2), 0.0f);
}

void SpectrumWidget::setDisplayMode(SpectrumMode mode)
{
    m_mode = mode;
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_magnitudes.empty()) return;

    const int w = width();
    const int h = height();
    const int binCount = static_cast<int>(m_magnitudes.size());

    painter.setPen(Qt::NoPen);

    switch (m_mode) {
    case SpectrumMode::Bars: {
        float barWidth = static_cast<float>(w) / static_cast<float>(binCount);
        for (int i = 0; i < binCount && i < w; ++i) {
            int barH = static_cast<int>(m_magnitudes[static_cast<size_t>(i)] * h);
            float ratio = static_cast<float>(barH) / static_cast<float>(h);
            QColor color = QColor::fromHsvF(0.33f * (1.0f - ratio), 1.0f, 1.0f);
            painter.setBrush(color);
            int x = static_cast<int>(i * barWidth);
            painter.drawRect(x, h - barH, static_cast<int>(barWidth) - 1, barH);
        }
        break;
    }
    case SpectrumMode::Line: {
        painter.setPen(QPen(QColor(0, 200, 255), 1));
        QPainterPath path;
        for (int i = 0; i < binCount; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(binCount) * w;
            float y = h - m_magnitudes[static_cast<size_t>(i)] * h;
            if (i == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }
        painter.drawPath(path);
        break;
    }
    case SpectrumMode::Waterfall:
        // TODO: waterfall display (scrolling spectrogram)
        break;
    }
}

} // namespace dawcast::widgets
