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

    [[nodiscard]] bool isValid() const { return !image.isNull(); }
};

} // namespace dawcast
