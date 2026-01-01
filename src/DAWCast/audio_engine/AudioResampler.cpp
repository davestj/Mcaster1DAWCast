// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioResampler.h"
// TODO: #include <libswresample/swresample.h>

namespace dawcast {

AudioResampler::AudioResampler()
{
    // TODO: Initialize SwrContext
}

AudioResampler::~AudioResampler()
{
    // TODO: swr_free(&m_swrCtx)
}

int AudioResampler::resample(const float* in, int inFrames, int inRate,
                             float* out, int outFrames, int outRate,
                             int channels)
{
    // TODO: Configure SwrContext if sample rates changed
    // TODO: swr_convert() to perform resampling
    // For now, return 0 frames written
    (void)in; (void)inFrames; (void)inRate;
    (void)out; (void)outFrames; (void)outRate;
    (void)channels;
    return 0;
}

} // namespace dawcast
