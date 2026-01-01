// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace dawcast {

class AudioResampler
{
public:
    AudioResampler();
    ~AudioResampler();

    // Non-copyable
    AudioResampler(const AudioResampler&) = delete;
    AudioResampler& operator=(const AudioResampler&) = delete;

    /// Resample audio data between sample rates using libswresample.
    /// Returns number of output frames written, or -1 on error.
    int resample(const float* in, int inFrames, int inRate,
                 float* out, int outFrames, int outRate,
                 int channels);

private:
    void* m_swrCtx = nullptr;  // SwrContext*
};

} // namespace dawcast
