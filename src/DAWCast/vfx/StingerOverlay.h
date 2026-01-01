// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

#include <QString>
#include <QList>
#include <QImage>

namespace dawcast {

// Animated overlay transition from a PNG sequence or video file.
// The stinger plays over the transition; the underlying cut from A to B
// happens at the trigger frame.

class StingerOverlay : public IVideoEffect
{
public:
    enum Param
    {
        SourcePath = 0,   // (set via setSourcePath)
        TriggerFrame,     // frame index where the cut happens (0-based)
        ParamCount
    };

    StingerOverlay();
    ~StingerOverlay() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;

    void setSourcePath(const QString& path);
    void setTriggerFrame(int frame) { m_triggerFrame = frame; }

    bool isLoaded() const { return !m_frames.isEmpty(); }
    int  frameCount() const { return m_frames.size(); }

private:
    void loadSequence();

    QString m_sourcePath;
    int     m_triggerFrame = 0;

    QList<QImage> m_frames; // pre-loaded PNG sequence
};

} // namespace dawcast
