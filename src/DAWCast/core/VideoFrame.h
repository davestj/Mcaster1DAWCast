// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QImage>
#include <cstdint>

namespace dawcast {

struct VideoFrame
{
    QImage  image;
    int64_t pts         = 0;
    int     width       = 0;
    int     height      = 0;
    double  timeSeconds = 0.0;

    // Compositing properties (used by VideoMixer)
    double  opacity     = 1.0;   // 0.0 = fully transparent, 1.0 = fully opaque
    int     posX        = 0;     // Horizontal offset in the output frame
    int     posY        = 0;     // Vertical offset in the output frame
    int     pipWidth    = 0;     // PIP target width (0 = full-frame scaling)
    int     pipHeight   = 0;     // PIP target height (0 = full-frame scaling)

    [[nodiscard]] bool isValid() const { return !image.isNull(); }
};

} // namespace dawcast
