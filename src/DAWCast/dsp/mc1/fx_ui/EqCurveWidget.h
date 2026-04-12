/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/EqCurveWidget.h — Frequency response Bode magnitude plot
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Renders an anti-aliased frequency response curve for EQ visualization.
 * Supports both 10-band parametric and 31-band graphic modes.
 * Dual-channel mode draws L (teal) and R (orange) curves.
 *
 * Features:
 *   - Log-frequency X axis (20 Hz to 20 kHz)
 *   - Linear dB Y axis (-18 to +18 dB, or -24 to +24)
 *   - Grid lines at decade frequencies + 6 dB intervals
 *   - Frequency labels along bottom
 *   - dB labels along left
 *   - Filled area under curve (semi-transparent)
 *   - Band position markers (dots at center frequencies)
 *   - Dark background matching rack unit aesthetic
 *
 * Header-only. Uses RBJ Audio EQ Cookbook biquad magnitude formulas
 * consistent with fx_parametric_eq.h / fx_graphic_eq31.h / fx_dual_eq15.h.
 */

#pragma once

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QLinearGradient>

#include <cmath>
#include <algorithm>
#include <vector>

class EqCurveWidget : public QWidget {
    Q_OBJECT

public:
    explicit EqCurveWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(minimumSizeHint());
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    /* ── 10-band parametric mode ──────────────────────────────────── */

    void setBands(const float gains[10], const float freqs[10], int sampleRate)
    {
        mode_ = Mode::Parametric10;
        numBands_ = 10;
        sampleRate_ = std::max(sampleRate, 1);

        for (int i = 0; i < 10; ++i) {
            bandsL_[i].gainDb    = gains[i];
            bandsL_[i].freq      = freqs[i];
            bandsL_[i].Q         = 1.0f;
            bandsL_[i].type      = (i == 0) ? FilterType::LowShelf
                                 : (i == 9) ? FilterType::HighShelf
                                            : FilterType::Peaking;
        }

        update();
    }

    /* ── 31-band graphic mode (stereo-linked) ─────────────────────── */

    void setBands31(const float gains[31], int sampleRate)
    {
        static constexpr float kFreqs31[31] = {
               20.0f,    25.0f,    31.5f,    40.0f,    50.0f,
               63.0f,    80.0f,   100.0f,   125.0f,   160.0f,
              200.0f,   250.0f,   315.0f,   400.0f,   500.0f,
              630.0f,   800.0f,  1000.0f,  1250.0f,  1600.0f,
             2000.0f,  2500.0f,  3150.0f,  4000.0f,  5000.0f,
             6300.0f,  8000.0f, 10000.0f, 12500.0f, 16000.0f,
            20000.0f
        };

        mode_ = Mode::Graphic31;
        numBands_ = 31;
        sampleRate_ = std::max(sampleRate, 1);

        for (int i = 0; i < 31; ++i) {
            bandsL_[i].gainDb = gains[i];
            bandsL_[i].freq   = kFreqs31[i];
            bandsL_[i].Q      = 4.3f;  /* constant-Q for 1/3-octave */
            bandsL_[i].type   = (i == 0)  ? FilterType::LowShelf
                              : (i == 30) ? FilterType::HighShelf
                                          : FilterType::Peaking;
        }

        update();
    }

    /* ── Dual 15-band mode (independent L/R) ──────────────────────── */

    void setDualBands15(const float gainsL[15], const float gainsR[15],
                        const float freqs[15], int sampleRate)
    {
        mode_ = Mode::Dual15;
        numBands_ = 15;
        sampleRate_ = std::max(sampleRate, 1);

        for (int i = 0; i < 15; ++i) {
            FilterType type = (i == 0)  ? FilterType::LowShelf
                            : (i == 14) ? FilterType::HighShelf
                                        : FilterType::Peaking;
            float Q = 2.0f;

            bandsL_[i] = { gainsL[i], freqs[i], Q, type };
            bandsR_[i] = { gainsR[i], freqs[i], Q, type };
        }

        update();
    }

    /* ── Configuration ────────────────────────────────────────────── */

