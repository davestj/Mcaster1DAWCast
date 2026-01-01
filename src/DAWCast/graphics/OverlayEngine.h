// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QImage>
#include <QList>
#include <QString>
#include <functional>

class QPainter;

namespace dawcast {

using OverlayRenderFunc = std::function<void(QPainter &, int width, int height, double timeSeconds)>;

struct Overlay {
    int id{0};
    QString name;
    bool active{true};
    OverlayRenderFunc renderFunc;
};

class OverlayEngine : public QObject
{
    Q_OBJECT

public:
    explicit OverlayEngine(QObject *parent = nullptr);
    ~OverlayEngine() override;

    int addOverlay(const QString &name, OverlayRenderFunc renderFunc);
    void removeOverlay(int id);
    void setOverlayActive(int id, bool active);

    void renderOverlays(QImage &frame, double timeSeconds);

private:
    QList<Overlay> m_overlays;
    int m_nextId{1};
};

} // namespace dawcast
