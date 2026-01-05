// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SpectrumWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>
#include <vector>

namespace dawcast::widgets {

namespace {
constexpr float kMinDbDisplay  = -60.0f;
constexpr float kMaxDbDisplay  = 0.0f;
constexpr float kMinFreqHz     = 20.0f;
constexpr float kMaxFreqHz     = 20000.0f;
constexpr int   kLeftMargin    = 36;
constexpr int   kBottomMargin  = 24;
constexpr int   kTopMargin     = 8;
constexpr int   kRightMargin   = 8;
constexpr int   kDefaultSampleRate = 48000;
constexpr float kSmoothingAlpha    = 0.3f;  // Exponential smoothing for visual decay

// Hanning window coefficient
inline float hanningWindow(int i, int N)
{
    return 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (N - 1)));
}

// Map linear magnitude to dB, clamped to display range
inline float magnitudeToDb(float mag)
{
    if (mag <= 0.0f) return kMinDbDisplay;
    float db = 20.0f * std::log10(mag);
    return std::clamp(db, kMinDbDisplay, kMaxDbDisplay);
}

// Octave frequency reference points for grid lines
const float kOctaveFreqs[] = {
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};
constexpr int kOctaveFreqCount = sizeof(kOctaveFreqs) / sizeof(kOctaveFreqs[0]);

// Nice dB values for horizontal grid lines
const float kDbGridLines[] = { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };
constexpr int kDbGridCount = sizeof(kDbGridLines) / sizeof(kDbGridLines[0]);

const QColor kBgColor(18, 18, 24);
const QColor kGridColor(50, 50, 60);
const QColor kLabelColor(120, 120, 130);
const QColor kLineColor(0, 200, 255);
const QColor kWaterfallLow(0, 0, 40);
const QColor kWaterfallHigh(255, 50, 20);
} // anonymous namespace

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 100);
    m_magnitudes.resize(static_cast<size_t>(m_fftSize / 2), 0.0f);
    m_smoothed.resize(static_cast<size_t>(m_fftSize / 2), 0.0f);
}

SpectrumWidget::~SpectrumWidget() = default;

void SpectrumWidget::processBuffer(const float* data, int frames)
{
    if (!data || frames <= 0) return;

    int N = qMin(frames, m_fftSize);
    int halfN = N / 2;

    // Working buffers for simple in-place DFT
    // For production use a proper FFT library (FFTW, KissFFT, etc.)
    // This is a magnitude-only DFT using the Goertzel-like approach for display bins.
    // We map display bins to log-spaced frequencies for efficiency.

    // Apply Hanning window and compute magnitudes via DFT for each display bin
    std::vector<float> windowed(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        windowed[static_cast<size_t>(i)] = data[i] * hanningWindow(i, N);
    }

    size_t binCount = static_cast<size_t>(halfN);
    if (binCount != m_magnitudes.size()) {
        m_magnitudes.resize(binCount, 0.0f);
        m_smoothed.resize(binCount, 0.0f);
    }

    // Compute DFT magnitudes for each bin
    float invN = 1.0f / static_cast<float>(N);
    for (size_t k = 0; k < binCount; ++k) {
        float re = 0.0f, im = 0.0f;
        float omega = 2.0f * static_cast<float>(M_PI) * static_cast<float>(k) / static_cast<float>(N);
        for (int n = 0; n < N; ++n) {
            float angle = omega * static_cast<float>(n);
            re += windowed[static_cast<size_t>(n)] * std::cos(angle);
            im -= windowed[static_cast<size_t>(n)] * std::sin(angle);
        }
        float mag = std::sqrt(re * re + im * im) * invN * 2.0f;
        m_magnitudes[k] = mag;

        // Exponential smoothing for visual decay
        m_smoothed[k] = kSmoothingAlpha * mag + (1.0f - kSmoothingAlpha) * m_smoothed[k];
    }

    update();
}

void SpectrumWidget::setFFTSize(int size)
{
    m_fftSize = size;
    m_magnitudes.resize(static_cast<size_t>(size / 2), 0.0f);
    m_smoothed.resize(static_cast<size_t>(size / 2), 0.0f);
}

void SpectrumWidget::setDisplayMode(SpectrumMode mode)
{
    m_mode = mode;
    update();
}

// Map a frequency (Hz) to an x-pixel in the plot area using log scale
static float freqToX(float freqHz, float plotLeft, float plotWidth)
{
    float logMin = std::log10(kMinFreqHz);
    float logMax = std::log10(kMaxFreqHz);
    float logFreq = std::log10(std::clamp(freqHz, kMinFreqHz, kMaxFreqHz));
    return plotLeft + (logFreq - logMin) / (logMax - logMin) * plotWidth;
}

