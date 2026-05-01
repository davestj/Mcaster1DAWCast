/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/RackKnob.h — Custom-painted 3D rotary knob widget
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Replaces QDial with a high-res custom-painted knob that looks like
 * a real hardware rack unit control. Features:
 *   - 3D metallic body (radial gradient, brushed metal look)
 *   - Illuminated position indicator line
 *   - Arc track showing value range (270-degree sweep)
 *   - Value label below the knob
 *   - Title label above the knob
 *   - Notch marks at regular intervals
 *   - Mouse drag (vertical) and scroll wheel to change value
 */

#pragma once

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QFont>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>

class RackKnob : public QWidget {
    Q_OBJECT
    Q_PROPERTY(float value READ value WRITE setValue NOTIFY valueChanged)

public:
    /// Visual skin for the knob body. Chosen per-plugin to give each MC1
    /// interface its own hardware vibe while keeping interaction uniform.
    enum Style {
        Modern       = 0, ///< Brushed metal chrome (default)
        Chicken      = 1, ///< Vintage chicken-head pointer, bakelite body
        Bellcap      = 2, ///< Rubber bellcap/silicone top, muted matte
        SoftLED      = 3, ///< Glowing LED indicator ring on dark metal
        VintageBakelite = 4 ///< Black/brown bakelite with ivory indicator
    };

    explicit RackKnob(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(44, 58);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(false);
    }

    Style style() const { return style_; }
    void  setStyle(Style s) { style_ = s; update(); }

    /* ── Accessors ── */

    float value() const { return value_; }

    void setValue(float v)
    {
        v = (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v));
        if (std::abs(v - value_) < 1e-6f) return;
        value_ = v;
        update();
        emit valueChanged(value_);
    }

    float defaultValue() const { return default_; }
    void setDefaultValue(float d) { default_ = (d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d)); }

    QString title() const { return title_; }
    void setTitle(const QString& t) { title_ = t; update(); }

    QString valueText() const { return valueText_; }
    void setValueText(const QString& t) { valueText_ = t; update(); }

    int notches() const { return notches_; }
    void setNotches(int n) { notches_ = std::max(0, n); update(); }

    QColor accentColor() const { return accent_; }
    void setAccentColor(const QColor& c) { accent_ = c; update(); }

    QSize sizeHint() const override { return {60, 80}; }
    QSize minimumSizeHint() const override { return {44, 58}; }

signals:
    void valueChanged(float value);

