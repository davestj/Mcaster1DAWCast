// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QImage>

class QPainter;

namespace dawcast {

class Watermark : public QObject
{
    Q_OBJECT

public:
    explicit Watermark(QObject *parent = nullptr);
    ~Watermark() override;

    void setImage(const QString &path);
    void setPosition(Qt::Alignment alignment);
    void setOpacity(float opacity);
    void setScale(float scale);

    float opacity() const;
    float scale() const;

    void render(QPainter &painter, int frameWidth, int frameHeight);

    static QImage extractWatermark(const QImage &frame);

private:
    QImage m_image;
    Qt::Alignment m_alignment{Qt::AlignBottom | Qt::AlignRight};
    float m_opacity{0.3f};
    float m_scale{1.0f};
};

} // namespace dawcast
