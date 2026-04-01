// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SpectralAnalysisPanel.h"
#include "../DAWCast/dsp/FFT.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace dawcast::editor {

SpectralAnalysisPanel::SpectralAnalysisPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMouseTracking(true);
}

SpectralAnalysisPanel::~SpectralAnalysisPanel() = default;

void SpectralAnalysisPanel::setAudioData(const float* data, int64_t frames,
                                          int channels, int sampleRate)
{
    m_data       = data;
    m_frames     = frames;
    m_channels   = channels;
    m_sampleRate = sampleRate;
    m_spectrogramDirty = true;
    m_markers.clear();
    update();
}

void SpectralAnalysisPanel::setPosition(int64_t samplePosition)
{
    if (m_cursorPosition != samplePosition) {
        m_cursorPosition = samplePosition;
        update();
    }
}

void SpectralAnalysisPanel::setFFTSize(int size)
{
    // Clamp to power-of-2 in range [256, 65536]
    int clamped = std::max(256, std::min(65536, size));
    // Round to nearest power of 2
    int p = 1;
    while (p < clamped) p <<= 1;
    if (p != m_fftSize) {
        m_fftSize = p;
        m_spectrogramDirty = true;
        update();
    }
}

void SpectralAnalysisPanel::setDisplayMode(SpectralDisplayMode mode)
{
    if (m_displayMode != mode) {
        m_displayMode = mode;
        m_spectrogramDirty = true;
        update();
    }
}

// ── Heat-map color mapping ─────────────────────────────────────────────────

QRgb SpectralAnalysisPanel::magnitudeToColor(float dB) const
{
    // Map from dB range [-100, 0] to [0.0, 1.0]
    float t = (dB + 100.0f) / 100.0f;
    t = std::max(0.0f, std::min(1.0f, t));

    // Color ramp: black -> blue -> cyan -> green -> yellow -> red -> white
    int r = 0, g = 0, b = 0;
    if (t < 0.16f) {
        // Black to dark blue
        float s = t / 0.16f;
        b = static_cast<int>(s * 128);
    } else if (t < 0.33f) {
        // Dark blue to cyan
        float s = (t - 0.16f) / 0.17f;
        b = 128 + static_cast<int>(s * 127);
        g = static_cast<int>(s * 255);
    } else if (t < 0.50f) {
        // Cyan to green
        float s = (t - 0.33f) / 0.17f;
        g = 255;
        b = 255 - static_cast<int>(s * 255);
    } else if (t < 0.67f) {
        // Green to yellow
        float s = (t - 0.50f) / 0.17f;
        r = static_cast<int>(s * 255);
        g = 255;
    } else if (t < 0.83f) {
        // Yellow to red
        float s = (t - 0.67f) / 0.16f;
        r = 255;
        g = 255 - static_cast<int>(s * 255);
    } else {
        // Red to white
        float s = (t - 0.83f) / 0.17f;
        r = 255;
        g = static_cast<int>(s * 255);
        b = static_cast<int>(s * 255);
    }

    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    return qRgb(r, g, b);
}

int SpectralAnalysisPanel::sampleToX(int64_t sample) const
{
    if (m_frames <= 0) return 0;
    return static_cast<int>(sample * width() / m_frames);
}

// ── Spectrogram rendering ──────────────────────────────────────────────────

void SpectralAnalysisPanel::rebuildSpectrogram()
{
    m_spectrogramDirty = false;

    if (!m_data || m_frames <= 0 || width() <= 0 || height() <= 0) {
        m_spectrogramImage = QImage();
        return;
    }

    int w = width();
    int h = height();
    int halfFFT = m_fftSize / 2;

    // Mix to mono
    std::vector<float> mono(static_cast<size_t>(m_frames));
    if (m_channels == 1) {
        std::copy(m_data, m_data + m_frames, mono.begin());
    } else {
        for (int64_t i = 0; i < m_frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < m_channels; ++ch)
                sum += m_data[i * m_channels + ch];
            mono[static_cast<size_t>(i)] = sum / static_cast<float>(m_channels);
        }
    }

    // Number of FFT columns that fit in the display
    int hopSize = m_fftSize / 4;
    int numColumns = w;

    m_spectrogramImage = QImage(w, h, QImage::Format_RGB32);
    m_spectrogramImage.fill(Qt::black);

    dawcast::FFT fft(m_fftSize);
    std::vector<float> block(static_cast<size_t>(m_fftSize));
    std::vector<float> real(static_cast<size_t>(m_fftSize));
    std::vector<float> imag(static_cast<size_t>(m_fftSize));

    for (int col = 0; col < numColumns; ++col) {
        // Map column to sample offset
        int64_t sampleOffset = static_cast<int64_t>(col) * m_frames / numColumns;

        // Extract Hann-windowed block
        std::fill(block.begin(), block.end(), 0.0f);
        int64_t available = std::min(static_cast<int64_t>(m_fftSize),
                                     m_frames - sampleOffset);
        for (int64_t i = 0; i < available; ++i) {
            double win = 0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(i)
                                                / static_cast<double>(m_fftSize - 1)));
            block[static_cast<size_t>(i)] =
                mono[static_cast<size_t>(sampleOffset + i)] * static_cast<float>(win);
        }

        fft.forward(block.data(), real.data(), imag.data());

        // Render this column: Y=0 is top (high frequency), Y=h-1 is bottom (low)
        for (int row = 0; row < h; ++row) {
            // Log-scale frequency mapping
            // Map row to frequency bin using logarithmic scale
            double t = static_cast<double>(h - 1 - row) / static_cast<double>(h - 1);
            // Log scale: map [0,1] to bin range [1, halfFFT-1]
            double logMin = std::log(1.0);
            double logMax = std::log(static_cast<double>(halfFFT));
            double logBin = logMin + t * (logMax - logMin);
            int bin = static_cast<int>(std::exp(logBin));
            bin = std::max(1, std::min(halfFFT - 1, bin));

            float re = real[static_cast<size_t>(bin)];
            float im = imag[static_cast<size_t>(bin)];
            float mag = std::sqrt(re * re + im * im) / static_cast<float>(m_fftSize);
            if (mag < 1e-10f) mag = 1e-10f;
            float dB = 20.0f * std::log10(mag);

            m_spectrogramImage.setPixel(col, row, magnitudeToColor(dB));
        }
    }
}

