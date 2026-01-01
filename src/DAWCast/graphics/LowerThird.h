// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QColor>

class QPainter;

namespace dawcast {

class LowerThird : public QObject
{
    Q_OBJECT

public:
    explicit LowerThird(QObject *parent = nullptr);
    ~LowerThird() override;

    void setLine1(const QString &text);
    void setLine2(const QString &text);
    void setStyle(const QColor &bg, const QColor &text, int fontSize);
    void setDuration(double seconds);

    QString line1() const;
    QString line2() const;
    double duration() const;

    void render(QPainter &painter, int frameWidth, int frameHeight, double progress);

private:
    QString m_line1;
    QString m_line2;
    QColor m_bgColor{0, 0, 0, 200};
    QColor m_textColor{Qt::white};
    int m_fontSize{24};
    double m_duration{5.0};
};

} // namespace dawcast
