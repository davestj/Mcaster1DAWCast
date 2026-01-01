// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Mp3Codec.h"

#ifdef HAVE_LAME
// TODO: #include <lame/lame.h>
#endif

#ifdef HAVE_MPG123
// TODO: #include <mpg123.h>
#endif

namespace dawcast {

bool Mp3Codec::encode(const AudioBuffer &buffer, const QString &path, int bitrate)
{
#ifdef HAVE_LAME
    // TODO: Initialize LAME encoder
    // lame_t lame = lame_init();
    // lame_set_num_channels(lame, buffer.channels);
    // lame_set_in_samplerate(lame, buffer.sampleRate);
    // lame_set_brate(lame, bitrate);
    // lame_set_quality(lame, 2);  // 0=best, 9=fastest
    // lame_init_params(lame);
    // TODO: Encode float PCM to MP3 frames, write to file
    // lame_close(lame);
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

AudioBuffer Mp3Codec::decode(const QString &path)
{
#ifdef HAVE_MPG123
    // TODO: Initialize mpg123 decoder
    // mpg123_handle *mh = mpg123_new(nullptr, &err);
    // mpg123_open(mh, path.toUtf8().constData());
    // mpg123_getformat(mh, &rate, &channels, &encoding);
    // TODO: Read decoded PCM into AudioBuffer
    // mpg123_close(mh);
    // mpg123_delete(mh);
    Q_UNUSED(path)
    return AudioBuffer{};
#else
    // TODO: Fall back to avcodec if available
    Q_UNUSED(path)
    return AudioBuffer{};
#endif
}

} // namespace dawcast
