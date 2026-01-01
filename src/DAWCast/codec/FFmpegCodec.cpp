// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FFmpegCodec.h"

#ifdef HAVE_AVFORMAT
extern "C" {
// TODO: #include <libavformat/avformat.h>
// TODO: #include <libavcodec/avcodec.h>
// TODO: #include <libswresample/swresample.h>
}
#endif

namespace dawcast {

AudioBuffer FFmpegCodec::decode(const QString &path)
{
#ifdef HAVE_AVFORMAT
    // TODO: Open input with avformat_open_input()
    // TODO: Find best audio stream with av_find_best_stream()
    // TODO: Open codec context with avcodec_open2()
    // TODO: Read packets with av_read_frame(), decode with avcodec_send_packet/receive_frame
    // TODO: Convert to float PCM with swr_convert() if needed
    // TODO: Populate AudioBuffer with decoded samples
    // TODO: Cleanup: avcodec_free_context, avformat_close_input
    Q_UNUSED(path)
    return AudioBuffer{};
#else
    Q_UNUSED(path)
    return AudioBuffer{};
#endif
}

bool FFmpegCodec::encode(const AudioBuffer &buffer, const QString &path,
                         const QString &codecName, int bitrate)
{
#ifdef HAVE_AVFORMAT
    // TODO: Find encoder with avcodec_find_encoder_by_name(codecName)
    // TODO: Create AVFormatContext for output with avformat_alloc_output_context2()
    // TODO: Add audio stream, configure codec context (sample_rate, channels, bit_rate)
    // TODO: Open output file with avio_open()
    // TODO: Write header with avformat_write_header()
    // TODO: Convert float PCM to encoder's sample format with swr_convert()
    // TODO: Encode frames with avcodec_send_frame/receive_packet
    // TODO: Write packets with av_interleaved_write_frame()
    // TODO: Write trailer, cleanup
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(codecName)
    Q_UNUSED(bitrate)
    return false;
#else
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(codecName)
    Q_UNUSED(bitrate)
    return false;
#endif
}

} // namespace dawcast
