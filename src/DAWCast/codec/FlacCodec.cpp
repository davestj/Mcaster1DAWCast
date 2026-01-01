// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FlacCodec.h"

#ifdef HAVE_FLAC
// TODO: #include <FLAC/stream_encoder.h>
// TODO: #include <FLAC/stream_decoder.h>
#endif

namespace dawcast {

bool FlacCodec::encode(const AudioBuffer &buffer, const QString &path, int bitDepth)
{
#ifdef HAVE_FLAC
    // TODO: Initialize FLAC__StreamEncoder
    // TODO: Set channels, bits_per_sample, sample_rate, compression_level
    // TODO: Convert float samples to int32 and feed to encoder
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(bitDepth)
    return false;
#else
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(bitDepth)
    return false;
#endif
}

AudioBuffer FlacCodec::decode(const QString &path)
{
#ifdef HAVE_FLAC
    // TODO: Initialize FLAC__StreamDecoder
    // TODO: Decode to int32 samples, convert to float, populate AudioBuffer
    Q_UNUSED(path)
    return AudioBuffer{};
#else
    Q_UNUSED(path)
    return AudioBuffer{};
#endif
}

} // namespace dawcast