protected:
    /* ── Paint ── */

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const int w = width();
        const int h = height();

        // Layout geometry
        const int titleH    = 18;                         // space for title
        const int valueH    = 18;                         // space for value text
        const int knobDiam  = std::min(w - 8, h - titleH - valueH - 8);
        const int knobR     = knobDiam / 2;
        const QPointF center(w / 2.0, titleH + knobR + 2);

        // ── 1. Title text ──
        if (!title_.isEmpty()) {
            QFont f = font();
            f.setPixelSize(10);
            f.setWeight(QFont::Medium);
            p.setFont(f);
            p.setPen(QColor("#d0e0f0"));
            p.drawText(QRect(0, 0, w, titleH), Qt::AlignCenter, title_);
        }

        // ── 2. Arc track (full 270-degree range) ──
        const qreal arcR     = knobR + 5;
        const QRectF arcRect(center.x() - arcR, center.y() - arcR,
                             arcR * 2.0, arcR * 2.0);

        // Qt angles: 0 = 3 o'clock, positive = counter-clockwise
        // 270-degree sweep: start at 225 deg (7 o'clock), sweep -270 to 315 deg (5 o'clock)
        const int startAngle = 225 * 16;   // bottom-left (7 o'clock)
        const int spanAngle  = -270 * 16;  // clockwise 270 degrees

        QPen trackPen(QColor("#2a3a4c"), 2.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(trackPen);
        p.drawArc(arcRect, startAngle, spanAngle);

        // ── 3. Value arc (filled portion) ──
        if (value_ > 0.001f) {
            int valueSpan = static_cast<int>(-270.0 * 16.0 * value_);
            QPen valuePen(accent_, 3.0, Qt::SolidLine, Qt::RoundCap);
            p.setPen(valuePen);
            p.drawArc(arcRect, startAngle, valueSpan);
        }

        // ── 4. Notch marks ──
        if (notches_ >= 2) {
            p.setPen(QPen(QColor(100, 110, 130, 160), 1.0));
            const qreal notchInner = arcR + 3;
            const qreal notchOuter = arcR + 7;
            for (int i = 0; i < notches_; ++i) {
                qreal frac  = static_cast<qreal>(i) / (notches_ - 1);
                qreal angle = degreesToRadians(225.0 - 270.0 * frac);
                qreal cs    = std::cos(angle);
                qreal sn    = std::sin(angle);
                p.drawLine(QPointF(center.x() + notchInner * cs,
                                   center.y() - notchInner * sn),
                           QPointF(center.x() + notchOuter * cs,
                                   center.y() - notchOuter * sn));
            }
        }

        // ── 5. Knob body — dispatched per style ──
        drawKnobBody(p, center, knobR);

        // ── 6. Position indicator — dispatched per style ──
        drawIndicator(p, center, knobR);

        // ── 7. Value text ──
        if (!valueText_.isEmpty()) {
            QFont f = font();
            f.setPixelSize(10);
            f.setFamily("Menlo");  // monospace for value readout
            f.setWeight(QFont::Normal);
            p.setFont(f);
            p.setPen(accent_);
            int textY = static_cast<int>(center.y() + knobR + 8);
            p.drawText(QRect(0, textY, w, valueH), Qt::AlignCenter, valueText_);
        }
    }

    /* ── Interaction ── */

    void mousePressEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton) {
            dragging_ = true;
            dragStartY_ = ev->position().y();
            dragStartVal_ = value_;
            setCursor(Qt::ClosedHandCursor);
            ev->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* ev) override
    {
        if (dragging_) {
            qreal dy = dragStartY_ - ev->position().y();  // up = positive
            float delta = static_cast<float>(dy / 200.0);  // 200px = full range
            setValue((dragStartVal_ + delta < 0.0f ? 0.0f : (dragStartVal_ + delta > 1.0f ? 1.0f : dragStartVal_ + delta)));
            ev->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            setCursor(Qt::PointingHandCursor);
            ev->accept();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* ev) override
    {
        setValue(default_);
        ev->accept();
    }

    void wheelEvent(QWheelEvent* ev) override
    {
        int steps = ev->angleDelta().y() / 120;  // 120 units per notch
        float delta = steps * 0.01f;
        setValue((value_ + delta < 0.0f ? 0.0f : (value_ + delta > 1.0f ? 1.0f : value_ + delta)));
        ev->accept();
    }

private:
    static constexpr qreal kPi = 3.14159265358979323846;

    static qreal degreesToRadians(qreal deg) { return deg * kPi / 180.0; }

    /* Paint the knob body according to style_. */
    void drawKnobBody(QPainter& p, const QPointF& center, int knobR)
    {
        // Outer dark ring (all styles)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#2a2a2e"));
        p.drawEllipse(center, knobR + 1.0, knobR + 1.0);

        switch (style_) {
        case Chicken: {
            // Bakelite body — warm brown-black with subtle highlight
            QRadialGradient g(center, knobR,
                              center + QPointF(-knobR * 0.3, -knobR * 0.4));
            g.setColorAt(0.0, QColor("#4a3a2a"));
            g.setColorAt(0.5, QColor("#2a1e14"));
            g.setColorAt(1.0, QColor("#100a06"));
            p.setBrush(g);
            p.drawEllipse(center, knobR, knobR);
            // Rim ring
            p.setPen(QPen(QColor("#605040"), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, knobR - 1, knobR - 1);
            break;
        }
        case Bellcap: {
            // Soft silicone rubber top — matte, slightly flattened highlight
            QRadialGradient g(center, knobR,
                              center + QPointF(0, -knobR * 0.3));
            g.setColorAt(0.0, QColor("#4a4a50"));
            g.setColorAt(0.4, QColor("#30303a"));
            g.setColorAt(1.0, QColor("#14141a"));
            p.setBrush(g);
            p.drawEllipse(center, knobR, knobR);
            // Subtle top highlight — long narrow ellipse
            QRadialGradient hi(center - QPointF(0, knobR * 0.4),
                                knobR * 0.7, center - QPointF(0, knobR * 0.4));
            hi.setColorAt(0.0, QColor(255, 255, 255, 40));
            hi.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(hi);
            p.drawEllipse(center - QPointF(0, knobR * 0.25),
                          knobR * 0.75, knobR * 0.4);
            break;
        }
        case SoftLED: {
            // Dark metal body with accent-colored glow ring
            QRadialGradient g(center, knobR,
                              center + QPointF(-knobR * 0.25, -knobR * 0.25));
            g.setColorAt(0.0, QColor("#2a2e34"));
            g.setColorAt(0.6, QColor("#181a1e"));
            g.setColorAt(1.0, QColor("#08080c"));
            p.setBrush(g);
            p.drawEllipse(center, knobR, knobR);
            // LED accent ring just inside the rim
            QColor glow(accent_);
            glow.setAlpha(180);
            p.setPen(QPen(glow, 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, knobR - 3, knobR - 3);
            break;
        }
        case VintageBakelite: {
            // Pure black-brown bakelite, tiny ivory-accent ring
            QRadialGradient g(center, knobR,
                              center + QPointF(-knobR * 0.2, -knobR * 0.35));
            g.setColorAt(0.0, QColor("#242018"));
            g.setColorAt(0.6, QColor("#14100a"));
            g.setColorAt(1.0, QColor("#050302"));
            p.setBrush(g);
            p.drawEllipse(center, knobR, knobR);
            p.setPen(QPen(QColor("#e8d8a8"), 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, knobR - 2, knobR - 2);
            break;
        }
        case Modern:
        default: {
            // Brushed chrome — original look
            QRadialGradient g(center, knobR,
                              center + QPointF(-knobR * 0.3, -knobR * 0.3));
            g.setColorAt(0.0, QColor("#d8d8d8"));
            g.setColorAt(0.35, QColor("#c0c0c0"));
            g.setColorAt(0.7, QColor("#909090"));
            g.setColorAt(1.0, QColor("#606060"));
            p.setBrush(g);
            p.drawEllipse(center, knobR, knobR);

            QConicalGradient cg(center, 135);
            cg.setColorAt(0.0,  QColor(255, 255, 255, 18));
            cg.setColorAt(0.25, QColor(0, 0, 0, 12));
            cg.setColorAt(0.5,  QColor(255, 255, 255, 22));
            cg.setColorAt(0.75, QColor(0, 0, 0, 10));
            cg.setColorAt(1.0,  QColor(255, 255, 255, 18));
            p.setBrush(cg);
            p.drawEllipse(center, knobR - 1, knobR - 1);

            // Inner concave dimple
            const qreal dimpleR = knobR * 0.25;
            QRadialGradient dg(center, dimpleR,
                               center + QPointF(dimpleR * 0.2, dimpleR * 0.2));
            dg.setColorAt(0.0, QColor("#505050"));
            dg.setColorAt(1.0, QColor("#808080"));
            p.setBrush(dg);
            p.drawEllipse(center, dimpleR, dimpleR);
            break;
        }
        }
    }

    /* Paint the position indicator per style. */
    void drawIndicator(QPainter& p, const QPointF& center, int knobR)
    {
        const qreal angle  = degreesToRadians(225.0 - 270.0 * value_);
        const qreal cs     = std::cos(angle);
        const qreal sn     = std::sin(angle);

        if (style_ == Chicken) {
            // Chicken-head pointer — triangular beak protruding past the rim
            p.save();
            p.translate(center);
            p.rotate(90.0 - (225.0 - 270.0 * value_));
            QPainterPath beak;
            beak.moveTo(0, -knobR * 0.20);           // base left
            beak.lineTo(knobR * 0.55, -knobR * 0.05); // tip up
            beak.lineTo(knobR * 0.60, knobR * 0.05);  // tip down
            beak.lineTo(0, knobR * 0.20);             // base right
            beak.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#f4e0a8"));           // ivory beak
            p.drawPath(beak);
            p.setPen(QPen(QColor("#604020"), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawPath(beak);
            p.restore();
            return;
        }
        if (style_ == Bellcap) {
            // Short dimple line on the cap surface
            const qreal inR = knobR * 0.15;
            const qreal outR = knobR * 0.70;
            QPointF p1(center.x() + inR * cs, center.y() - inR * sn);
            QPointF p2(center.x() + outR * cs, center.y() - outR * sn);
            p.setPen(QPen(QColor(255, 255, 255, 200), 3.0, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p1, p2);
            return;
        }
        if (style_ == SoftLED) {
            // LED dot at the tip
            const qreal rOut = knobR * 0.78;
            QPointF tip(center.x() + rOut * cs, center.y() - rOut * sn);
            QColor glow(accent_);
            glow.setAlpha(200);
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(tip, 3.5, 3.5);
            glow.setAlpha(70);
            p.setBrush(glow);
            p.drawEllipse(tip, 6.5, 6.5);
            return;
        }
        if (style_ == VintageBakelite) {
            // Ivory-thin pointer line
            const qreal inR = knobR * 0.25;
            const qreal outR = knobR * 0.85;
            QPointF p1(center.x() + inR * cs, center.y() - inR * sn);
            QPointF p2(center.x() + outR * cs, center.y() - outR * sn);
            p.setPen(QPen(QColor("#e8d8a8"), 2.2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p1, p2);
            return;
        }

        // Modern (default)
        const qreal innerR = knobR * 0.30;
        const qreal outerR = knobR * 0.82;
        QPointF p1(center.x() + innerR * cs, center.y() - innerR * sn);
        QPointF p2(center.x() + outerR * cs, center.y() - outerR * sn);
        QPen glowPen(QColor(accent_.red(), accent_.green(), accent_.blue(), 80),
                     6.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(glowPen);
        p.drawLine(p1, p2);
        QPen indPen(Qt::white, 3.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(indPen);
        p.drawLine(p1, p2);
    }

    float   value_        = 0.5f;
    float   default_      = 0.5f;
    QString title_;
    QString valueText_;
    int     notches_      = 11;
    QColor  accent_       = QColor("#00d4aa");
    Style   style_        = Modern;

    // Drag state
    bool    dragging_     = false;
    qreal   dragStartY_   = 0.0;
    float   dragStartVal_ = 0.0f;
};
