// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "StingerOverlay.h"

#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <algorithm>

namespace dawcast {

StingerOverlay::StingerOverlay() = default;
StingerOverlay::~StingerOverlay() = default;

QImage StingerOverlay::process(QImage& frameA, QImage& frameB, float progress)
{
    if (m_frames.isEmpty()) {
        // No stinger loaded — just cut at midpoint
        return (progress < 0.5f) ? frameA : frameB;
    }

    int totalFrames = m_frames.size();
    int currentFrame = static_cast<int>(progress * static_cast<float>(totalFrames - 1));
    currentFrame = std::clamp(currentFrame, 0, totalFrames - 1);

    // Determine base frame: A before trigger, B after trigger
    const QImage& base = (currentFrame < m_triggerFrame) ? frameA : frameB;

    // Composite stinger overlay on top of base
    QImage result = base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QImage overlay = m_frames.at(currentFrame);
    if (overlay.size() != result.size()) {
        overlay = overlay.scaled(result.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    painter.drawImage(0, 0, overlay);
    painter.end();

    return result;
}

QString StingerOverlay::name() const
{
    return QStringLiteral("Stinger Overlay");
}

int StingerOverlay::parameterCount() const
{
    return ParamCount;
}

void StingerOverlay::setSourcePath(const QString& path)
{
    m_sourcePath = path;
    loadSequence();
}

void StingerOverlay::loadSequence()
{
    m_frames.clear();

    QFileInfo info(m_sourcePath);
    if (!info.exists()) return;

    if (info.isDir()) {
        // Load PNG sequence from directory (sorted alphabetically)
        QDir dir(m_sourcePath);
        QStringList filters;
        filters << "*.png" << "*.PNG";
        QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

        for (const QString& file : files) {
            QImage img(dir.filePath(file));
            if (!img.isNull()) {
                m_frames.append(img.convertToFormat(QImage::Format_ARGB32_Premultiplied));
            }
        }
    } else {
        // TODO: Support loading frames from a video file (e.g. WebM, MOV with alpha).
        //       Would require FFmpeg / Qt Multimedia integration.
        //       For now, only PNG sequences are supported.
    }

    // Default trigger frame to middle if not explicitly set
    if (m_triggerFrame <= 0 && !m_frames.isEmpty()) {
        m_triggerFrame = m_frames.size() / 2;
    }
}

} // namespace dawcast
