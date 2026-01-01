// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AacCodec.h"

#ifdef HAVE_FDKAAC
// TODO: #include <fdk-aac/aacenc_lib.h>
// TODO: #include <fdk-aac/aacdecoder_lib.h>
#endif

namespace dawcast {

bool AacCodec::encode(const AudioBuffer &buffer, const QString &path, int bitrate)
{
#ifdef HAVE_FDKAAC
    // TODO: Initialize FDK-AAC encoder
    // HANDLE_AACENCODER encoder;
    // aacEncOpen(&encoder, 0, buffer.channels);
    // aacEncoder_SetParam(encoder, AACENC_BITRATE, bitrate * 1000);
    // aacEncoder_SetParam(encoder, AACENC_SAMPLERATE, buffer.sampleRate);
    // aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, MODE_2);
    // aacEncEncode(encoder, ...);
    // TODO: Write encoded AAC frames to M4A/ADTS container
    // aacEncClose(&encoder);
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

AudioBuffer AacCodec::decode(const QString &path)
{
#ifdef HAVE_FDKAAC
    // TODO: Initialize FDK-AAC decoder or fall back to avcodec
    Q_UNUSED(path)
    return AudioBuffer{};
#else
    // TODO: Fall back to avcodec if available
    Q_UNUSED(path)
    return AudioBuffer{};
#endif
}

} // namespace dawcast
