// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "OverlayEngine.h"

#include <QPainter>

namespace dawcast {

OverlayEngine::OverlayEngine(QObject *parent)
    : QObject(parent)
{
}

OverlayEngine::~OverlayEngine() = default;

int OverlayEngine::addOverlay(const QString &name, OverlayRenderFunc renderFunc)
{
    Overlay overlay;
    overlay.id = m_nextId++;
    overlay.name = name;
    overlay.renderFunc = std::move(renderFunc);
    m_overlays.append(overlay);
    return overlay.id;
}

void OverlayEngine::removeOverlay(int id)
{
    for (int i = 0; i < m_overlays.size(); ++i) {
        if (m_overlays[i].id == id) {
            m_overlays.removeAt(i);
            return;
        }
    }
}

void OverlayEngine::setOverlayActive(int id, bool active)
{
    for (auto &overlay : m_overlays) {
        if (overlay.id == id) {
            overlay.active = active;
            return;
        }
    }
}

void OverlayEngine::renderOverlays(QImage &frame, double timeSeconds)
{
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    for (const auto &overlay : m_overlays) {
        if (overlay.active && overlay.renderFunc) {
            overlay.renderFunc(painter, frame.width(), frame.height(), timeSeconds);
        }
    }

    painter.end();
}

} // namespace dawcast
