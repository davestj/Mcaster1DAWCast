// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include "../core/AudioBuffer.h"

namespace dawcast {

class FlacCodec
{
public:
    FlacCodec() = default;
    ~FlacCodec() = default;

    bool encode(const AudioBuffer &buffer, const QString &path, int bitDepth = 16);
    AudioBuffer decode(const QString &path);
};

} // namespace dawcast
