// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "OpusCodec.h"

#ifdef HAVE_OPUS
// TODO: #include <opus/opus.h>
// TODO: #include <ogg/ogg.h>  // for Ogg container
#endif

namespace dawcast {

bool OpusCodec::encode(const AudioBuffer &buffer, const QString &path, int bitrate)
{
#ifdef HAVE_OPUS
    // TODO: Create OpusEncoder via opus_encoder_create()
    // TODO: Set bitrate via opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate * 1000))
    // TODO: Encode 20ms frames (960 samples at 48kHz)
    // TODO: Wrap in Ogg container and write to file
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(bitrate)
    return false;
#else
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(bitrate)
    return false;
#endif
}

AudioBuffer OpusCodec::decode(const QString &path)
{
#ifdef HAVE_OPUS
    // TODO: Open Ogg container, create OpusDecoder via opus_decoder_create()
    // TODO: Decode Opus packets to float PCM, populate AudioBuffer
    Q_UNUSED(path)
    return AudioBuffer{};
#else
    Q_UNUSED(path)
    return AudioBuffer{};
#endif
}

} // namespace dawcast
