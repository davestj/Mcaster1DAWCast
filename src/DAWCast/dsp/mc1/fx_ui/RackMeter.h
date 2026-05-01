/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/RackMeter.h — Custom-painted vertical level meter
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Three meter styles for different contexts:
 *   INPUT_METER  — green-yellow-red gradient (signal level)
 *   GR_METER     — orange downward (gain reduction)
 *   OUTPUT_METER — green-yellow-red gradient (output level)
 *
 * Features:
 *   - 3D inset bezel look (dark border, inner shadow)
 *   - Gradient fill based on level
 *   - Peak hold indicator (white line, 2-second decay)
 *   - dB scale marks on the side
 *   - Current level readout at bottom
 */

#pragma once

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>
#include <QFontMetrics>
#include <QElapsedTimer>

#include <algorithm>
#include <cmath>

class RackMeter : public QWidget {
    Q_OBJECT

public:
    enum Style {
        INPUT_METER,
        GR_METER,
        OUTPUT_METER
    };
    Q_ENUM(Style)

    explicit RackMeter(Style style = INPUT_METER, QWidget* parent = nullptr)
        : QWidget(parent)
        , style_(style)
    {
        setMinimumSize(22, 135);
        setMaximumSize(44, 320);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        peakTimer_.start();
    }

    /* ── Accessors ── */

    float level() const { return level_; }
    float peakHold() const { return peakHold_; }
    Style meterStyle() const { return style_; }

    void setLevel(float db)
    {
        level_ = (db < kMinDb ? kMinDb : (db > kMaxDb ? kMaxDb : db));

        // Peak hold logic
        if (level_ > peakHold_ || peakTimer_.elapsed() > kPeakHoldMs) {
            if (level_ > peakHold_) {
                peakHold_ = level_;
                peakTimer_.restart();
            } else {
                // Decay: drop 20 dB/sec
                qint64 elapsed = peakTimer_.elapsed() - kPeakHoldMs;
                if (elapsed > 0) {
                    float decay = static_cast<float>(elapsed) / 1000.0f * 20.0f;
                    peakHold_ = std::max(peakHold_ - decay, kMinDb);
                }
            }
        }

        update();
    }

    void setPeakHold(float db)
    {
        peakHold_ = (db < kMinDb ? kMinDb : (db > kMaxDb ? kMaxDb : db));
        peakTimer_.restart();
        update();
    }

    void setMeterStyle(Style s)
    {
        style_ = s;
        update();
    }

    void reset()
    {
        level_ = kMinDb;
        peakHold_ = kMinDb;
        update();
    }

