// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include "../core/AudioBuffer.h"

namespace dawcast {

class AacCodec
{
public:
    AacCodec() = default;
    ~AacCodec() = default;

    bool encode(const AudioBuffer &buffer, const QString &path, int bitrate = 128);
    AudioBuffer decode(const QString &path);
};

} // namespace dawcast