    void setDbRange(float maxDb)
    {
        maxDb_ = (maxDb < 6.0f ? 6.0f : (maxDb > 48.0f ? 48.0f : maxDb));
        update();
    }

    void setSelectedBand(int bandIndex)
    {
        selectedBand_ = bandIndex;
        update();
    }

    /* ── QWidget overrides ────────────────────────────────────────── */

    QSize minimumSizeHint() const override { return QSize(400, 200); }
    QSize sizeHint()        const override { return QSize(600, 280); }

    /* ── Static magnitude helpers (RBJ Audio EQ Cookbook) ──────────── */

    static float peakingMagnitudeDb(float gainDb, float centerFreq, float Q,
                                    int sampleRate, float freqHz)
    {
        if (std::fabs(gainDb) < 0.001f) return 0.0f;

        const float sr = static_cast<float>(sampleRate);
        const float A     = std::pow(10.0f, gainDb / 40.0f);
        const float w0    = kTwoPi * centerFreq / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float b0 =  1.0f + alpha * A;
        const float b1 = -2.0f * cosw0;
        const float b2 =  1.0f - alpha * A;
        const float a0 =  1.0f + alpha / A;
        const float a1 = -2.0f * cosw0;
        const float a2 =  1.0f - alpha / A;

        return evalMagnitudeDb(b0, b1, b2, a0, a1, a2, freqHz, sr);
    }

    static float lowShelfMagnitudeDb(float gainDb, float centerFreq,
                                     int sampleRate, float freqHz)
    {
        if (std::fabs(gainDb) < 0.001f) return 0.0f;

        const float sr = static_cast<float>(sampleRate);
        const float A     = std::pow(10.0f, gainDb / 40.0f);
        const float w0    = kTwoPi * centerFreq / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float Q     = 0.707f;
        const float alpha = sinw0 / (2.0f * Q);
        const float sqA   = std::sqrt(A);

        const float b0 =        A * ((A+1) - (A-1)*cosw0 + 2*sqA*alpha);
        const float b1 =  2.0f *A * ((A-1) - (A+1)*cosw0);
        const float b2 =        A * ((A+1) - (A-1)*cosw0 - 2*sqA*alpha);
        const float a0 =              (A+1) + (A-1)*cosw0 + 2*sqA*alpha;
        const float a1 = -2.0f *     ((A-1) + (A+1)*cosw0);
        const float a2 =              (A+1) + (A-1)*cosw0 - 2*sqA*alpha;

        return evalMagnitudeDb(b0, b1, b2, a0, a1, a2, freqHz, sr);
    }

