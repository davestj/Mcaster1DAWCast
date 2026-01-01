// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QColor>

class QPainter;

namespace dawcast {

class TickerCrawl : public QObject
{
    Q_OBJECT

public:
    explicit TickerCrawl(QObject *parent = nullptr);
    ~TickerCrawl() override;

    void setText(const QString &text);
    void setSpeed(float pixelsPerSecond);
    void setStyle(const QColor &bg, const QColor &text, int fontSize);

    QString text() const;
    float speed() const;

    void render(QPainter &painter, int frameWidth, int frameHeight, double timeSeconds);

private:
    QString m_text;
    float m_speed{100.0f};
    QColor m_bgColor{0, 0, 0, 180};
    QColor m_textColor{Qt::white};
    int m_fontSize{18};
};

} // namespace dawcast
