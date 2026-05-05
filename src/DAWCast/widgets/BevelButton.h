// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPushButton>
#include <QColor>
#include <QIcon>

namespace dawcast::widgets {

class BevelButton : public QPushButton {
    Q_OBJECT

    Q_PROPERTY(int bevelDepth READ bevelDepth WRITE setBevelDepth)
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)
    Q_PROPERTY(QColor faceColor READ faceColor WRITE setFaceColor)
    Q_PROPERTY(QColor checkedFaceColor READ checkedFaceColor WRITE setCheckedFaceColor)

public:
    explicit BevelButton(QWidget* parent = nullptr);
    explicit BevelButton(const QString& text, QWidget* parent = nullptr);
    explicit BevelButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);
    ~BevelButton() override;

    void setBevelDepth(int pixels);
    void setHighlightColor(const QColor& color);
    void setShadowColor(const QColor& color);
    void setFaceColor(const QColor& color);
    void setCheckedFaceColor(const QColor& color);

    int    bevelDepth()       const;
    QColor highlightColor()   const;
    QColor shadowColor()      const;
    QColor faceColor()        const;
    QColor checkedFaceColor() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int    m_bevelDepth       = 1;
    QColor m_highlightColor   = QColor(255, 255, 255, 120);
    QColor m_shadowColor      = QColor(0, 0, 0, 120);
    QColor m_faceColor;              // empty = use palette
    QColor m_checkedFaceColor;       // empty = use derived highlight
    bool   m_hovered          = false;
};

} // namespace dawcast::widgets
