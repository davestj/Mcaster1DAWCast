// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioResampler.h"

#include <cstring>
#include <algorithm>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}
#endif

namespace dawcast {

AudioResampler::AudioResampler()
{
#ifdef HAVE_AVFORMAT
    // SwrContext will be lazily allocated on first resample() call
    // because we need to know the actual sample rates and channel count.
    m_swrCtx = nullptr;
#endif
}

AudioResampler::~AudioResampler()
{
#ifdef HAVE_AVFORMAT
    if (m_swrCtx) {
        SwrContext *ctx = static_cast<SwrContext*>(m_swrCtx);
        swr_free(&ctx);
        m_swrCtx = nullptr;
    }
#endif
}

int AudioResampler::resample(const float* in, int inFrames, int inRate,
                             float* out, int outFrames, int outRate,
                             int channels)
{
    if (!in || !out || inFrames <= 0 || outFrames <= 0 || channels <= 0) {
        return -1;
    }

    // If rates match, just copy (no resampling needed)
    if (inRate == outRate) {
        int framesToCopy = std::min(inFrames, outFrames);
        std::memcpy(out, in, static_cast<size_t>(framesToCopy * channels) * sizeof(float));
        return framesToCopy;
    }

#ifdef HAVE_AVFORMAT
    SwrContext *ctx = static_cast<SwrContext*>(m_swrCtx);

    // (Re-)initialize the context if parameters have changed
    if (!ctx || m_lastInRate != inRate || m_lastOutRate != outRate || m_lastChannels != channels) {
        if (ctx) {
            swr_free(&ctx);
        }

        ctx = swr_alloc();
        if (!ctx) {
            return -1;
        }

        // Configure channel layout based on channel count
        AVChannelLayout chLayout;
        if (channels == 1) {
            chLayout = AV_CHANNEL_LAYOUT_MONO;
        } else {
            chLayout = AV_CHANNEL_LAYOUT_STEREO;
        }

        av_opt_set_chlayout(ctx, "in_chlayout",  &chLayout, 0);
        av_opt_set_chlayout(ctx, "out_chlayout", &chLayout, 0);
        av_opt_set_int(ctx, "in_sample_rate",  inRate,  0);
        av_opt_set_int(ctx, "out_sample_rate", outRate, 0);
        av_opt_set_sample_fmt(ctx, "in_sample_fmt",  AV_SAMPLE_FMT_FLT, 0);
        av_opt_set_sample_fmt(ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

        int ret = swr_init(ctx);
        if (ret < 0) {
            swr_free(&ctx);
            m_swrCtx = nullptr;
            return -1;
        }

        m_swrCtx = ctx;
        m_lastInRate   = inRate;
        m_lastOutRate  = outRate;
        m_lastChannels = channels;
    }

    // Perform the resampling
    const uint8_t *inBuf[1]  = { reinterpret_cast<const uint8_t*>(in) };
    uint8_t       *outBuf[1] = { reinterpret_cast<uint8_t*>(out) };

    int framesWritten = swr_convert(ctx, outBuf, outFrames, inBuf, inFrames);
    return framesWritten;

#else
    // Fallback: simple linear interpolation resampler
    const double ratio = static_cast<double>(inRate) / static_cast<double>(outRate);
    int framesWritten = 0;

    for (int i = 0; i < outFrames; ++i) {
        double srcPos = i * ratio;
        int srcIdx = static_cast<int>(srcPos);
        double frac = srcPos - srcIdx;

        if (srcIdx >= inFrames - 1) {
            break;
        }

        for (int ch = 0; ch < channels; ++ch) {
            float s0 = in[srcIdx * channels + ch];
            float s1 = in[(srcIdx + 1) * channels + ch];
            out[i * channels + ch] = static_cast<float>(s0 + (s1 - s0) * frac);
        }
        framesWritten++;
    }

    return framesWritten;
#endif
}

} // namespace dawcast