    QSize sizeHint() const override { return {30, 180}; }
    QSize minimumSizeHint() const override { return {30, 180}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int w = width();
        const int h = height();

        // Reserve space for dB readout at bottom
        const int readoutH  = 16;
        const int bezelTop  = 0;
        const int bezelH    = h - readoutH - 2;
        const int meterPad  = 3;  // inset from bezel edge
        const QRect bezelRect(0, bezelTop, w, bezelH);
        const QRect meterRect(meterPad, bezelTop + meterPad,
                              w - meterPad * 2, bezelH - meterPad * 2);

        // ── 1. Bezel (3D inset border) ──
        {
            // Outer border
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#1a1a2e"));
            p.drawRoundedRect(bezelRect, 3, 3);

            // Inner shadow (top-left lighter = inset look)
            QLinearGradient shadowGrad(0, bezelTop, 0, bezelTop + bezelH);
            shadowGrad.setColorAt(0.0, QColor(0, 0, 0, 100));
            shadowGrad.setColorAt(0.05, QColor(0, 0, 0, 50));
            shadowGrad.setColorAt(0.95, QColor(40, 40, 60, 30));
            shadowGrad.setColorAt(1.0, QColor(60, 60, 80, 60));
            p.setBrush(shadowGrad);
            p.drawRoundedRect(bezelRect.adjusted(1, 1, -1, -1), 2, 2);
        }

        // ── 2. Background ──
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#0a0a14"));
        p.drawRect(meterRect);

        // ── 3. Level fill ──
        const float normLevel = dbToNorm(level_);
        const int fillH = static_cast<int>(normLevel * meterRect.height());

        if (fillH > 0) {
            if (style_ == GR_METER) {
                // GR meter: orange from top downward
                QRect fillRect(meterRect.left(), meterRect.top(),
                               meterRect.width(), fillH);
                QLinearGradient grGrad(0, meterRect.top(), 0, meterRect.top() + fillH);
                grGrad.setColorAt(0.0, QColor("#FF9800"));
                grGrad.setColorAt(0.7, QColor("#E65100"));
                grGrad.setColorAt(1.0, QColor("#BF360C"));
                p.setBrush(grGrad);
                p.drawRect(fillRect);
            } else {
                // INPUT/OUTPUT: green-yellow-red from bottom up
                QRect fillRect(meterRect.left(),
                               meterRect.bottom() - fillH + 1,
                               meterRect.width(), fillH);

                QLinearGradient lvlGrad(0, meterRect.bottom(),
                                        0, meterRect.top());
                // Map dB thresholds to normalized positions for gradient stops
                float normMinus12 = dbToNorm(-12.0f);  // ~0.8
                float normMinus3  = dbToNorm(-3.0f);   // ~0.95
                lvlGrad.setColorAt(0.0,          QColor("#22cc66"));  // green at bottom
                lvlGrad.setColorAt(normMinus12,  QColor("#22cc66"));  // green to -12dB
                lvlGrad.setColorAt(normMinus12 + 0.01, QColor("#ffc107")); // yellow above -12
                lvlGrad.setColorAt(normMinus3,   QColor("#ffc107"));  // yellow to -3dB
                lvlGrad.setColorAt(normMinus3 + 0.01, QColor("#ff3d00")); // red above -3
                lvlGrad.setColorAt(1.0,          QColor("#ff1744"));  // bright red at 0

                p.setBrush(lvlGrad);
                p.drawRect(fillRect);
            }
        }

        // ── 4. Segmented look (horizontal lines every 2px) ──
        {
            p.setPen(QPen(QColor("#0a0a14"), 1.0));
            for (int y = meterRect.top(); y < meterRect.bottom(); y += 3) {
                p.drawLine(meterRect.left(), y, meterRect.right(), y);
            }
        }

        // ── 5. Peak hold indicator ──
        if (peakHold_ > kMinDb + 1.0f) {
            float normPeak = dbToNorm(peakHold_);
            int peakY;
            if (style_ == GR_METER) {
                peakY = meterRect.top() + static_cast<int>(normPeak * meterRect.height());
            } else {
                peakY = meterRect.bottom() - static_cast<int>(normPeak * meterRect.height());
            }

            if (peakY < meterRect.top()) peakY = meterRect.top();
            else if (peakY > meterRect.bottom()) peakY = meterRect.bottom();

            // Glow
            QPen glowPen(QColor(255, 255, 255, 60), 4.0, Qt::SolidLine, Qt::RoundCap);
            p.setPen(glowPen);
            p.drawLine(meterRect.left(), peakY, meterRect.right(), peakY);

            // Crisp line
            bool hot = (style_ != GR_METER && peakHold_ > -3.0f);
            QPen peakPen(hot ? QColor("#ff1744") : QColor("#ffffff"), 2.0,
                         Qt::SolidLine, Qt::RoundCap);
            p.setPen(peakPen);
            p.drawLine(meterRect.left(), peakY, meterRect.right(), peakY);
        }

        // ── 6. dB scale marks ──
        {
            static constexpr float dbMarks[] = {
                0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f
            };
            static constexpr int numMarks = sizeof(dbMarks) / sizeof(dbMarks[0]);

            QFont f = font();
            f.setPixelSize(7);
            f.setFamily("Menlo");
            p.setFont(f);

            for (int i = 0; i < numMarks; ++i) {
                float norm = dbToNorm(dbMarks[i]);
                int y;
                if (style_ == GR_METER) {
                    y = meterRect.top() + static_cast<int>(norm * meterRect.height());
                } else {
                    y = meterRect.bottom() - static_cast<int>(norm * meterRect.height());
                }

                if (y < meterRect.top() + 4 || y > meterRect.bottom() - 4)
                    continue;

                // Tick mark
                p.setPen(QPen(QColor(160, 170, 190, 140), 1.0));
                p.drawLine(meterRect.left(), y, meterRect.left() + 3, y);
                p.drawLine(meterRect.right() - 3, y, meterRect.right(), y);

                // dB label (only on wider meters, >= 40px)
                if (w >= 40) {
                    p.setPen(QColor(160, 170, 190, 180));
                    QString label = (dbMarks[i] == 0.0f)
                        ? QStringLiteral("0")
                        : QString::number(static_cast<int>(dbMarks[i]));
                    QRect textR(meterRect.left(), y - 5, meterRect.width(), 10);
                    p.drawText(textR, Qt::AlignCenter, label);
                }
            }
        }

        // ── 7. Level readout ──
        {
            QFont f = font();
            f.setPixelSize(9);
            f.setFamily("Menlo");
            f.setWeight(QFont::Bold);
            p.setFont(f);

            QColor textColor;
            if (style_ == GR_METER) {
                textColor = (level_ > kMinDb + 1.0f) ? QColor("#FF9800") : QColor("#606878");
            } else {
                if (level_ > -3.0f)
                    textColor = QColor("#ff3d00");
                else if (level_ > -12.0f)
                    textColor = QColor("#ffc107");
                else if (level_ > kMinDb + 1.0f)
                    textColor = QColor("#00d4aa");
                else
                    textColor = QColor("#606878");
            }

            p.setPen(textColor);
            QString readout;
            if (level_ <= kMinDb + 1.0f) {
                readout = QStringLiteral("-inf");
            } else {
                readout = QString::number(static_cast<double>(level_), 'f', 1);
            }

            QRect readoutRect(0, h - readoutH, w, readoutH);
            p.drawText(readoutRect, Qt::AlignCenter, readout);
        }
    }

private:
    /* ── dB to normalized (0.0..1.0) mapping ──
     *
     * Uses a log-like curve so that -60 to -12 dB occupies roughly the
     * bottom 60% and -12 to 0 dB occupies the top 40%. This gives
     * better visual resolution in the critical broadcast range.
     */
    static float dbToNorm(float db)
    {
        if (db <= kMinDb) return 0.0f;
        if (db >= kMaxDb) return 1.0f;

        // Piecewise linear for broadcast-friendly scaling:
        //   -60 to -12 dB  -->  0.0 to 0.6
        //   -12 to  -3 dB  -->  0.6 to 0.85
        //    -3 to   0 dB  -->  0.85 to 1.0
        if (db < -12.0f) {
            return 0.6f * (db - kMinDb) / (-12.0f - kMinDb);
        } else if (db < -3.0f) {
            return 0.6f + 0.25f * (db + 12.0f) / 9.0f;
        } else {
            return 0.85f + 0.15f * (db + 3.0f) / 3.0f;
        }
    }

    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb =   0.0f;
    static constexpr qint64 kPeakHoldMs = 2000;  // 2-second hold

    float  level_    = kMinDb;
    float  peakHold_ = kMinDb;
    Style  style_    = INPUT_METER;

    QElapsedTimer peakTimer_;
};
