// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QImage>
#include <QList>
#include <functional>

class QPainter;

namespace dawcast {

using OverlayRenderFunc = std::function<void(QPainter &, int width, int height, double timeSeconds)>;

class GraphicsRenderer : public QObject
{
    Q_OBJECT

public:
    explicit GraphicsRenderer(QObject *parent = nullptr);
    ~GraphicsRenderer() override;

    void renderFrame(QImage &frame, const QList<OverlayRenderFunc> &overlays, double timeSeconds);

private:
    // TODO: Add render state, caching, double-buffering
};

} // namespace dawcast
