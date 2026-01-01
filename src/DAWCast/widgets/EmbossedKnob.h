// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QString>
#include <QColor>

namespace dawcast::widgets {

class EmbossedKnob : public QWidget {
    Q_OBJECT

    Q_PROPERTY(float value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel)

public:
    explicit EmbossedKnob(QWidget* parent = nullptr);
    ~EmbossedKnob() override;

    void  setValue(float val);
    float value() const;

    void setRange(float min, float max);
    void setLabel(const QString& label);
    QString label() const;

    void setSuffix(const QString& suffix);
    void setDecimals(int decimals);
    void setArcColor(const QColor& color);
    void setKnobSize(int diameter);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void valueChanged(float value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    float normalizedValue() const;

    float   m_value      = 0.0f;
    float   m_min        = 0.0f;
    float   m_max        = 1.0f;
    QString m_label;
    QString m_suffix;
    int     m_decimals   = 2;
    QColor  m_arcColor   = QColor(80, 180, 255);
    int     m_knobDiam   = 48;

    bool    m_dragging   = false;
    int     m_lastY      = 0;

    static constexpr float kArcSpanDeg  = 270.0f;
    static constexpr float kArcStartDeg = 225.0f; // 7 o'clock
};

} // namespace dawcast::widgets
