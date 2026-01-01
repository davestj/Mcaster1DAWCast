// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dawcast {

struct AudioBuffer
{
    float* data       = nullptr;
    int    frames     = 0;
    int    channels   = 0;
    int    sampleRate = 44100;

    [[nodiscard]] int sampleCount() const { return frames * channels; }

    void silence()
    {
        if (data && sampleCount() > 0) {
            std::memset(data, 0, static_cast<size_t>(sampleCount()) * sizeof(float));
        }
    }

    void mix(const AudioBuffer& other)
    {
        if (!data || !other.data) return;
        int count = std::min(sampleCount(), other.sampleCount());
        for (int i = 0; i < count; ++i) {
            data[i] += other.data[i];
        }
    }

    [[nodiscard]] float peak() const
    {
        if (!data || sampleCount() == 0) return 0.0f;
        float maxVal = 0.0f;
        int count = sampleCount();
        for (int i = 0; i < count; ++i) {
            maxVal = std::max(maxVal, std::fabs(data[i]));
        }
        return maxVal;
    }
};

} // namespace dawcast
