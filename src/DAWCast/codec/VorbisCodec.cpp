// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VorbisCodec.h"

#ifdef HAVE_VORBIS
// TODO: #include <vorbis/vorbisenc.h>
// TODO: #include <ogg/ogg.h>
#endif

namespace dawcast {

bool VorbisCodec::encode(const AudioBuffer &buffer, const QString &path, float quality)
{
#ifdef HAVE_VORBIS
    // TODO: Initialize vorbis_info, vorbis_encode_init_vbr() with quality
    // TODO: Set up ogg_stream_state for Ogg container
    // TODO: Feed float PCM via vorbis_analysis_buffer/wrote
    // TODO: Extract Ogg pages and write to file
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(quality)
    return false;
#else
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(quality)
    return false;
#endif
}

AudioBuffer VorbisCodec::decode(const QString &path)
{
#ifdef HAVE_VORBIS
    // TODO: Open Ogg/Vorbis file via ov_fopen()
    // TODO: Read decoded float PCM into AudioBuffer
    Q_UNUSED(path)
    return AudioBuffer{};
#else
    // TODO: Fall back to avcodec if available
    Q_UNUSED(path)
    return AudioBuffer{};
#endif
}

} // namespace dawcast
