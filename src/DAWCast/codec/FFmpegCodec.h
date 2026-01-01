// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include "../core/AudioBuffer.h"

namespace dawcast {

class FFmpegCodec
{
public:
    FFmpegCodec() = default;
    ~FFmpegCodec() = default;

    AudioBuffer decode(const QString &path);
    bool encode(const AudioBuffer &buffer, const QString &path,
                const QString &codecName, int bitrate = 192);
};

} // namespace dawcast