    static float highShelfMagnitudeDb(float gainDb, float centerFreq,
                                      int sampleRate, float freqHz)
    {
        if (std::fabs(gainDb) < 0.001f) return 0.0f;

        const float sr = static_cast<float>(sampleRate);
        const float A     = std::pow(10.0f, gainDb / 40.0f);
        const float w0    = kTwoPi * centerFreq / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float Q     = 0.707f;
        const float alpha = sinw0 / (2.0f * Q);
        const float sqA   = std::sqrt(A);

        const float b0 =        A * ((A+1) + (A-1)*cosw0 + 2*sqA*alpha);
        const float b1 = -2.0f *A * ((A-1) + (A+1)*cosw0);
        const float b2 =        A * ((A+1) + (A-1)*cosw0 - 2*sqA*alpha);
        const float a0 =              (A+1) - (A-1)*cosw0 + 2*sqA*alpha;
        const float a1 =  2.0f *     ((A-1) - (A+1)*cosw0);
        const float a2 =              (A+1) - (A-1)*cosw0 - 2*sqA*alpha;

        return evalMagnitudeDb(b0, b1, b2, a0, a1, a2, freqHz, sr);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const int   w   = width();
        const int   h   = height();

        /* Plot area with margins for labels */
        const int marginL = 42;  /* dB labels */
        const int marginR = 12;
        const int marginT = 10;
        const int marginB = 22;  /* freq labels */

        const QRect plot(marginL, marginT, w - marginL - marginR,
                         h - marginT - marginB);
        if (plot.width() < 10 || plot.height() < 10) return;

        /* ── 1. Background ─────────────────────────────────────────── */

        QLinearGradient bgGrad(0, 0, 0, h);
        bgGrad.setColorAt(0.0, QColor(0x12, 0x1a, 0x28));
        bgGrad.setColorAt(1.0, QColor(0x0c, 0x12, 0x1c));
        p.fillRect(rect(), bgGrad);

        /* Subtle inner glow at top of plot area */
        QLinearGradient glowGrad(plot.left(), plot.top(),
                                 plot.left(), plot.top() + 40);
        glowGrad.setColorAt(0.0, QColor(255, 255, 255, 6));
        glowGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(plot, glowGrad);

        /* ── 2. Grid lines ─────────────────────────────────────────── */

        drawGrid(p, plot);

        /* ── 3. Frequency response curve(s) ────────────────────────── */

        if (mode_ == Mode::Dual15) {
            /* Draw R first (underneath), then L on top */
            drawCurve(p, plot, bandsR_, numBands_, kColorR, kFillAlpha);
            drawCurve(p, plot, bandsL_, numBands_, kColorL, kFillAlpha);
        } else {
            drawCurve(p, plot, bandsL_, numBands_, kColorL, kFillAlpha);
        }

        /* ── 4. Band markers ───────────────────────────────────────── */

        if (mode_ == Mode::Dual15) {
            drawBandMarkers(p, plot, bandsR_, numBands_, kColorR, -1);
            drawBandMarkers(p, plot, bandsL_, numBands_, kColorL, selectedBand_);
        } else {
            drawBandMarkers(p, plot, bandsL_, numBands_, kColorL, selectedBand_);
        }

        /* ── 5. Plot border ────────────────────────────────────────── */

        p.setPen(QPen(QColor(0x2a, 0x35, 0x45), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(plot);
    }

private:
    /* ── Constants ─────────────────────────────────────────────────── */

    static constexpr float kTwoPi   = 6.28318530718f;
    static constexpr float kMinFreq = 20.0f;
    static constexpr float kMaxFreq = 20000.0f;
    /* log10(maxFreq/minFreq) = log10(1000) = 3 */
    static constexpr float kLogRange = 3.0f;

    static constexpr int   kFillAlpha = 40;
    static constexpr int   kMaxBands  = 31;

    /* Curve colors */
    static inline const QColor kColorL = QColor(0x00, 0xd4, 0xaa);  /* teal  */
    static inline const QColor kColorR = QColor(0xFF, 0x98, 0x00);  /* orange */

    /* Grid colors */
    static inline const QColor kGridColor   = QColor(0x1a, 0x25, 0x30);
    static inline const QColor kZeroDbColor = QColor(0x2a, 0x35, 0x40);
    static inline const QColor kLabelColor  = QColor(0x60, 0x70, 0x80);

    /* ── Filter data ──────────────────────────────────────────────── */

    enum class FilterType { LowShelf, Peaking, HighShelf };

    enum class Mode { Parametric10, Graphic31, Dual15 };

    struct BandInfo {
        float      gainDb = 0.0f;
        float      freq   = 1000.0f;
        float      Q      = 1.0f;
        FilterType type    = FilterType::Peaking;
    };

    Mode     mode_         = Mode::Parametric10;
    int      numBands_     = 0;
    int      sampleRate_   = 48000;
    float    maxDb_        = 18.0f;
    int      selectedBand_ = -1;
    BandInfo bandsL_[kMaxBands] = {};
    BandInfo bandsR_[kMaxBands] = {};

    /* ── Coordinate transforms ────────────────────────────────────── */

    /* freq -> normalized x [0..1] using log scale */
    static float freqToNorm(float freq)
    {
        if (freq <= kMinFreq) return 0.0f;
        if (freq >= kMaxFreq) return 1.0f;
        return std::log10(freq / kMinFreq) / kLogRange;
    }

    /* Pixel x within plot -> frequency */
    static float pixelToFreq(int px, int plotLeft, int plotWidth)
    {
        float norm = static_cast<float>(px - plotLeft) / plotWidth;
        return kMinFreq * std::pow(10.0f, norm * kLogRange);
    }

    /* dB -> normalized y [0..1] (top = +maxDb, bottom = -maxDb) */
    float dbToNorm(float db) const
    {
        return 0.5f - (db / (2.0f * maxDb_));
    }

    /* ── Biquad magnitude evaluation ──────────────────────────────── */

    static float evalMagnitudeDb(float b0, float b1, float b2,
                                 float a0, float a1, float a2,
                                 float freqHz, float sr)
    {
        /* Normalize coefficients */
        const float nb0 = b0 / a0;
        const float nb1 = b1 / a0;
        const float nb2 = b2 / a0;
        const float na1 = a1 / a0;
        const float na2 = a2 / a0;

        /* Evaluate H(e^jw) at w = 2*pi*freq/sr */
        const float w = kTwoPi * freqHz / sr;
        const float cosw  = std::cos(w);
        const float cos2w = std::cos(2.0f * w);
        const float sinw  = std::sin(w);
        const float sin2w = std::sin(2.0f * w);

        /* Numerator: b0 + b1*e^-jw + b2*e^-2jw */
        const float numRe = nb0 + nb1 * cosw + nb2 * cos2w;
        const float numIm =     - nb1 * sinw - nb2 * sin2w;

        /* Denominator: 1 + a1*e^-jw + a2*e^-2jw */
        const float denRe = 1.0f + na1 * cosw + na2 * cos2w;
        const float denIm =      - na1 * sinw - na2 * sin2w;

        const float numMagSq = numRe * numRe + numIm * numIm;
        const float denMagSq = denRe * denRe + denIm * denIm;

        if (denMagSq < 1.0e-20f) return 0.0f;

        return 10.0f * std::log10(numMagSq / denMagSq);
    }

    /* Compute single band magnitude in dB at a given frequency */
    float bandMagnitudeDb(const BandInfo& band, float freqHz) const
    {
        if (std::fabs(band.gainDb) < 0.001f) return 0.0f;

        const float sr    = static_cast<float>(sampleRate_);
        const float A     = std::pow(10.0f, band.gainDb / 40.0f);
        const float w0    = kTwoPi * band.freq / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * band.Q);

        float b0, b1, b2, a0, a1, a2;

        switch (band.type) {
        case FilterType::Peaking:
            b0 =  1.0f + alpha * A;
            b1 = -2.0f * cosw0;
            b2 =  1.0f - alpha * A;
            a0 =  1.0f + alpha / A;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha / A;
            break;
        case FilterType::LowShelf: {
            float sqA = std::sqrt(A);
            b0 =        A * ((A+1) - (A-1)*cosw0 + 2*sqA*alpha);
            b1 =  2.0f *A * ((A-1) - (A+1)*cosw0);
            b2 =        A * ((A+1) - (A-1)*cosw0 - 2*sqA*alpha);
            a0 =              (A+1) + (A-1)*cosw0 + 2*sqA*alpha;
            a1 = -2.0f *     ((A-1) + (A+1)*cosw0);
            a2 =              (A+1) + (A-1)*cosw0 - 2*sqA*alpha;
            break;
        }
        case FilterType::HighShelf: {
            float sqA = std::sqrt(A);
            b0 =        A * ((A+1) + (A-1)*cosw0 + 2*sqA*alpha);
            b1 = -2.0f *A * ((A-1) + (A+1)*cosw0);
            b2 =        A * ((A+1) + (A-1)*cosw0 - 2*sqA*alpha);
            a0 =              (A+1) - (A-1)*cosw0 + 2*sqA*alpha;
            a1 =  2.0f *     ((A-1) - (A+1)*cosw0);
            a2 =              (A+1) - (A-1)*cosw0 - 2*sqA*alpha;
            break;
        }
        }

        return evalMagnitudeDb(b0, b1, b2, a0, a1, a2, freqHz,
                               static_cast<float>(sampleRate_));
    }

    /* Sum total magnitude across all bands for one channel */
    float totalMagnitudeDb(const BandInfo* bands, int count, float freqHz) const
    {
        float total = 0.0f;
        for (int i = 0; i < count; ++i)
            total += bandMagnitudeDb(bands[i], freqHz);
        return total;
    }

    /* ── Grid drawing ─────────────────────────────────────────────── */

    void drawGrid(QPainter& p, const QRect& plot) const
    {
        /* Frequency grid lines (vertical) */
        static constexpr float kGridFreqs[] = {
            20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000
        };
        static constexpr const char* kGridLabels[] = {
            "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"
        };
        static constexpr int kNumGridFreqs = 10;

        QFont labelFont;
        labelFont.setPixelSize(10);
        labelFont.setFamily("Menlo");
        p.setFont(labelFont);

        const QPen gridPen(kGridColor, 1.0, Qt::SolidLine);
        const QPen zeroPen(kZeroDbColor, 1.0, Qt::SolidLine);

        /* Vertical frequency lines */
        for (int i = 0; i < kNumGridFreqs; ++i) {
            float norm = freqToNorm(kGridFreqs[i]);
            int x = plot.left() + static_cast<int>(norm * plot.width());

            p.setPen(gridPen);
            p.drawLine(x, plot.top(), x, plot.bottom());

            /* Frequency label below plot */
            p.setPen(kLabelColor);
            QRect labelRect(x - 20, plot.bottom() + 3, 40, 16);
            p.drawText(labelRect, Qt::AlignCenter, kGridLabels[i]);
        }

        /* Sub-grid: additional faint lines at intermediate frequencies */
        static constexpr float kSubFreqs[] = {
            30, 40, 60, 70, 80, 150, 300, 400, 600, 700, 800,
            1500, 3000, 4000, 6000, 7000, 8000, 15000
        };
        QPen subPen(QColor(0x14, 0x1e, 0x28), 1.0, Qt::SolidLine);
        for (float f : kSubFreqs) {
            float norm = freqToNorm(f);
            int x = plot.left() + static_cast<int>(norm * plot.width());
            p.setPen(subPen);
            p.drawLine(x, plot.top(), x, plot.bottom());
        }

        /* Horizontal dB lines */
        /* Compute dB step: use 6 dB for ranges >= 12, 3 dB for smaller */
        float dbStep = (maxDb_ >= 12.0f) ? 6.0f : 3.0f;

        for (float db = -maxDb_; db <= maxDb_ + 0.01f; db += dbStep) {
            float norm = dbToNorm(db);
            int y = plot.top() + static_cast<int>(norm * plot.height());

            bool isZero = (std::fabs(db) < 0.01f);
            p.setPen(isZero ? zeroPen : gridPen);
            p.drawLine(plot.left(), y, plot.right(), y);

            /* dB label to the left of plot */
            p.setPen(isZero ? QColor(0x80, 0x90, 0xa0) : kLabelColor);
            char buf[8];
            if (isZero)
                snprintf(buf, sizeof(buf), "0");
            else
                snprintf(buf, sizeof(buf), "%+.0f", db);

            QRect labelRect(0, y - 7, plot.left() - 4, 14);
            p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter,
                       QString::fromLatin1(buf));
        }
    }

