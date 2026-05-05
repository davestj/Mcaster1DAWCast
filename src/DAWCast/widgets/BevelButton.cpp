// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BevelButton.h"
#include "ThemeEngine.h"

#include <QPainter>
#include <QLinearGradient>
#include <QEnterEvent>

namespace dawcast::widgets {

BevelButton::BevelButton(QWidget* parent)
    : QPushButton(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

BevelButton::BevelButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

BevelButton::BevelButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QPushButton(icon, text, parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

BevelButton::~BevelButton() = default;

void BevelButton::setBevelDepth(int pixels)      { m_bevelDepth = pixels; update(); }
void BevelButton::setHighlightColor(const QColor& c) { m_highlightColor = c; update(); }
void BevelButton::setShadowColor(const QColor& c)    { m_shadowColor = c; update(); }
void BevelButton::setFaceColor(const QColor& c)      { m_faceColor = c; update(); }
void BevelButton::setCheckedFaceColor(const QColor& c) { m_checkedFaceColor = c; update(); }

int    BevelButton::bevelDepth()       const { return m_bevelDepth; }
QColor BevelButton::highlightColor()   const { return m_highlightColor; }
QColor BevelButton::shadowColor()      const { return m_shadowColor; }
QColor BevelButton::faceColor()        const { return m_faceColor; }
QColor BevelButton::checkedFaceColor() const { return m_checkedFaceColor; }

QSize BevelButton::sizeHint() const
{
    QSize base = QPushButton::sizeHint();
    // Global 25% downsize: was 36x28 minimum, now 27x21.
    return QSize(qMax(base.width(), 27), qMax(base.height(), 21));
}

void BevelButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void BevelButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void BevelButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const int d   = m_bevelDepth;
    const bool pressed  = isDown();
    const bool checked  = isChecked();

    // ── Resolve face color ──────────────────────────────────────────────
    QColor face;
    if (checked && m_checkedFaceColor.isValid()) {
        face = m_checkedFaceColor;
    } else if (m_faceColor.isValid()) {
        face = m_faceColor;
    } else {
        // Try ThemeEngine first, fall back to palette
        QColor themeBtn = ThemeEngine::instance()->color(QStringLiteral("button_bg"));
        face = themeBtn.isValid() ? themeBtn : palette().button().color();
    }

    // Checked highlight tint when no explicit checked color is set
    if (checked && !m_checkedFaceColor.isValid()) {
        // Blend face color toward the highlight color
        int hr = m_highlightColor.red();
        int hg = m_highlightColor.green();
        int hb = m_highlightColor.blue();
        face = QColor(
            (face.red()   + hr) / 2,
            (face.green() + hg) / 2,
            (face.blue()  + hb) / 2);
    }

    // Hover: lighten the face slightly
    if (m_hovered && !pressed) {
        face = face.lighter(115);
    }

    // ── Face gradient ───────────────────────────────────────────────────
    QLinearGradient faceGrad;
    if (pressed) {
        faceGrad = QLinearGradient(0, r.bottom(), 0, r.top());
        faceGrad.setColorAt(0.0, face.lighter(108));
        faceGrad.setColorAt(1.0, face.darker(115));
    } else {
        faceGrad = QLinearGradient(0, r.top(), 0, r.bottom());
        faceGrad.setColorAt(0.0, face.lighter(112));
        faceGrad.setColorAt(0.45, face);
        faceGrad.setColorAt(1.0, face.darker(120));
    }

    // Outer border — 1px flat, 1px radius
    p.setPen(QPen(QColor(160, 160, 168), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 1, 1);

    // Face fill inside the border
    QRect inner = r.adjusted(1, 1, -1, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(faceGrad));
    p.drawRoundedRect(inner, 1, 1);

    // ── Bevel edges ─────────────────────────────────────────────────────
    QColor topLeftC     = pressed ? m_shadowColor : m_highlightColor;
    QColor bottomRightC = pressed ? m_highlightColor : m_shadowColor;

    // Top bevel
    for (int i = 0; i < d; ++i) {
        float alpha = topLeftC.alphaF() * (1.0f - static_cast<float>(i) / d);
        QColor c = topLeftC;
        c.setAlphaF(alpha);
        p.setPen(c);
        p.drawLine(inner.left() + i + 1, inner.top() + i,
                   inner.right() - i - 1, inner.top() + i);
    }

    // Left bevel
    for (int i = 0; i < d; ++i) {
        float alpha = topLeftC.alphaF() * (1.0f - static_cast<float>(i) / d);
        QColor c = topLeftC;
        c.setAlphaF(alpha);
        p.setPen(c);
        p.drawLine(inner.left() + i, inner.top() + i + 1,
                   inner.left() + i, inner.bottom() - i - 1);
    }

    // Bottom bevel
    for (int i = 0; i < d; ++i) {
        float alpha = bottomRightC.alphaF() * (1.0f - static_cast<float>(i) / d);
        QColor c = bottomRightC;
        c.setAlphaF(alpha);
        p.setPen(c);
        p.drawLine(inner.left() + i + 1, inner.bottom() - i,
                   inner.right() - i - 1, inner.bottom() - i);
    }

    // Right bevel
    for (int i = 0; i < d; ++i) {
        float alpha = bottomRightC.alphaF() * (1.0f - static_cast<float>(i) / d);
        QColor c = bottomRightC;
        c.setAlphaF(alpha);
        p.setPen(c);
        p.drawLine(inner.right() - i, inner.top() + i + 1,
                   inner.right() - i, inner.bottom() - i - 1);
    }

    // ── Content (icon + text) ───────────────────────────────────────────
    QRect contentRect = inner.adjusted(d, d, -d, -d);
    if (pressed) contentRect.translate(1, 1);

    // Draw icon if present
    if (!icon().isNull()) {
        const QIcon::Mode mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;

        if (text().isEmpty()) {
            // Auto-fit: shrink to a square that fits contentRect, centered.
            // QIcon::paint handles Retina DPR correctly and keeps aspect ratio.
            const int side = qMax(8, qMin(contentRect.width(), contentRect.height()));
            QRect iconRect(0, 0, side, side);
            iconRect.moveCenter(contentRect.center());
            icon().paint(&p, iconRect, Qt::AlignCenter, mode);
        } else {
            // Icon on the left, text on the right
            const int side = qMax(8, contentRect.height());
            QRect iconRect(contentRect.left(), contentRect.top(), side, contentRect.height());
            icon().paint(&p, iconRect, Qt::AlignLeft | Qt::AlignVCenter, mode);
            const int spacing = 4;
            contentRect.setLeft(contentRect.left() + side + spacing);
        }
    }

    // Draw text
    if (!text().isEmpty()) {
        p.setPen(palette().buttonText().color());
        QFont f = font();
        p.setFont(f);
        p.drawText(contentRect, Qt::AlignCenter, text());
    }
}

} // namespace dawcast::widgets
