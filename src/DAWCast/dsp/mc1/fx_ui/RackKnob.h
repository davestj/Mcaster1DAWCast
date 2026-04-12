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
    explicit RackKnob(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(80, 110);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(false);
    }

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

    QSize sizeHint() const override { return {80, 110}; }
    QSize minimumSizeHint() const override { return {80, 110}; }

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

        // ── 5. Knob body ──

        // Outer ring (dark edge for depth)
        {
            QPainterPath ring;
            ring.addEllipse(center, knobR + 1.0, knobR + 1.0);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#3a3a3a"));
            p.drawPath(ring);
        }

        // Main metallic body (radial gradient: bright center, dark edge)
        {
            QRadialGradient grad(center, knobR, center + QPointF(-knobR * 0.3, -knobR * 0.3));
            grad.setColorAt(0.0, QColor("#d8d8d8"));   // bright highlight
            grad.setColorAt(0.35, QColor("#c0c0c0"));  // mid chrome
            grad.setColorAt(0.7, QColor("#909090"));    // darker ring
            grad.setColorAt(1.0, QColor("#606060"));    // edge shadow

            p.setBrush(grad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(center, knobR, knobR);
        }

        // Subtle conical gradient overlay for brushed-metal look
        {
            QConicalGradient cg(center, 135);
            cg.setColorAt(0.0,  QColor(255, 255, 255, 18));
            cg.setColorAt(0.25, QColor(0, 0, 0, 12));
            cg.setColorAt(0.5,  QColor(255, 255, 255, 22));
            cg.setColorAt(0.75, QColor(0, 0, 0, 10));
            cg.setColorAt(1.0,  QColor(255, 255, 255, 18));

            p.setBrush(cg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(center, knobR - 1, knobR - 1);
        }

        // Inner concave dimple
        {
            const qreal dimpleR = knobR * 0.25;
            QRadialGradient dg(center, dimpleR, center + QPointF(dimpleR * 0.2, dimpleR * 0.2));
            dg.setColorAt(0.0, QColor("#505050"));
            dg.setColorAt(1.0, QColor("#808080"));
            p.setBrush(dg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(center, dimpleR, dimpleR);
        }

        // ── 6. Position indicator line ──
        {
            qreal angle = degreesToRadians(225.0 - 270.0 * value_);
            qreal innerR = knobR * 0.30;
            qreal outerR = knobR * 0.82;
            QPointF p1(center.x() + innerR * std::cos(angle),
                       center.y() - innerR * std::sin(angle));
            QPointF p2(center.x() + outerR * std::cos(angle),
                       center.y() - outerR * std::sin(angle));

            // Glow behind indicator
            QPen glowPen(QColor(accent_.red(), accent_.green(), accent_.blue(), 80),
                         6.0, Qt::SolidLine, Qt::RoundCap);
            p.setPen(glowPen);
            p.drawLine(p1, p2);

            // Bright indicator line
            QPen indPen(Qt::white, 3.0, Qt::SolidLine, Qt::RoundCap);
            p.setPen(indPen);
            p.drawLine(p1, p2);
        }

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

    float   value_        = 0.5f;
    float   default_      = 0.5f;
    QString title_;
    QString valueText_;
    int     notches_      = 11;
    QColor  accent_       = QColor("#00d4aa");

    // Drag state
    bool    dragging_     = false;
    qreal   dragStartY_   = 0.0;
    float   dragStartVal_ = 0.0f;
};