// Map a dB value to a y-pixel in the plot area
static float dbToY(float db, float plotTop, float plotHeight)
{
    float norm = (db - kMinDbDisplay) / (kMaxDbDisplay - kMinDbDisplay);
    return plotTop + plotHeight * (1.0f - norm);
}

void SpectrumWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // Background
    p.fillRect(rect(), kBgColor);

    if (m_smoothed.empty()) return;

    const float plotLeft   = static_cast<float>(kLeftMargin);
    const float plotTop    = static_cast<float>(kTopMargin);
    const float plotWidth  = static_cast<float>(w - kLeftMargin - kRightMargin);
    const float plotHeight = static_cast<float>(h - kTopMargin - kBottomMargin);

    if (plotWidth <= 0 || plotHeight <= 0) return;

    // --- Grid lines ---
    QFont gridFont = font();
    gridFont.setPointSize(8);
    p.setFont(gridFont);

    // Vertical grid: octave frequencies
    p.setPen(QPen(kGridColor, 1, Qt::DotLine));
    for (int i = 0; i < kOctaveFreqCount; ++i) {
        float x = freqToX(kOctaveFreqs[i], plotLeft, plotWidth);
        p.drawLine(QPointF(x, plotTop), QPointF(x, plotTop + plotHeight));

        // Frequency label
        p.setPen(kLabelColor);
        QString label;
        if (kOctaveFreqs[i] >= 1000.0f)
            label = QString::number(static_cast<int>(kOctaveFreqs[i] / 1000.0f)) + QStringLiteral("k");
        else
            label = QString::number(static_cast<int>(kOctaveFreqs[i]));
        QFontMetrics fm(gridFont);
        int tw = fm.horizontalAdvance(label);
        p.drawText(static_cast<int>(x) - tw / 2,
                   static_cast<int>(plotTop + plotHeight) + 14, label);
        p.setPen(QPen(kGridColor, 1, Qt::DotLine));
    }

    // Horizontal grid: dB levels
    for (int i = 0; i < kDbGridCount; ++i) {
        float y = dbToY(kDbGridLines[i], plotTop, plotHeight);
        p.setPen(QPen(kGridColor, 1, Qt::DotLine));
        p.drawLine(QPointF(plotLeft, y), QPointF(plotLeft + plotWidth, y));

        // dB label
        p.setPen(kLabelColor);
        QString label = QString::number(static_cast<int>(kDbGridLines[i]));
        QFontMetrics fm(gridFont);
        int tw = fm.horizontalAdvance(label);
        p.drawText(static_cast<int>(plotLeft) - tw - 4, static_cast<int>(y) + 4, label);
    }

    // Plot border
    p.setPen(QPen(QColor(60, 60, 70), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(plotLeft, plotTop, plotWidth, plotHeight));

    // --- Spectrum data ---
    int binCount = static_cast<int>(m_smoothed.size());
    float sampleRate = static_cast<float>(kDefaultSampleRate);
    float binHz = sampleRate / static_cast<float>(m_fftSize);

    switch (m_mode) {
    case SpectrumMode::Bars: {
        // Draw vertical bars at log-spaced frequency positions
        // Group bins into display columns for cleaner rendering
        int numColumns = qMin(static_cast<int>(plotWidth), 128);
        float columnWidth = plotWidth / static_cast<float>(numColumns);

        for (int col = 0; col < numColumns; ++col) {
            // Map column to frequency range (log scale)
            float logMin = std::log10(kMinFreqHz);
            float logMax = std::log10(kMaxFreqHz);
            float logFreqLow  = logMin + (logMax - logMin) * static_cast<float>(col) / numColumns;
            float logFreqHigh = logMin + (logMax - logMin) * static_cast<float>(col + 1) / numColumns;
            float freqLow  = std::pow(10.0f, logFreqLow);
            float freqHigh = std::pow(10.0f, logFreqHigh);

            int binLow  = qMax(1, static_cast<int>(freqLow / binHz));
            int binHigh = qMin(binCount - 1, static_cast<int>(freqHigh / binHz));

            // Find max magnitude in this frequency range
            float maxMag = 0.0f;
            for (int b = binLow; b <= binHigh; ++b) {
                maxMag = qMax(maxMag, m_smoothed[static_cast<size_t>(b)]);
            }

            float db = magnitudeToDb(maxMag);
            float norm = (db - kMinDbDisplay) / (kMaxDbDisplay - kMinDbDisplay);
            norm = std::clamp(norm, 0.0f, 1.0f);

            float barH = norm * plotHeight;
            float barX = plotLeft + col * columnWidth;
            float barY = plotTop + plotHeight - barH;

            // Color gradient: green -> yellow -> red
            QColor barColor;
            if (norm < 0.5f)
                barColor = QColor::fromHsvF(0.33f * (1.0f - norm * 2.0f) + 0.0f * norm * 2.0f, 0.9f, 0.85f);
            else
                barColor = QColor::fromHsvF(0.16f * (1.0f - (norm - 0.5f) * 2.0f), 0.95f, 0.9f);

            p.setPen(Qt::NoPen);
            p.setBrush(barColor);
            p.drawRect(QRectF(barX + 1, barY, columnWidth - 2, barH));
        }
        break;
    }

    case SpectrumMode::Line: {
        QPainterPath path;
        bool started = false;

        // Also draw a filled gradient underneath
        QPainterPath fillPath;

        for (int b = 1; b < binCount; ++b) {
            float freq = static_cast<float>(b) * binHz;
            if (freq < kMinFreqHz || freq > kMaxFreqHz) continue;

            float x = freqToX(freq, plotLeft, plotWidth);
            float db = magnitudeToDb(m_smoothed[static_cast<size_t>(b)]);
            float y = dbToY(db, plotTop, plotHeight);

            if (!started) {
                path.moveTo(x, y);
                fillPath.moveTo(x, plotTop + plotHeight);
                fillPath.lineTo(x, y);
                started = true;
            } else {
                path.lineTo(x, y);
                fillPath.lineTo(x, y);
            }
        }

        // Close fill path
        if (started) {
            float lastX = static_cast<float>(path.currentPosition().x());
            fillPath.lineTo(lastX, plotTop + plotHeight);
            fillPath.closeSubpath();

            // Gradient fill
            QLinearGradient grad(0, plotTop, 0, plotTop + plotHeight);
            grad.setColorAt(0.0, QColor(0, 200, 255, 60));
            grad.setColorAt(1.0, QColor(0, 200, 255, 5));
            p.setBrush(grad);
            p.setPen(Qt::NoPen);
            p.drawPath(fillPath);
        }

        // Line on top
        p.setPen(QPen(kLineColor, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        break;
    }

    case SpectrumMode::Waterfall: {
        // Waterfall / spectrogram: scroll history downward
        // Store current line in history buffer, then render
        if (m_waterfallHistory.empty() ||
            static_cast<int>(m_waterfallHistory[0].size()) != binCount) {
            int historyLines = qMax(1, static_cast<int>(plotHeight));
            m_waterfallHistory.resize(static_cast<size_t>(historyLines));
            for (auto& line : m_waterfallHistory) {
                line.resize(static_cast<size_t>(binCount), 0.0f);
            }
        }

        // Shift history down
        for (size_t row = m_waterfallHistory.size() - 1; row > 0; --row) {
            m_waterfallHistory[row] = m_waterfallHistory[row - 1];
        }
        // Insert current frame at top
        for (int b = 0; b < binCount; ++b) {
            m_waterfallHistory[0][static_cast<size_t>(b)] = m_smoothed[static_cast<size_t>(b)];
        }

        // Render waterfall image
        int rows = static_cast<int>(m_waterfallHistory.size());
        int displayRows = qMin(rows, static_cast<int>(plotHeight));
        for (int row = 0; row < displayRows; ++row) {
            float y = plotTop + static_cast<float>(row);
            for (int col = 0; col < static_cast<int>(plotWidth); ++col) {
                // Map pixel column to frequency bin via log scale
                float logMin = std::log10(kMinFreqHz);
                float logMax = std::log10(kMaxFreqHz);
                float logFreq = logMin + (logMax - logMin) * static_cast<float>(col) / plotWidth;
                float freq = std::pow(10.0f, logFreq);
                int bin = static_cast<int>(freq / binHz);
                bin = std::clamp(bin, 0, binCount - 1);

                float mag = m_waterfallHistory[static_cast<size_t>(row)][static_cast<size_t>(bin)];
                float db = magnitudeToDb(mag);
                float norm = (db - kMinDbDisplay) / (kMaxDbDisplay - kMinDbDisplay);
                norm = std::clamp(norm, 0.0f, 1.0f);

                // Heat-map color
                int r = static_cast<int>(norm * 255);
                int g = static_cast<int>(qMax(0.0f, (norm - 0.3f) * 1.4f) * 200);
                int b = static_cast<int>(qMax(0.0f, (1.0f - norm * 2.0f)) * 100);
                p.setPen(QColor(r, g, b));
                p.drawPoint(static_cast<int>(plotLeft) + col, static_cast<int>(y));
            }
        }
        break;
    }
    } // switch
}

} // namespace dawcast::widgets
