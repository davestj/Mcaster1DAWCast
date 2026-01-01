// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SubtitleRenderer.h"
#include <QPainter>
#include <QFont>
#include <QFile>
#include <QTextStream>

namespace dawcast {

SubtitleRenderer::SubtitleRenderer(QObject* parent)
    : QObject(parent)
{
}

SubtitleRenderer::~SubtitleRenderer() = default;

bool SubtitleRenderer::loadSRT(const QString& path)
{
    // TODO: Parse SRT format
    // Format: index\n start --> end\n text\n\n
    (void)path;
    return false;
}

bool SubtitleRenderer::loadASS(const QString& path)
{
    // TODO: Parse ASS/SSA format
    (void)path;
    return false;
}

bool SubtitleRenderer::loadVTT(const QString& path)
{
    // TODO: Parse WebVTT format
    (void)path;
    return false;
}

void SubtitleRenderer::renderAt(QImage& frame, double timeSeconds)
{
    if (m_entries.isEmpty()) return;

    // Find active subtitle entries at the given time
    QString activeText;
    for (const auto& entry : m_entries) {
        if (timeSeconds >= entry.startTime && timeSeconds <= entry.endTime) {
            if (!activeText.isEmpty()) activeText += "\n";
            activeText += entry.text;
        }
    }

    if (activeText.isEmpty()) return;

    // TODO: Render subtitle text onto the frame
    QPainter painter(&frame);
    QFont font("Arial", 24, QFont::Bold);
    painter.setFont(font);
    painter.setPen(Qt::white);

    QRect textRect = frame.rect();
    textRect.setTop(frame.height() - 100);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, activeText);
    painter.end();
}

void SubtitleRenderer::clear()
{
    m_entries.clear();
}

} // namespace dawcast