    /* ── Curve drawing ────────────────────────────────────────────── */

    void drawCurve(QPainter& p, const QRect& plot,
                   const BandInfo* bands, int count,
                   const QColor& color, int fillAlpha) const
    {
        if (count <= 0) return;

        const int pw = plot.width();
        const int ph = plot.height();

        /* Build the response path pixel by pixel */
        QPainterPath curvePath;
        bool started = false;

        /* Also build a closed fill path */
        QPainterPath fillPath;

        /* Walk every pixel column in the plot area */
        for (int px = 0; px <= pw; ++px) {
            float freq = pixelToFreq(plot.left() + px, plot.left(), pw);
            float db   = totalMagnitudeDb(bands, count, freq);

            /* Clamp to display range */
            db = (db < -maxDb_ ? -maxDb_ : (db > maxDb_ ? maxDb_ : db));

            float normY = dbToNorm(db);
            float y = plot.top() + normY * ph;

            if (!started) {
                curvePath.moveTo(plot.left() + px, y);
                fillPath.moveTo(plot.left() + px, y);
                started = true;
            } else {
                curvePath.lineTo(plot.left() + px, y);
                fillPath.lineTo(plot.left() + px, y);
            }
        }

        /* Close fill path along 0 dB line */
        float zeroY = plot.top() + dbToNorm(0.0f) * ph;
        fillPath.lineTo(plot.right(), zeroY);
        fillPath.lineTo(plot.left(), zeroY);
        fillPath.closeSubpath();

        /* Draw filled area */
        QColor fillColor = color;
        fillColor.setAlpha(fillAlpha);
        p.setPen(Qt::NoPen);
        p.setBrush(fillColor);
        p.drawPath(fillPath);

        /* Draw curve line with glow effect */
        /* Outer glow (wider, more transparent) */
        QColor glowColor = color;
        glowColor.setAlpha(30);
        p.setPen(QPen(glowColor, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curvePath);

        /* Main curve */
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(curvePath);
    }

    /* ── Band markers ─────────────────────────────────────────────── */

    void drawBandMarkers(QPainter& p, const QRect& plot,
                         const BandInfo* bands, int count,
                         const QColor& color, int selected) const
    {
        const int pw = plot.width();
        const int ph = plot.height();

        for (int i = 0; i < count; ++i) {
            float freq = bands[i].freq;
            if (freq < kMinFreq || freq > kMaxFreq) continue;

            float normX = freqToNorm(freq);
            float x = plot.left() + normX * pw;

            float db = totalMagnitudeDb(bands, count, freq);
            db = (db < -maxDb_ ? -maxDb_ : (db > maxDb_ ? maxDb_ : db));
            float normY = dbToNorm(db);
            float y = plot.top() + normY * ph;

            bool isSelected = (i == selected);
            float radius = isSelected ? 5.0f : 3.0f;

            /* Marker dot */
            if (isSelected) {
                /* Outer glow for selected band */
                QColor glow = color;
                glow.setAlpha(60);
                p.setPen(Qt::NoPen);
                p.setBrush(glow);
                p.drawEllipse(QPointF(x, y), radius + 4.0f, radius + 4.0f);
            }

            /* Bright dot */
            QColor dotColor = isSelected ? color.lighter(140) : color;
            p.setPen(QPen(dotColor.darker(120), 1.0));
            p.setBrush(dotColor);
            p.drawEllipse(QPointF(x, y), radius, radius);

            /* Label for selected band */
            if (isSelected) {
                QFont markerFont;
                markerFont.setPixelSize(10);
                markerFont.setBold(true);
                p.setFont(markerFont);

                char label[32];
                if (freq >= 1000.0f)
                    snprintf(label, sizeof(label), "%.1fk  %+.1fdB",
                             freq / 1000.0f, db);
                else
                    snprintf(label, sizeof(label), "%.0f Hz  %+.1fdB",
                             freq, db);

                /* Position label above or below marker depending on dB */
                QRect labelRect;
                int lw = 90, lh = 14;
                int lx = static_cast<int>(x) - lw / 2;
                int ly = (db >= 0) ? static_cast<int>(y) - radius - lh - 4
                                   : static_cast<int>(y) + radius + 4;

                /* Keep label within plot bounds */
                if (lx < plot.left()) lx = plot.left();
                else if (lx > plot.right() - lw) lx = plot.right() - lw;
                if (ly < plot.top()) ly = plot.top();
                else if (ly > plot.bottom() - lh) ly = plot.bottom() - lh;

                labelRect = QRect(lx, ly, lw, lh);

                /* Background pill */
                QColor pillBg(0x0f, 0x15, 0x20, 200);
                p.setPen(Qt::NoPen);
                p.setBrush(pillBg);
                p.drawRoundedRect(labelRect.adjusted(-2, -1, 2, 1), 3, 3);

                /* Text */
                p.setPen(color.lighter(160));
                p.drawText(labelRect, Qt::AlignCenter,
                           QString::fromLatin1(label));
            }
        }
    }
};
