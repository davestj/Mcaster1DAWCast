// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QPointF>
#include <QColor>
#include <QFont>

class QPainter;

namespace dawcast {

class Callout : public QObject
{
    Q_OBJECT

public:
    explicit Callout(QObject *parent = nullptr);
    ~Callout() override;

    void setPosition(const QPointF &pos);
    void setText(const QString &text);
    void setArrowTarget(const QPointF &target);
    void setStyle(const QColor &color, const QFont &font);

    QPointF position() const;
    QString text() const;
    QPointF arrowTarget() const;

    void render(QPainter &painter);

private:
    QPointF m_position;
    QString m_text;
    QPointF m_arrowTarget;
    QColor m_color{Qt::yellow};
    QFont m_font;
};

} // namespace dawcast