// ── Paint ──────────────────────────────────────────────────────────────────

void SpectralAnalysisPanel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_spectrogramDirty) {
        rebuildSpectrogram();
    }

    if (!m_spectrogramImage.isNull()) {
        p.drawImage(0, 0, m_spectrogramImage);
    } else {
        p.fillRect(rect(), Qt::black);
        p.setPen(QColor(80, 80, 80));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("No audio data - open a file to view spectrogram"));
    }

    // Draw playback cursor
    if (m_frames > 0) {
        int cx = sampleToX(m_cursorPosition);
        p.setPen(QPen(QColor(255, 255, 255, 200), 1));
        p.drawLine(cx, 0, cx, height());
    }

    // Draw detection markers
    p.setFont(QFont(QStringLiteral("Monospace"), 8));
    for (const auto& marker : m_markers) {
        if (marker.frameIndex >= 0) continue; // skip video-only markers
        int mx = sampleToX(marker.positionSamples);
        p.setPen(QPen(marker.color, 2));
        p.drawLine(mx, 0, mx, height());

        // Draw small label
        p.setPen(marker.color);
        p.drawText(mx + 3, 12, marker.label.left(30));
    }

    // Frequency axis labels (right side)
    p.setPen(QColor(180, 180, 180, 150));
    p.setFont(QFont(QStringLiteral("Monospace"), 7));
    int halfFFT = m_fftSize / 2;
    double logMin = std::log(1.0);
    double logMax = std::log(static_cast<double>(halfFFT));
    double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(m_fftSize);

    // Draw frequency labels at musical intervals
    double freqs[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    for (double freq : freqs) {
        if (freq > static_cast<double>(m_sampleRate) / 2.0) break;
        int bin = static_cast<int>(freq / binHz);
        if (bin < 1 || bin >= halfFFT) continue;
        double logBin = std::log(static_cast<double>(bin));
        double t = (logBin - logMin) / (logMax - logMin);
        int row = height() - 1 - static_cast<int>(t * (height() - 1));

        p.drawLine(width() - 40, row, width(), row);
        QString label;
        if (freq >= 1000)
            label = QString("%1k").arg(freq / 1000.0, 0, 'f', 0);
        else
            label = QString("%1").arg(freq, 0, 'f', 0);
        p.drawText(width() - 38, row - 2, label);
    }
}

void SpectralAnalysisPanel::mousePressEvent(QMouseEvent* event)
{
    // Check if click is near a marker
    int clickX = event->pos().x();
    for (const auto& marker : m_markers) {
        int mx = sampleToX(marker.positionSamples);
        if (std::abs(clickX - mx) < 5) {
            emit markerClicked(marker);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void SpectralAnalysisPanel::resizeEvent(QResizeEvent* event)
{
    m_spectrogramDirty = true;
    QWidget::resizeEvent(event);
}

// ── Forensic detection wrappers ────────────────────────────────────────────

QList<DetectionMarker> SpectralAnalysisPanel::runFrequencyDetection(
    double minFreq, double maxFreq, double thresholdDb)
{
    ForensicDetector det;
    det.setAudioData(m_data, m_frames, m_channels, m_sampleRate);
    auto result = det.detectFrequencyRange(minFreq, maxFreq, thresholdDb);
    m_markers.append(result);
    emit analysisComplete(result);
    update();
    return result;
}

QList<DetectionMarker> SpectralAnalysisPanel::runAnomalyDetection()
{
    ForensicDetector det;
    det.setAudioData(m_data, m_frames, m_channels, m_sampleRate);
    auto result = det.detectAnomalies(70.0);
    m_markers.append(result);
    emit analysisComplete(result);
    update();
    return result;
}

QList<DetectionMarker> SpectralAnalysisPanel::runEVPDetection()
{
    ForensicDetector det;
    det.setAudioData(m_data, m_frames, m_channels, m_sampleRate);
    auto result = det.detectEVP(70.0);
    m_markers.append(result);
    emit analysisComplete(result);
    update();
    return result;
}

QList<DetectionMarker> SpectralAnalysisPanel::runInfrasonicScan()
{
    ForensicDetector det;
    det.setAudioData(m_data, m_frames, m_channels, m_sampleRate);
    auto result = det.detectInfrasonic();
    m_markers.append(result);
    emit analysisComplete(result);
    update();
    return result;
}

QList<DetectionMarker> SpectralAnalysisPanel::runUltrasonicScan()
{
    ForensicDetector det;
    det.setAudioData(m_data, m_frames, m_channels, m_sampleRate);
    auto result = det.detectUltrasonic();
    m_markers.append(result);
    emit analysisComplete(result);
    update();
    return result;
}

} // namespace dawcast::editor
