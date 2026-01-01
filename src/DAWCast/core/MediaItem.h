// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <cstdint>

namespace dawcast {

struct MediaItem
{
    QString path;
    QString title;
    int64_t durationSamples = 0;
    int     sampleRate      = 44100;
    int     channels        = 2;
    bool    hasVideo        = false;

    [[nodiscard]] double durationSeconds() const
    {
        if (sampleRate <= 0) return 0.0;
        return static_cast<double>(durationSamples) / sampleRate;
    }
};

} // namespace dawcast
